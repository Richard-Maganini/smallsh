# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <signal.h>
# include <string.h>

struct command {
	char* name;
	struct arg* args;
	int* foreground;
	char* inputFile;
	char* outputFile;
};

struct arg {
	char* name;
	struct arg* next;
};

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
