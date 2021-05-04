# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <signal.h>
# include <string.h>
# include "dll.h"

// struct for holding information from parsed input commands 
struct command {
	char* name;
	struct arg* args;
	int* foreground;
	char* inputFile;
	char* outputFile;
};

// LL node struct for holding a list of command arguments
struct arg {
	char* name;
	struct arg* next;
};

// uses chdir() to execute built in cd command
void bcd(struct command* currentCommand) {
	if (currentCommand->args == NULL) {
		char* home = getenv("HOME");
		chdir(home);
	}
	else {
		int success = chdir(currentCommand->args->name);
		if (success == -1) {
			printf("Invalid pathname\n");
			fflush(stdout);
		}
	}
}

// if currentCommand is a built in, it is "executed" by this function and returns 1. else returns 0
int handleBuiltIns(struct command* currentCommand, struct dllNode* dllHead, int* exitStatus, int* signalNum, int* childProcessExecuted) {
	int builtIn = 0;
	struct dllNode* current;
	if (!(strcmp(currentCommand->name, "exit"))) { // if the command was exit kill all smallsh children and exit(0)
		current = dllHead;
		while (current != NULL && current->pid != NULL) {
			kill(*current->pid, 9);
			current = current->next;
		}
		exit(0);
	}
	if (!(strcmp(currentCommand->name, "cd"))) {   // call bcd to change directory if command was cd
		bcd(currentCommand);
		builtIn = 1;
	}
	if (!(strcmp(currentCommand->name, "status"))) {  // if command was status
		if (*childProcessExecuted == 0) {   // if no foreground children have executed, print exit value 0
			printf("exit value 0\n");
			fflush(stdout);
		}
		else if (*exitStatus != 44) {       // else if most recent finished foreground child exited, print its exit value
			printf("exit value %d\n", *exitStatus);
			fflush(stdout);
		}
		else {     // else print termination number of most recent finished foreground child
			printf("terminated by signal %d\n", *signalNum);
			fflush(stdout);
		}
		builtIn = 1;
	}
	return builtIn;
}
