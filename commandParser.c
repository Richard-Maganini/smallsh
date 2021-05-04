#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#define MAX_CHARS 2049

// struct for holding information from parsed input commands 
struct arg {
	char* name;
	struct arg* next;
};
// struct for holding information from parsed input commands 
struct command {
	char* name;
	struct arg* args;
	int* foreground;
	char* inputFile;
	char* outputFile;
};

// prints the prompt, reads in and validates a string, then copies it to the heap if valid. Returns heap pointer if valid else NULL.
char* getCommand(void) {
	char input[MAX_CHARS];
	char* command;
	printf(": ");
	fflush(stdout);
	fgets(input, sizeof(input), stdin);
	if (strlen(input) > MAX_CHARS) {
		printf("Invalid command -- exceeded 2048 characters");
		fflush(stdout);
		command = NULL;
	}
	else {
		command = malloc(sizeof(input));
		strcpy(command, input);
	}
	return command;
}

// performs expansion of $$ characters in the command string (passed in as token) and returns the expansion
char* expandDollars(char* token) {
	int expansions = 0;
	int token_length = strlen(token);
	int i = 0;
	// count the number of expansions that need to be performed
	while (i < token_length) {
		if (token[i] == '$') {
			if (i != token_length - 1) {
				if (token[i + 1] == '$') {
					expansions += 1;
					i++;
				}
			}
		}
		i++;
	}
	if (expansions > 0) { // if we need to do expansion
		int pid = getpid();  // get pid we will replace $$ with
		char pid_str[20];
		memset(pid_str, '\0', 20);   // zero buffer that will hold stringified pid so we get correct number of pid digits below
		sprintf(pid_str, "%d", pid);  // fill pid_str buffer with stringifed pid
		int pid_digits = strlen(pid_str);  // get number of digits in pid
		int new_length = token_length + 1 + (expansions * pid_digits);   // calculate number of characters needed to expand command string 
		char newToken[new_length];  // initialize newToken buffer, will hold the expanded command string
		memset(newToken, '\0', new_length);   // zero newToken buffer so the buffer will always have a null terminator in the right spot

		i = 0;
		int j = 0;
		int k;
		// iterate through command string and copy each character to newToken, but replacing $$ with pid_str
		while (i < token_length) {
			if (token[i] == '$' && i != token_length - 1 && token[i + 1] == '$') {
				i++;
				k = 0;
				while (k < pid_digits) {
					newToken[j] = pid_str[k];
					k++;
					j++;
				}
			}
			else {
				newToken[j] = token[i];
				j++;
			}
			i++;
		}
		token = newToken;
	}
	return token;
}


// parses the input command string and fills a command structs members with information in the command string.
// returns filled command struct
struct command* parseCommand(char* commandString) {
	// initialize command struct to be filled
	struct command* currentCommand = malloc(sizeof(struct command));
	currentCommand->foreground = malloc(sizeof(int));
	*currentCommand->foreground = 1;
	currentCommand->name = NULL;
	currentCommand->args = NULL;
	currentCommand->inputFile = NULL;
	currentCommand->outputFile = NULL;
	char* save;	// for repeated strtok_r calls
	commandString = expandDollars(commandString);    // expand input command string
	int lastCharIdx = strlen(commandString) - 1;
	char stringHolder[lastCharIdx + 5];
	commandString[lastCharIdx] = '\0';
	strcpy(stringHolder, commandString);			// copy into stringholder so strtok_r works
	char* token = strtok_r(stringHolder, " ", &save);  // start tokenizing expanded command string with space delimiter and interpreting the words
	int len;	// holds length of specific tokens for mallocing command struct members
	int stopArgs = 0;			// set to 1 when < or > is encountered (i.e., if we shouldnt be getting anymore arguments)
	struct arg* prev;			// remembers previous argument inserted into argument LL
	while (token != NULL) {
		if (currentCommand->name == NULL) {         // first word will be command
			len = strlen(token) + 1;
			currentCommand->name = malloc(len);
			strncpy(currentCommand->name, token, len);
		}
		else if (!(strcmp(token, ">"))) {           // next word will be filename for output redirect
			stopArgs = 1;							// should get no more arguments after this
			token = strtok_r(NULL, " ", &save);     // if command string ends with a > or < the command has invalid syntax, so flag it by setting foreground = 2 and return
			if (token == NULL) {
				*currentCommand->foreground = 2;
				break;
			}
			len = strlen(token) + 1;
			currentCommand->outputFile = malloc(len);
			strncpy(currentCommand->outputFile, token, len);
		}
		else if (!(strcmp(token, "<"))) {           // next word will be filename for input redirect
			stopArgs = 1;							// should get no more arguments after this
			token = strtok_r(NULL, " ", &save);     // if command string ends with a > or < the command has invalid syntax, so flag it by setting foreground = 2 and return
			if (token == NULL) {
				*currentCommand->foreground = 2;
				break;
			}
			len = strlen(token) + 1;
			currentCommand->inputFile = malloc(len);
			strncpy(currentCommand->inputFile, token, len);
		}
		else if (!(strcmp(token, "&"))) {           // should be last token
			char* ampersand = token;
			token = strtok_r(NULL, " ", &save);
			if (token == NULL) {					// if the ampersand was the last character,
				*currentCommand->foreground = 0;	// current command should be run in the background so indicate in its foreground member and break
				break;
			}
			else {
				if (stopArgs) {					// if we shouldnt have gotten another character the & because > or < was encountered, flag invalid syntax
					*currentCommand->foreground = 2;
				}
				if (currentCommand->args == NULL) {  // we strtok_r'd the word after & in command string ~10 lines above to check if & was the last word in the command string,
													 // and since we know this isnt the first word and we havent seen < or > yet, the current token must be argument.
													 // so add it to the args LL
					currentCommand->args = malloc(sizeof(struct arg));
					prev = currentCommand->args;
				}
				else {
					prev->next = malloc(sizeof(struct arg));
					prev = prev->next;
				}
				len = strlen(ampersand) + 1;
				prev->name = malloc(len);
				strncpy(prev->name, ampersand, len);
				prev->next = NULL;
			}
			continue; // token already holds the next word in the command string after & at this point so restart the while loop 
		}
		else {										// if none of the above the token is an arg
			if (stopArgs) {							// flag argument coming after < or >
				*currentCommand->foreground = 2;
			}
			if (currentCommand->args == NULL) {    // initialize the args LL if it is empty and put the first arg in it
				currentCommand->args = malloc(sizeof(struct arg));
				prev = currentCommand->args;  // point prev to the most recently inserted arg in the LL
			}
			else {
				prev->next = malloc(sizeof(struct arg));
				prev = prev->next;
			}
			len = strlen(token) + 1;
			prev->name = malloc(len);
			strncpy(prev->name, token, len);
			prev->next = NULL;
		}
		token = strtok_r(NULL, " ", &save);
	}
	return currentCommand;
}


void viewCommand(struct command* command) {
	// included for debugging purposes. prints out information in a filled command struct parsed with above function
	printf("command name: %s\n", command->name);
	fflush(stdout);
	printf("foreground(1) or background(0): %d\n", *command->foreground);
	fflush(stdout);
	printf("inputFile: %s\n", command->inputFile);
	fflush(stdout);
	printf("outputFile: %s\n", command->outputFile);
	fflush(stdout);
	printf("args: \n");
	fflush(stdout);
	struct arg* currentArg = command->args;
	while (currentArg != NULL) {
		printf(currentArg->name);
		printf("\n");
		fflush(stdout);
		currentArg = currentArg->next;
	}
	return;
}

void cleanupCommand(struct command* currentCommand) {
	// frees a filled command struct and all of its members
	free(currentCommand->name);
	free(currentCommand->inputFile);
	free(currentCommand->outputFile);
	free(currentCommand->foreground);
	struct arg* currentArg = currentCommand->args;
	struct arg* temp;
	while (currentArg != NULL) {
		temp = currentArg;
		currentArg = currentArg->next;
		free(temp);
	}
	free(currentCommand);
}
