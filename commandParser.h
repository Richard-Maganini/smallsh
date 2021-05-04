#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

// LL node struct for holding a list of command arguments
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


char* getCommand(void);
struct command* parseCommand(char* command);
void viewCommand(struct command* command);
void cleanupCommand(struct command* currentCommand);
char* expandDollars(char* commandString);
