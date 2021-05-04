#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

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


char* getCommand(void);
struct command* parseCommand(char* command);
void viewCommand(struct command* command);
void cleanupCommand(struct command* currentCommand);
char* expandDollars(char* commandString);
