#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#define MAX_CHARS 2049

struct arg {
	char* name;
	struct arg* next;
};
struct command {
	char* name;
	struct arg* args;
	int* foreground;
	char* inputFile;
	char* outputFile;
};

char* getCommand(void) {
	char input[MAX_CHARS];
	char* command;
	printf(": ");
	fflush(stdout);
	fgets(input, sizeof(input), stdin);
	if (strlen(input) > MAX_CHARS) {
		printf("Invalid command -- exceeded 2048 characters");
		fflush(stdout);
		command = "";
	}
	else {
		command = malloc(sizeof(input));
		strcpy(command, input);
	}
	return command;
}

char* expandDollars(char* token) {
	int expansions = 0;
	int token_length = strlen(token);
	int i = 0;
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
	if (expansions > 0) {
		int pid = getpid();
		char pid_str[20];
		memset(pid_str, '\0', 20);
		sprintf(pid_str, "%d", pid);
		int pid_digits = strlen(pid_str);
		int new_length = token_length + 1 + (expansions * pid_digits);
		char newToken[new_length];
		memset(newToken, '\0', new_length);

		i = 0;
		int j = 0;
		int k;
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

struct command* parseCommand(char* commandString) {
	struct command* currentCommand = malloc(sizeof(struct command));
	currentCommand->foreground = malloc(sizeof(int));
	*currentCommand->foreground = 1;
	currentCommand->name = NULL;
	currentCommand->args = NULL;
	currentCommand->inputFile = NULL;
	currentCommand->outputFile = NULL;
	char* save;
	commandString = expandDollars(commandString);
	int lastCharIdx = strlen(commandString) - 1;
	char stringHolder[lastCharIdx + 5];
	commandString[lastCharIdx] = '\0';
	strcpy(stringHolder, commandString);
	char* token = strtok_r(stringHolder, " ", &save);
	int len;
	int stopArgs = 0;
	struct arg* prev;
	while (token != NULL) {
		if (currentCommand->name == NULL) {         // first word will be command
			len = strlen(token) + 1;
			currentCommand->name = malloc(len);
			strncpy(currentCommand->name, token, len);
		}
		else if (!(strcmp(token, ">"))) {           // next word will be filename for output redirect
			stopArgs = 1;
			token = strtok_r(NULL, " ", &save);
			if (token == NULL) {
				*currentCommand->foreground = 2;
				break;
			}
			len = strlen(token) + 1;
			currentCommand->outputFile = malloc(len);
			strncpy(currentCommand->outputFile, token, len);
		}
		else if (!(strcmp(token, "<"))) {           // next word will be filename for input redirect
			stopArgs = 1;
			token = strtok_r(NULL, " ", &save);
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
			if (token == NULL) {
				*currentCommand->foreground = 0;
				break;
			}
			else {
				if (stopArgs) {
					*currentCommand->foreground = 2;
				}
				if (currentCommand->args == NULL) {
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
			continue;
		}
		else {										// if none of the above the token is an arg
			if (stopArgs) {
				*currentCommand->foreground = 2;
			}
			if (currentCommand->args == NULL) {
				currentCommand->args = malloc(sizeof(struct arg));
				prev = currentCommand->args;
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
