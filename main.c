#include "commandParser.h"
#include "builtIns.h"
#include "dll.h"
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>


int fgOnly = 0;
int termStopped = 0;


void handleTSTP(int sigNum) {
	if (fgOnly == 0) {
		fgOnly = 1;
		termStopped = 1;
	}
	else {
		fgOnly = 0;
		termStopped = 1;
	}
}

int main(void) {
	pid_t child;
	int builtIn = 0;
	int childStatus;
	int bChildStatus;
	int exitStatus = 44;
	int signalNum = 44;
	int childProcessExecuted = 0;
	int bExitStatus;
	int bSignalNum;
	int bTerminated;
	int jumpDown = 0;

	struct dllNode* dllHead = malloc(sizeof(struct dllNode));
	struct dllNode* dllTail;
	struct dllNode* current;
	dllHead->pid = NULL;
	dllHead->prev = NULL;
	dllHead->next = NULL;

	struct sigaction ignoreSignal = { 0 };
	ignoreSignal.sa_handler = SIG_IGN;
	sigfillset(&ignoreSignal.sa_mask);
	ignoreSignal.sa_flags = 0;
	sigaction(2, &ignoreSignal, NULL);

	struct sigaction dontIgnoreSignal = { 0 };
	dontIgnoreSignal.sa_handler = SIG_DFL;
	sigfillset(&dontIgnoreSignal.sa_mask);
	dontIgnoreSignal.sa_flags = 0;

	struct sigaction toggleFgOnly = { 0 };
	toggleFgOnly.sa_handler = handleTSTP;
	sigfillset(&toggleFgOnly.sa_mask);
	toggleFgOnly.sa_flags = 0;
	sigaction(SIGTSTP, &toggleFgOnly, NULL);

	while (1) {
		char* input = getCommand();
		if (input[0] == '\n' || input[0] == '#' || termStopped == 1) {
			jumpDown = 1;
		}
		if (jumpDown == 0) {
			struct command* currentCommand;
			currentCommand = parseCommand(input);
			//viewCommand(currentCommand);

			if (*currentCommand->foreground == 2) {
				printf("Invalid command syntax. Try again");
				fflush(stdout);
			}
			if (!(strcmp(currentCommand->name, "exit"))) {
				builtIn = 1;
				current = dllHead;
				while (current != NULL && current->pid != NULL) {
					kill(*current->pid, 9);
					current = current->next;
				}
				exit(0);
			}
			if (!(strcmp(currentCommand->name, "cd"))) {
				builtIn = 1;
				bcd(currentCommand);
			}
			if (!(strcmp(currentCommand->name, "status"))) {
				builtIn = 1;
				if (childProcessExecuted == 0) {
					printf("exit value 0\n");
					fflush(stdout);
				}
				else if (exitStatus != 44) {
					printf("exit value %d\n", exitStatus);
					fflush(stdout);
				}
				else {
					printf("terminated by signal %d\n", signalNum);
					fflush(stdout);
				}
			}
			if (builtIn == 0) {
				if (*currentCommand->foreground == 1 || fgOnly == 1) {
					child = fork();
					if (child == -1) {
						printf("fork failed\n");
						fflush(stdout);
					}
					else if (child == 0) {   // child process branch
						struct arg* current = currentCommand->args;
						int numArgs = 0;
						while (current != NULL) {
							numArgs++;
							current = current->next;
						}
						current = currentCommand->args;
						char* argv[numArgs + 2];
						argv[0] = currentCommand->name;
						argv[numArgs + 1] = NULL;
						int i = 1;
						while (i < numArgs + 1) {
							argv[i] = current->name;
							current = current->next;
							i++;
						}
						if (currentCommand->inputFile != NULL) {
							int inputFileFD = open(currentCommand->inputFile, O_RDONLY);
							if (inputFileFD == -1) {
								printf("cannot open %s for input\n", currentCommand->inputFile);
								fflush(stdout);
								exit(1);
							}
							dup2(inputFileFD, 0);
							close(inputFileFD);
						}
						if (currentCommand->outputFile != NULL) {
							int outputFileFD = open(currentCommand->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
							if (outputFileFD == -1) {
								printf("cannot open %s for output\n", currentCommand->outputFile);
								fflush(stdout);
								exit(1);
							}
							dup2(outputFileFD, 1);
							close(outputFileFD);
						}

						sigaction(2, &dontIgnoreSignal, NULL);
						sigaction(SIGTSTP, &ignoreSignal, NULL);
						execvp(currentCommand->name, argv);
						printf("%s: no such file or directory\n", currentCommand->name);
						fflush(stdout);
						exit(1);
					}
					else { // parent process branch
						int childDone = waitpid(child, &childStatus, 0);
						while (childDone == -1) {
							childDone = waitpid(child, &childStatus, 0);
						}
						childProcessExecuted = 1;
						if (WIFEXITED(childStatus)) {
							exitStatus = WEXITSTATUS(childStatus);
							signalNum = 44;
						}
						else {
							signalNum = WTERMSIG(childStatus);
							printf("terminated by signal %d\n", signalNum);
							exitStatus = 44;
						}
					}
				}
				else { // background command
					child = fork();
					if (child == -1) {
						printf("fork failed\n");
						fflush(stdout);
					}
					else if (child == 0) {   // child process branch
						sigaction(2, &ignoreSignal, NULL);
						sigaction(SIGTSTP, &ignoreSignal, NULL);
						if (currentCommand->inputFile == NULL) {
							int theVoid = open("/dev/null", O_WRONLY);
							if (theVoid == -1) {
								printf("cannot open devnull for input\n");
								fflush(stdout);
								exit(1);
							}
							dup2(theVoid, 0);
							close(theVoid);
						}
						else {
							int inputFileFD = open(currentCommand->inputFile, O_RDONLY);
							if (inputFileFD == -1) {
								printf("cannot open %s for input\n", currentCommand->inputFile);
								fflush(stdout);
								exit(1);
							}
							dup2(inputFileFD, 0);
							close(inputFileFD);
						}
						if (currentCommand->outputFile == NULL) {
							int theVoid = open("/dev/null", O_WRONLY);
							if (theVoid == -1) {
								printf("cannot open devnull for input for output\n");
								fflush(stdout);
								exit(1);
							}
							dup2(theVoid, 1);
							close(theVoid);
						}
						else {
							int outputFileFD = open(currentCommand->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
							if (outputFileFD == -1) {
								printf("cannot open %s for output\n", currentCommand->outputFile);
								fflush(stdout);
								exit(1);
							}
							dup2(outputFileFD, 1);
							close(outputFileFD);
						}

						struct arg* current = currentCommand->args;
						int numArgs = 0;
						while (current != NULL) {
							numArgs++;
							current = current->next;
						}
						current = currentCommand->args;
						char* argv[numArgs + 2];
						argv[0] = currentCommand->name;
						argv[numArgs + 1] = NULL;
						int i = 1;
						while (i < numArgs + 1) {
							argv[i] = current->name;
							current = current->next;
							i++;
						}
						execvp(currentCommand->name, argv);
						// printf("background %s failed\n", currentCommand->name);
						//(stdout);
						exit(1);
					}
					else { // parent process branch
						printf("background pid is %d\n", child);
						fflush(stdout);
						if (dllHead->pid == NULL) {
							dllHead->pid = malloc(sizeof(int));
							*dllHead->pid = child;
							dllTail = dllHead;
						}
						else {
							struct dllNode* temp = dllTail;
							dllTail->next = malloc(sizeof(struct dllNode));
							dllTail = dllTail->next;
							dllTail->prev = temp;   // dont need to malloc prev because the previous node struct was already allocated
							dllTail->next = NULL;
							dllTail->pid = malloc(sizeof(int));
							*dllTail->pid = child;
						}
					}
				}
			}
			cleanupCommand(currentCommand);
		}
		current = dllHead;
		while (current != NULL && current->pid != NULL) {
			bTerminated = waitpid(*current->pid, &bChildStatus, WNOHANG);
			if (bTerminated == -1) {
				printf("waitpid on background process failed\n");
				fflush(stdout);
			}
			else if (bTerminated != 0) {   // if a background child process has terminated print message and free from heap and list
				if (WIFEXITED(bChildStatus)) {
					bExitStatus = WEXITSTATUS(bChildStatus);
					printf("background pid %d is done: exit value %d \n", bTerminated, bExitStatus);
				}
				else {
					bSignalNum = WTERMSIG(bChildStatus);
					printf("background pid %d is done: terminated by signal %d \n", bTerminated, bSignalNum);
				}
				fflush(stdout);
				if (current == dllHead) {   
					if (current->next == NULL) { // if we are at the head of the dll of child processes and head is only node in list
						free(current->pid);
						current->pid = NULL;
						current = current->next;
					}
					else {
						struct dllNode* temp;
						temp = current;
						dllHead = current->next;
						current = dllHead;
						free(temp->pid);
						free(temp);
					}
				}
				else if (current == dllTail) {
					fflush(stdout);
					struct dllNode* temp;
					temp = current;
					dllTail = current->prev;
					free(temp->pid);
					free(temp);
					dllTail->next = NULL;
					current = NULL;
				}
				else {
					fflush(stdout);
					struct dllNode* temp;
					temp = current;
					current->prev->next = current->next;
					current->next->prev = current->prev;
					current = current->next;
					free(temp->pid);
					free(temp);
				}
				continue;
			}
			current = current->next;
		}
		if (termStopped == 1) {
			if (fgOnly == 0) {
				printf("\nExiting foreground-only mode\n");
			}
			else {
				printf("\nEntering foreground-only mode (& is now ignored)\n");
			}
			fflush(stdout);
		}
		termStopped = 0;
		builtIn = 0;
		jumpDown = 0;
	}
	return 0;
}