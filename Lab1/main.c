#define _GNU_SOURCE
#include <fcntl.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

enum Redirect { Input = 0,
				Output = 1 };
enum Side { LEFT = 0,
			RIGHT = 1 };
enum Status { STOPPED = -1, RUNNING = 0, DONE = 1 };

#define PROCESS_GROUP 10
typedef struct process {
	char* arg1;	  //Ex. this would be "ls", then argv should be "-l"
	char** argv;  // all arguments
	int numTokens;
	int numRedirects;  //how many redirects
	int outRedirectLoc;
	int inRedirectLoc;
	int errRedirectLoc;
	int pipeLoc;	  //how many pipes
	int baseCommand;  //use as bool -- 0 is false, 1 is true
	char* output_file;
	char* input_file;
	char* error_file;
	enum Side pipeSide;	 //TODO: create enum for sides
} process;

typedef struct job_t {
	int jobid;
	pid_t pGid;
	enum Status status;
	struct job_t* next;
} job;

//So apparently tokens is a keyword?
static void sig_handler(int signo)
{
	int pid = getpid();
	switch (signo) {
	case SIGCHLD:
		//TODO: This case
		printf("Caught sig child \n");
		break;
	case SIGINT:
		// printf("caught SIGINT, PID: %d \n", getpid());
		kill(pid, SIGINT);
		waitpid(pid, 0, WNOHANG | WUNTRACED);
		break;
	case SIGTSTP:
		// printf("caught SIGINT, PID: %d \n", getpid());
		kill(pid, SIGTSTP);
		waitpid(pid, 0, WNOHANG | WUNTRACED);
		break;
	}
}

process* makeProcess(char* inputStr)
{
	//track number for a loop later
	process* newProcess = malloc(sizeof(process));
	char** argv = (char**)malloc(sizeof(char*) * 2000);
	int numTokens = 0;
	int pipeLoc = 0;
	int numRedirects = 0;
	//ok so this needs to be a double arr
	//Why?
	//String is char* (dynaamic size) -> arr of char* is char**
	// char** tokensArr = tokens;
	char* currToken;
	newProcess->argv = argv;
	currToken = strtok(inputStr, " ");
	while (currToken != NULL) {
		// printf("Curr token is %s \n", currToken);
		if (numTokens == 0) {
			newProcess->arg1 = currToken;
		}
		// tokensArr[numTokens] = currToken;
		if (strcmp(currToken, "|") == 0) {
			// runPipe();
			pipeLoc = numTokens;
			//TODO: get side info here?
		}
		else if (strcmp(currToken, "<") == 0) {
			// val = runRedirect(myTokens, numTokens, i, Input);
			numRedirects++;
			newProcess->inRedirectLoc = numTokens;
		}
		else if (strcmp(currToken, ">") == 0) {
			// val = runRedirect(myTokens, numTokens, i, Output);
			numRedirects++;
			newProcess->outRedirectLoc = numTokens;
		}
		else if (strcmp(currToken, "2>") == 0) {
			// val = runRedirect(myTokens, numTokens, i, Output);
			numRedirects++;
			newProcess->errRedirectLoc = numTokens;
		}
		else {
			newProcess->argv[numTokens] = currToken;
			numTokens++;
		}

		currToken = strtok(NULL, " ");
	}
	// tokensArr[numTokens] = NULL;
	newProcess->argv[numTokens] = NULL;
	newProcess->numTokens = numTokens;
	newProcess->pipeLoc = pipeLoc;
	newProcess->numRedirects = numRedirects;
	if (pipeLoc == 0 && numRedirects == 0) {
		newProcess->baseCommand = 1;
	}
	// printf("%s is in tokens (tokenize func) \n", tokensArr[numTokens - 1]);
	return newProcess;
}

process* splitProcess(process* currProcess)
{
	//track number for a loop later
	process* rightProcess = malloc(sizeof(process));
	char** argv = (char**)malloc(sizeof(char*) * 2000);
	int numTokens = 0;
	int pipeLoc = currProcess->pipeLoc;
	int numRedirects = 0;
	//ok so this needs to be a double arr
	//Why?
	//String is char* (dynaamic size) -> arr of char* is char**
	// char** tokensArr = tokens;
	char* currToken;
	rightProcess->argv = argv;
	rightProcess->arg1 = currProcess->argv[pipeLoc];
	for (int i = 0; i < currProcess->numTokens; i++) {
		char* currToken = currProcess->argv[i];
		if (i >= pipeLoc) {
			if (strcmp(currToken, "<") == 0) {
				// val = runRedirect(myTokens, numTokens, i, Input);
				numRedirects++;
				rightProcess->inRedirectLoc = numTokens;
			}
			else if (strcmp(currToken, ">") == 0) {
				// val = runRedirect(myTokens, numTokens, i, Output);
				numRedirects++;
				rightProcess->outRedirectLoc = numTokens;
			}
			else if (strcmp(currToken, "2>") == 0) {
				// val = runRedirect(myTokens, numTokens, i, Output);
				numRedirects++;
				rightProcess->errRedirectLoc = numTokens;
			}
			else {
				rightProcess->argv[i - pipeLoc] = currToken;
				numTokens++;
			}
			currProcess->argv[i] = NULL;
		}
	}
	// tokensArr[numTokens] = NULL;
	rightProcess->argv[numTokens] = NULL;
	rightProcess->numTokens = numTokens;
	// newProcess->pipeLoc = pipeLoc;
	rightProcess->numRedirects = numRedirects;
	if (pipeLoc == 0 && numRedirects == 0) {
		rightProcess->baseCommand = 1;
	}
	// printf("%s is in tokens (tokenize func) \n", tokensArr[numTokens - 1]);
	return rightProcess;
}

int runProcess(process* currProcess, int isPipe, int isRight, int fd[], pid_t pgid)
{
	// printf("Im in runProcess \n");
	// printf("the out file is %s \n", currProcess -> output_file);
	int pid = fork();
	printf("THe command is %s and PID is %i and PGID is %d \n", currProcess->arg1, pid, getpgid(0));
	if (pid == 0) {
		if (pgid != -1) {
			int newPgid = setpgid(0, pgid);
		}
		if (!isPipe) {
			if (currProcess->output_file) {
				// printf("reached outFile set up \n");
				int outFile = open(currProcess->output_file, O_CREAT | O_WRONLY | O_TRUNC,
								   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
				dup2(outFile, 1);
			}
			if (currProcess->input_file) {
				// printf("reached inFile set up \n");
				int inFile = open(currProcess->input_file, O_RDONLY);  //TODO: numTokens -1 isn't always the one
				if(inFile == -1){
					return -1;
				}
				dup2(inFile, 0);
			}
			if (currProcess->error_file) {
				int outFile = open(currProcess->error_file, O_CREAT | O_WRONLY | O_TRUNC,
								   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
				dup2(outFile, 2);
			}
		}
		else {
			//is a pipe
			if (!isRight) {
				//Left
				close(fd[0]);
				dup2(fd[1], 1);
			}
			else {
				//Right
				close(fd[1]);
				dup2(fd[0], 0);
			}
		}
		setpgid(0, 0);
		printf("THe command is %s and PID is %i and PGID is %d (just set pgid) \n", currProcess->arg1, pid, getpgid(0));
		int validate = execvp(currProcess->arg1, currProcess->argv);
		if (validate == -1) {
			exit(-1);
		}
	}
	// else {
	// 	// printf("This is my pid before  %i \n", pid);
	// 	// printf("THe command is %s and PID is %i \n", currProcess -> arg1, pid);
	// 	int status;
	// 	printf("waiting on pid %i \n", pid);
	// 	if(pgid != -1){
	// 		printf("and now and im waiting on pgid %i \n", pgid);
	// 		waitpid(pgid, &status, 0);
	// 		waitpid(pid, &status, 0);
	// 	}else {
	// 		waitpid(pid, &status, 0);
	// 	}

	// 	return pid;
	// }
	return pid;
}


int main()
{
	char** myTokens = (char**)malloc(sizeof(char*) * 2000);
	int fd[2];
	char* inputPtr;
	

	// printf() displays the string inside quotation
	while (1) {
		char *input = readline("# "), *inputStr;
		inputPtr = input;

		//double pointer like arraylist
		if (input != NULL) {
			inputStr = strdup(input);
		}

		//TODO: Tokenize function to split inputStr into pieces?
		process* currProcess = makeProcess(input);

		//TODO: check if bg, fg, jobs, etc
		int pid, pid2;
		int status;
		if (currProcess->baseCommand == 1) {  //No pipes, no redirects, run command as is
			//Single command
			// int pid = fork();
			pid = runProcess(currProcess, 0, -1, NULL, -1);
			
			// if (pid == 0) {
			// 	//child
			// 	int val = execvp(currProcess->arg1, currProcess->argv);
			// 	printf("status was %i \n", val);
			// }
			// else {
			// 	
			// }
			waitpid(pid, &status, 0);
		}
		else {
			if (currProcess->pipeLoc != 0) {
				//TODO: set up pipes here
				process* processLeft = currProcess;
				//Split process turns one process with a pipe into 2
				process* processRight = splitProcess(currProcess);
				int index = currProcess->pipeLoc;
				pipe(fd);  //TODO: handle error
				pid = runProcess(processLeft, 1, 0, fd, -1);
				printf("PID1 is %i \n", pid);
				// int pgid = getpgrp();
				setpgid(pid, 0);
				pid2 = runProcess(processRight, 1, 1, fd, pid);
				// waitpid(pid, &status, 0);
				close(fd[0]);
				close(fd[1]);

				waitpid(pid, &status, 0);
				waitpid(pid2, &status, 0);
				// waitpid(pid, &status, 0);
				free(processLeft);
				free(processRight);
			}
			if (currProcess->numRedirects != 0) {
				// printf("Have a redirect  \n");
				//TODO: set up redirects
				int outRedirectToken = currProcess->outRedirectLoc;
				int inRedirectToken = currProcess->inRedirectLoc;
				if (outRedirectToken > 0) {
					currProcess->output_file = currProcess->argv[outRedirectToken];
					currProcess->argv[outRedirectToken] = NULL;
					// printf("the out file is %s \n", currProcess->output_file);
				}
				if (inRedirectToken > 0) {
					currProcess->input_file = currProcess->argv[inRedirectToken];
					currProcess->argv[inRedirectToken] = NULL;
				}
				pid = runProcess(currProcess, 0, -1, NULL, -1);
				waitpid(pid, &status, 0);
			}
			
		}
	}
	free(myTokens);
	free(inputPtr);
	return 0;
}

// WARNING: Make sure to have another shell ready since the child won't respond to
// Ctrl + C pressed from the parent, and parent will simply print "caught SIGINT..."
// To terminate the child and hence the parent as well, note down the PID of the child
// and kill from the another shell using "kill -SIGINT <child_pid>
// You can also use "ps ax | grep yes" to get child's pid

// int main(void) {
//     // Parent should print "caught SIGNINT for every Ctrl + C"
//     if (signal(SIGINT, sig_handler) == SIG_ERR)
//         perror("Couldn't install signal handler");

//     pid_t pid = fork();
//     if (pid == 0) {
//         // IMPORTANT: What happens if we remove this setpgid() call? Why does that
//         // behaviour happen?
//         setpgid(0,0);

//         // Since I restored child's signal handler to SIG_DFL,
//         // it should now get killed. Use another bash shell to run
//         // `kill` command to send SIGINT to the child (PID printed by parent below)
//         // using: kill -SIGINT <child_pid>
//         if (signal(SIGINT, SIG_DFL) == SIG_ERR)
//             perror("Couldn't install signal handler");

//         execlp ("yes", "yes", NULL);
//         // NOTE: What if I try following command instead? Why is the behaviour different?
//         // execlp("bash", "bash", "-c", "while true; do echo child here! I am alive; sleep 1; done", NULL);
//     }
//     printf ("PID of parent: %d\n", getpid());
//     printf ("PID of child: %d\n", pid);
//     wait(NULL);
//     return 0;
// }
