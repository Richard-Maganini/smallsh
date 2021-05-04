#include "commandParser.h"
#include "builtIns.h"
#include "dll.h"
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>

// globals for handling foreground only mode
int fgOnly = 0;       // 0 when not in foreground only mode (initial), 1 when in foreground only mode
int termStopped = 0;  // set to 1 when user does ctrl-z. reset to 0 every prompt loop. 
					     // -necessary to initiate a new prompt when ctrl-z occurs at empty prompt 
					     // -also ensures fg-mode messages are displayed just before next prompt is displayed


void checkBPs(struct dllNode* dllHead, struct dllNode* dllTail) {
	//---checks for finished child processes by looping through DLL of child processes that were alive on last check--//
	struct dllNode* current = dllHead;
	int bTerminated;  
	int bChildStatus;   
	int bExitStatus;
	int bSignalNum;
	while (current != NULL && current->pid != NULL) {
		bTerminated = waitpid(*current->pid, &bChildStatus, WNOHANG);    // bTerminated holds non-blocking call to see if the current child is done
		if (bTerminated == -1) {
			printf("waitpid on background process failed\n");
			fflush(stdout);
		}
		else if (bTerminated != 0) {   // if a background child process has terminated print message, remove from DLL, and free its dllNode from heap
			//print message
			if (WIFEXITED(bChildStatus)) {
				bExitStatus = WEXITSTATUS(bChildStatus);
				printf("background pid %d is done: exit value %d \n", bTerminated, bExitStatus);
			}
			else {
				bSignalNum = WTERMSIG(bChildStatus);
				printf("background pid %d is done: terminated by signal %d \n", bTerminated, bSignalNum);
			}
			fflush(stdout);

			// update dll references and free its dllNode memory from heap
			if (current == dllHead) { // if we are at the head of the dll of child processes
				if (current->next == NULL) {  // and head is only node in list, just free dllHead pid
					free(current->pid);
					current->pid = NULL;
					current = current->next;
				}
				else {					     // and head is not only node in list
					struct dllNode* temp;
					temp = current;
					dllHead = current->next;
					current = dllHead;
					free(temp->pid);
					free(temp);
				}
			}
			else if (current == dllTail) {   // elif we are at the tail of dll of length >=2
				struct dllNode* temp;
				temp = current;
				dllTail = current->prev;
				free(temp->pid);
				free(temp);
				dllTail->next = NULL;
				current = NULL;
			}
			else {							 // else we must be at a middle node
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
}


void handleTSTP(int sigNum) {
	//--- custom signal handler for TSTP, toggles globals involved in foreground only mode ---// 
	if (fgOnly == 0) {
		fgOnly = 1;       // ignore & operator until next ctrl-z
	}
	else {
		fgOnly = 0;       // respect & operator until next ctrl-z
	}
	termStopped = 1;      // tell main loop to print message about foreground only mode at appropriate time
}

int main(void) {
	pid_t child;		  // holds the most recently forked child (foreground or background)
	int childStatus;      // holds status info about foreground child process inspected by most recent waitpid call on a foreground child process
	int* exitStatus = malloc(sizeof(int));   // if most recent finished child foreground process exited, points to its exit value. else points to 44
	*exitStatus = 44;
	int* signalNum = malloc(sizeof(int));    // if most recent finished child foreground process was terminated, points to number of terminating signal. else points to 44
	*signalNum = 44;
	int* childProcessExecuted = malloc(sizeof(int));    // points to 1 once a foreground child process is finished
	*childProcessExecuted = 0;
	int skip = 0;         // set to 1 if the user types enter or ctrl-z at empty prompt or if user types # as first character of a command

	struct dllNode* dllHead = malloc(sizeof(struct dllNode));    // head and tail of DLL of process ids of background children alive since last loop
	struct dllNode* dllTail;
	dllHead->pid = NULL;
	dllHead->prev = NULL;
	dllHead->next = NULL;

	struct sigaction ignoreSignal = { 0 };    // signal handling struct for ignoring signals
	ignoreSignal.sa_handler = SIG_IGN;
	sigfillset(&ignoreSignal.sa_mask);
	ignoreSignal.sa_flags = 0;
	sigaction(2, &ignoreSignal, NULL);

	struct sigaction dontIgnoreSignal = { 0 };  // signal handling struct for setting default signal handling behavior for signals
	dontIgnoreSignal.sa_handler = SIG_DFL;
	sigfillset(&dontIgnoreSignal.sa_mask);
	dontIgnoreSignal.sa_flags = 0;

	struct sigaction toggleFgOnly = { 0 };   // signal handling struct responsible for foreground only mode toggling/notification when user ctrl-z
	toggleFgOnly.sa_handler = handleTSTP;
	sigfillset(&toggleFgOnly.sa_mask);
	toggleFgOnly.sa_flags = 0;
	sigaction(SIGTSTP, &toggleFgOnly, NULL);

	// main shell loop
	while (1) {

		// get command and check if should not attempt to run a command
		char* input = getCommand();
		if (input[0] == '\n' || input[0] == '#' || termStopped == 1) { // termStopped will be 1 here if the user ctrl-z at prompt
			skip = 1;
		}
		else { // if we should attempt a command
			// parse input into command struct
			struct command* currentCommand;
			currentCommand = parseCommand(input);
			if (*currentCommand->foreground == 2) {   // foreground member of currentCommand will be 2 here if something illegal command syntax in input
				printf("Invalid command syntax. Try again");
				fflush(stdout);
			}

			// if command is a built in, execute it from within call in next line and return 1, else return 0
			int builtIn = handleBuiltIns(currentCommand, dllHead, childProcessExecuted, exitStatus, signalNum);

			// if command was not a built in, need to do a fork + other stuff
			if (builtIn == 0) {
				// branch for non built in foreground commands
				if (*currentCommand->foreground == 1 || fgOnly == 1) {
					child = fork();
					if (child == -1) {
						printf("fork failed\n");
						fflush(stdout);
					}
					else if (child == 0) {   // fork child process branch
						// get args for non built in command we are about to exec() from currentCommand struct and store them in argv for foreground execvp call
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
						// check for input output redirection and perform it if it was specified
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

						// tell the actual command were about to run with exec to respond to ctrl-c as normal but ignore ctrl-z;
						// successful exec() calls will still listen to specified SIG_IGN and SIG_DFL signal handling but ignore any other custom signal handling 
						// specified prior to the exec call in the exec-calling process
						sigaction(2, &dontIgnoreSignal, NULL);
						sigaction(SIGTSTP, &ignoreSignal, NULL);
						execvp(currentCommand->name, argv);
						printf("%s: no such file or directory\n", currentCommand->name);
						fflush(stdout);
						exit(1);			// if the exec call fails exit the child with status 1
					}

					else { // fork parent process branch -- simply waits for fork child to finish and then updates *exitStatus/*signalNum accordingly
						int childDone = waitpid(child, &childStatus, 0);
						while (childDone == -1) {  // wait for the fork child to finish
							childDone = waitpid(child, &childStatus, 0);
						}
						*childProcessExecuted = 1;
						if (WIFEXITED(childStatus)) { // update exitStatus (used by built in status command) if fork child exited and reset *signalNum
							*exitStatus = WEXITSTATUS(childStatus);  
							*signalNum = 44;
						}
						else {   // update signalNum (used by built in status command) if fork child was terminated and reset *exitStatus
							*signalNum = WTERMSIG(childStatus);  
							printf("terminated by signal %d\n", *signalNum);
							*exitStatus = 44;
						}
					}
				}

				// branch for non built in background commands
				else {
					child = fork();
					if (child == -1) {
						printf("fork failed\n");
						fflush(stdout);
					}
					else if (child == 0) {   // fork child process branch
						// handle input output redirection for the background process. if none specified redirect input and output to dev/null
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

						// get args for the non built in command we are about to exec() from currentCommand struct and store them in argv for foreground execvp call
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
						// tell the actual command were about to run with exec to ignore ctrl-c and ctrl-z;
						// successful exec() calls will still listen to specified SIG_IGN and SIG_DFL signal handling but ignore any other custom signal handling 
						// specified prior to the exec call in the exec-calling process
						sigaction(2, &ignoreSignal, NULL);
						sigaction(SIGTSTP, &ignoreSignal, NULL);
						execvp(currentCommand->name, argv);
						exit(1);
					}
					else { // fork parent process branch -- doesn't wait for the fork child to finish, instead adds it to the dll of background child processes pointed to by dllHead
						printf("background pid is %d\n", child);
						fflush(stdout);
						if (dllHead->pid == NULL) {  // if there is nothing in the background child process dll prior to adding this fork child to it
							dllHead->pid = malloc(sizeof(int));
							*dllHead->pid = child;
							dllTail = dllHead;
						}
						else {   // if background child process dll not empty prior to adding this fork child to it
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
			// cleanup currentCommand struct memory with free()
			cleanupCommand(currentCommand);
		}
		// check background child processes to see if any of them have finished. print message for each one that finished and remove it from the dll
		checkBPs(dllHead, dllTail);
		// print relevant foreground only mode message if user ctrl-z at prompt before typing enter or during foreground command execution
		if (termStopped == 1) {
			if (fgOnly == 0) {
				printf("\nExiting foreground-only mode\n");
			}
			else {
				printf("\nEntering foreground-only mode (& is now ignored)\n");
			}
			fflush(stdout);
		}
		// reset termStopped and skip so they can be reused in next loop to monitor ctrl-z/empty command/comment command
		termStopped = 0;
		skip = 0;
	}
	// free still unfreed malloced stuff prior to return from main
	free(exitStatus);
	free(signalNum);
	free(childProcessExecuted);
	return 0;
}