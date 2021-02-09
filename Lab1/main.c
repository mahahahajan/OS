#define _GNU_SOURCE
#include <fcntl.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdbool.h>
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
enum Status { EMPTY = -2,
			  STOPPED = -1,
			  RUNNING = 0,
			  DONE = 1 };

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
	int isBackgroundJob;
	enum Side pipeSide;	 //TODO: create enum for sides
} process;

typedef struct job {
	int jobid;
	char* jobStr;
	pid_t pGid;
	pid_t pGid2;
	enum Status status;
	struct job_t* next;
} job;

static void sig_handler(int signo);
void printProcess(process* p);
void cleanProcess(process* p);
process* makeProcess(char* inputStr);
process* splitProcess(process* currProcess);
int runProcess(process* currProcess, int isPipe, int isRight, int fd[], pid_t pgid);
job* getNextJob(job* jobsArr[]);
char* enumToString(enum Status val);
void update_jobs(job* jobsArr[], int mostRecentJob, int status);
void clean_job(job* currJob);
void print_jobs(job* jobsArr[], int mostRecentJob);

int mostRecentJob = 0;
int mostRecentFG = 0;
job* jobsArr[20];

//So apparently tokens is a keyword?
static void sig_handler(int signo)
{
	switch (signo) {
	// case SIGCHLD:
	// 	//TODO: This case
	// 	printf("Caught sig child \n");
	// 	break;
	case SIGINT:
		// printf("caught SIGINT, PID: %d \n", getpid());
		kill(jobsArr[mostRecentJob]->pGid, SIGINT);
		waitpid(most, 0, WNOHANG | WUNTRACED);
		break;
	case SIGTSTP:
		// printf("caught SIGINT, PID: %d \n", getpid());
		kill(jobsArr[mostRecentJob]->pGid, SIGTSTP);
		waitpid(pid, 0, WNOHANG | WUNTRACED);
		break;
	}
}

void printProcess(process* p)
{
	printf("Arg1 is %s \n", p->arg1);
	printf("should have %i tokens \n", p->numTokens);
	for (int i = 0; i < (p->numTokens) + 1; i++) {
		printf("curr arg is %s at index %i \n", p->argv[i], i);
	}
}
void cleanProcess(process* p)
{
	p->arg1 = NULL;	 //Ex. this would be "ls", then argv should be "-l"
	p->argv = NULL;	 // all arguments
	p->numTokens = -1;
	p->numRedirects = -1;  //how many redirects
	p->outRedirectLoc = -1;
	p->inRedirectLoc = -1;
	p->errRedirectLoc = -1;
	p->pipeLoc = -1;	  //how many pipes
	p->baseCommand = -1;  //use as bool -- 0 is false, 1 is true
	p->output_file = NULL;
	p->input_file = NULL;
	p->error_file = NULL;
	p->isBackgroundJob = -1;
}

process* makeProcess(char* inputStr)
{
	//track number for a loop later
	process* newProcess = malloc(sizeof(process));
	cleanProcess(newProcess);
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

		if (strcmp(newProcess->argv[numTokens - 1], "&") == 0) {
			newProcess->isBackgroundJob = 1;
			newProcess->argv[numTokens - 1] = NULL;
		}
	}

	// printf("MY first command is %s \n", newProcess->arg1);
	// for (int i = 0; i < newProcess->numTokens; i++) {
	// 	printf("token is %s \n", (newProcess->argv)[i]);

	// }
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
	if (strcmp(rightProcess->argv[numTokens - 1], "&") == 0) {
		rightProcess->isBackgroundJob = 1;
		rightProcess->argv[numTokens - 1] = NULL;
	}
	// printf("%s is in tokens (tokenize func) \n", tokensArr[numTokens - 1]);
	return rightProcess;
}

int runProcess(process* currProcess, int isPipe, int isRight, int fd[], pid_t pgid)
{
	// printf("Im in runProcess \n");
	// printf("the out file is %s \n", currProcess -> output_file);
	int pid = fork();
	// printf("THe command is %s and PID is %i and PGID is %d \n", currProcess->arg1, pid, getpgid(0));
	if (pid == 0) {
		if (pgid != -1) {
			int newPgid = setpgid(0, pgid);
		}
		if (isPipe != 1) {
			if (currProcess->output_file) {
				// printf("reached outFile set up \n");
				int outFile = open(currProcess->output_file, O_CREAT | O_WRONLY | O_TRUNC,
								   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
				dup2(outFile, 1);
			}
			if (currProcess->input_file) {
				// printf("reached inFile set up \n");
				int inFile = open(currProcess->input_file, O_RDONLY);  //TODO: numTokens -1 isn't always the one
				if (inFile == -1) {
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
			if (isRight == 0) {
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
		// printf("THe command is %s and PID is %i and PGID is %d (just set pgid) \n", currProcess->arg1, pid, getpgid(0));
		int validate = execvp(currProcess->argv[0], currProcess->argv);
		if (validate == -1) {
			// printf("TESt \n");
			_exit(-1);
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
job* getNextJob(job* jobsArr[])
{
	int jobFound = -1;

	for (int i = 0; i < 20; i++) {
		if (jobsArr[i]->status == EMPTY) {
			//Found spot for job
			jobsArr[i]->status = RUNNING;
			jobsArr[i]->jobid = i;
			return jobsArr[i];
		}
	}
	return jobsArr[0];
}

char* enumToString(enum Status val)
{
	switch (val) {
	case EMPTY:
		return "EMPTY";
	case STOPPED:
		return "Stopped";
	case RUNNING:
		return "Running";
	case DONE:
		return "Done";
	}
}

void printDoneJob(job* currJob)
{
	currJob->status = DONE;
	printf("[%d]-   %s\t\t%s\n", currJob->jobid, enumToString(currJob->status), currJob->jobStr);
	clean_job(currJob);
}

void update_jobs(job* jobsArr[], int mostRecentJob, int status)
{
	for (int i = 0; i < 20; i++) {
		if (jobsArr[i]->status != EMPTY) {
			int pid = jobsArr[i]->pGid;
			int pid2 = -1;
			int stat;
			if (jobsArr[i]->pGid2 != -1) {
				pid2 = jobsArr[i]->pGid2;
				stat = waitpid(pid2, &status, WNOHANG | WUNTRACED);
				if (stat != 0 && stat != -1) {
					printDoneJob(jobsArr[i]);
				}
			}
			else {
				stat = waitpid(pid, &status, WNOHANG | WUNTRACED);
				if (stat != 0 && stat != -1) {
					printDoneJob(jobsArr[i]);
				}
			}
		}
	}
}
void clean_job(job* currJob)
{
	currJob->status = EMPTY;
	currJob->pGid = -1;
	currJob->pGid2 = -1;
	currJob->jobid = -1;
	currJob->jobStr = NULL;
}

void print_jobs(job* jobsArr[], int mostRecentJob)
{
	char use = '-';
	for (int i = 0; i < 20; i++) {
		// printf("jobs at %i is %s \n", i, enumToString(jobsArr[i].status));
		if (jobsArr[i]->status != EMPTY) {
			if (i == mostRecentJob) {
				use = '+';
			}
			printf("[%d]%c %s\t\t%s\n", i, use, enumToString(jobsArr[i]->status), jobsArr[i]->jobStr);
			use = '-';
		}
	}
}

int main()
{
	char** myTokens = (char**)malloc(sizeof(char*) * 2000);
	int fd[2];
	int status;
	// printf() displays the string inside quotation
	// signal(SIGCHLD, sig_handler);
	signal(SIGTSTP, sig_handler);
	signal(SIGINT, sig_handler);

	for (int i = 0; i < 20; i++) {
		jobsArr[i] = (job*)malloc(sizeof(job));
		clean_job(jobsArr[i]);
	}

	while (1) {
		// signal(SIGCHLD, sig_handler);
		signal(SIGTSTP, sig_handler);

		update_jobs(jobsArr, mostRecentJob, status);

		char* input = readline("# ");
		char* ogInput;
		int pid = -1;
		int pid2 = -1;
		bool bgPresent = false;
		// inputPtr = input;
		if (strcmp(input, "") == 0 || strcmp(input, " ") == 0) {
			// printf("Why am i not hitting this \n");
			continue;
		}
		//double pointer like arraylist
		if (input != NULL) {
			//Want to save the string so that I can list in jobs
			ogInput = strdup(input);
		}

		//TODO: Tokenize function to split inputStr into pieces?
		process* currProcess = makeProcess(input);
		process* processLeft = NULL;
		process* processRight = NULL;

		//TODO: check if bg, fg, jobs, etc
		if (strcmp(currProcess->arg1, "jobs") == 0) {
			print_jobs(jobsArr, mostRecentJob);
			continue;
		}
		if (strcmp(currProcess->arg1, "bg") == 0) {
			//TODO: Handle bg stuff
			//So the way I understand it, bg finds the last stopped process and puts it in the background
			printf("Most recent job was %i \n", mostRecentJob);
			continue;
		}
		if (strcmp(currProcess->arg1, "fg") == 0) {
			//TODO: Handle bg stuff
			//So the way I understand it, bg finds the last stopped process and puts it in the background
			printf("Most recent fg job was %i \n", mostRecentFG);
			continue;
		}

		if (currProcess->pipeLoc != 0) {
			//TODO: set up pipes here
			processLeft = currProcess;
			//Split process turns one process with a pipe into 2
			processRight = splitProcess(currProcess);
			int index = currProcess->pipeLoc;
		}
		if (currProcess->numRedirects != 0) {
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
		}

		// newJob->jobid = getNextJob(jobsArr);
		//TODO: check if bg job otherwise handle waits

		if (processRight != NULL) {	 //This means that i have a pipe for sure

			if (processRight->isBackgroundJob == 1) {
				bgPresent = true;
			}
			pipe(fd);  //TODO: handle error
			pid = runProcess(processLeft, 1, 0, fd, -1);
			// printf("PID1 is %i \n", pid);
			// int pgid = getpgrp();
			setpgid(pid, 0);
			pid2 = runProcess(processRight, 1, 1, fd, pid);
			// waitpid(pid, &status, 0);
		}
		else {
			if (currProcess->isBackgroundJob != -1) {
				bgPresent = true;
			}
			pid = runProcess(currProcess, 0, -1, NULL, -1);
		}
		job* newJob = getNextJob(jobsArr);
		// printf("Try and abed in the morning \n ");
		newJob->jobStr = ogInput;
		newJob->pGid = pid;
		newJob->status = RUNNING;
		mostRecentJob = newJob->jobid;
		newJob->next = NULL;

		if (currProcess->isBackgroundJob == 1) {
			waitpid(pid, &status, WNOHANG | WUNTRACED);
		}
		else {
			mostRecentFG = newJob->jobid;
			waitpid(pid, &status, WUNTRACED);
			if (!WIFSTOPPED(status)) {
				//TODO: Clean job;
				// remove_job(job->jobid, (job_t*)root, NULL);
				clean_job(jobsArr[mostRecentJob]);
			}
		}
		if (pid2 != -1) {
			newJob->pGid2 = pid2;
			close(fd[0]);
			close(fd[1]);
			if (processRight->isBackgroundJob == 1) {
				waitpid(pid2, &status, WNOHANG | WUNTRACED);
			}
			else {
				waitpid(pid2, &status, WUNTRACED);
				if (!WIFSTOPPED(status)) {
					//TODO: Clean job;
					// remove_job(job->jobid, (job_t*)root, NULL);
					clean_job(jobsArr[mostRecentJob]);
				}
			}
			free(processRight->argv);
			free(processRight);
		}
		free(input);
		free(currProcess->argv);
		free(currProcess);
	}
	free(myTokens);
	free(jobsArr);
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
