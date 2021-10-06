#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>


#define MAX_CHARACTERS 2048

pid_t spawnPid = -5;
int flagBackground = 0;
int exitFlag = 1;
int statusCode;
char input[MAX_CHARACTERS];
int childStatus = -5;
int bgArray[40];
int bgProcessCounter;
int flagSIGTSTP = 0;
int signalCount = 0;
void startShell();
void builtInCommands();
void forkProcess();
void childCommands();
void checkBGfeasible();
void expansion();
void handle_SIGINT(int signo);
void handle_SIGTSTP();


//Initializes SIGINT and SIGTSTP, assigns each to their handlers, and 
//creates sigaction struct for each. Checks background processes while exit 
//flag == 1. Displays : prompt and gathers and handles input from user.
void startShell() {
	struct sigaction SIGINT_action = {0};
	struct sigaction SIGTSTP_action = {0};
	SIGINT_action.sa_handler = handle_SIGINT;
	sigfillset(&SIGINT_action.sa_mask);
	sigaction(SIGINT, &SIGINT_action, NULL);
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);
	//Checks background processes, gathers user input, checks for built in commands
	while (exitFlag == 1) {
		int i;
        for (i = 0; i < bgProcessCounter; i++) {
            //Parent process is non waiting
            if (waitpid(bgArray[i], &childStatus, WNOHANG) > 0) {
                //If child process revieced termination signal
                if (WIFSIGNALED(childStatus)) {
                    printf("background pid %d is done: ", bgArray[i]);
                    printf("terminated by signal %d\n", WTERMSIG(childStatus));
					fflush(stdout);
                }
                //If child process terminated normally
                if (WIFEXITED(childStatus)) {
                    printf("exit value %d\n", WEXITSTATUS(childStatus));
					fflush(stdout);
                }
            }
	    }
	//Stores first 4 characters of input
	char firstInput[5] = {0};
	printf(": ");
	fflush(stdout);
	//Gets input
	fgets(input, sizeof(input), stdin);
	//Formats input by adding '\0'
	input[strcspn(input, "\n")] = '\0';
	strncpy(firstInput, input, 4);
	//Checks if output is occurring in command
	if (strcmp(firstInput, "echo") != 0) {
		if (strchr(input, '&') != NULL) {
			//If running in background, check background status
			checkBGfeasible();
		}
	}
	//Checks if command is SIGTSTP signal
	if (strstr(input, "TSTP") == NULL) {
		if (strstr(input, "$$") != NULL) {
			//If command needs to be expanded, call $$expansion function
			expansion();
		}
	}
	//Checks if command is SIGTSTP and calls signal catcher if true
	if (strstr(input, "TSTP") != NULL) {
		handle_SIGTSTP();
	}
	//calls function to process built ins
	builtInCommands();
	}
}


//Forks processes as needed in the shell. If the child is running in
//background, parent will proceed without waiting, and vice versa if child
//is running in foreground.
void forkProcess() {
	//Forks and creates a child process.
	spawnPid = fork();
	//Check if fork was successful 
	if (spawnPid < 0) {
		printf("Could not fork. Exit status: 1.\n");
		fflush(stdout);
		exit(1);
	}
	//Forked okay
	else if (spawnPid == 0) {
		if (signalCount > 0) {
			if (strstr(input, "kill") != NULL) {
				//kills and proceeds with Child PID
				char killCommandArray[MAX_CHARACTERS];
                int size;
                //Stores length of input minus signal and '$$'
                size = (strlen(input) - 11);
                strncpy(killCommandArray, input, size);
                strcpy(input, killCommandArray);
                sprintf(killCommandArray, "%d", getpid());
                strcat(input, killCommandArray);
            
		    }
		}
		//Executes child commands
		childCommands();
	}
	else {
		//Checks if child process is running in background
		if (flagBackground == 1) {
			//Add child to background array
			bgArray[bgProcessCounter] = spawnPid;
			bgProcessCounter++;
			//Parent proceeds without waiting for child
			waitpid(spawnPid, &childStatus, WNOHANG);
			//no background processes
			flagBackground = 0;
			printf("background pid is %d\n", spawnPid);
			fflush(stdout);
		}
		else {
			//Parent waits for child to end
			waitpid(spawnPid, &childStatus, 0);
			//If error occured, set error status
			if (WIFEXITED(childStatus))
				statusCode = WEXITSTATUS(childStatus);
		}
	}
}


//Controls and handles child processes.
void childCommands() {
	int i = 0;
	int index = 0;
	char* commands[512];
	int redirectFlag = 0;
	int filePointer = 2;
	int fd;
	//Make first token from bash command
	commands[0] = strtok(input, " ");
	while (commands[index] != NULL) {
		index++;
		commands[index] = strtok(NULL, " ");
	}
	while (index != 0) {
		//Check for > redirect
		if (strcmp(commands[i], ">") == 0) {
			//Creates a write only file
			fd = open(commands[i + 1], O_CREAT | O_WRONLY, 0755);
			//Documents redirect
			redirectFlag = 1;
			//Sets the file_pointer to stdout
			filePointer = 1;
		}
		//Check < redirect
		else if (strcmp(commands[i], "<") == 0) {
			//creates a read only file
			fd = open(commands[i + 1], O_RDONLY, 0);
			if (fd < 0) {
				//Output error if file did not open properly
				printf("cannot open %s for input\n", commands[i + 1]);
				fflush(stdout);
				exit(1);
			}
			else {
				//Documents redirect
				redirectFlag = 1;
				//Sets filePointer to stdin
				filePointer = 0;
			}
		}
		if (redirectFlag == 1) {
			dup2(fd, filePointer);
			//commands[i] = 0 removes r
			commands[i] = 0;
			//Call exec using redirect
			execvp(commands[0], commands);
			close(fd);
		}
		//Resets redirect flag
		redirectFlag = 0;
		//Reset filePointer
		filePointer = -2;
		index -= 1;
		i += 1;
	}
	//No redirect, executes a command
    statusCode = execvp(commands[0], commands);
	if (statusCode != 0) {
		printf("%s: no such file or directory\n", input);
		fflush(stdout);
		//Exits program using error code
		exit(statusCode);
	}
}


//Handles all built in functions required for smallsh
void builtInCommands() {
	//holds current working directory
	char cwd[MAX_CHARACTERS] = "";
	//Handles 'exit' built in
	if (strcmp(input, "exit") == 0) {
		//Quits the program
		exitFlag = 0;
	}
	else if ((strcmp(input, " ") == 0)) {
		//Nothing
	}
	else if ((strncmp(input, "#", 1)) == 0) {
		//Nothing
	}
	//Handles 'cd' built in 
	else if (strncmp(input, "cd", 2) == 0) {
		//directory array
		char pathArray[MAX_CHARACTERS] = "";
		char slash[] = "/";
		//Returns current directory
		getcwd(cwd, sizeof(cwd));
		char *path = strstr(input, " ");
		//Parses path and appends all into one char array cwd
		if (path) {
			path++;
			strcat(cwd, slash);
			strcpy(pathArray, path);
			strcat(cwd, pathArray);
			//Changes directory to desired input 
			chdir(cwd);
		}
		else {
			//Returns HOME directory
			chdir("/nfs/stak/users/garcadal");
		}
		//Returns new current directory 
		printf("%s\n", cwd);
		fflush(stdout);
	}
	//Handles 'status' built in
	else if (strcmp(input, "status") == 0) {
		printf("exit value %d\n", statusCode);
		fflush(stdout);
	}
	else {
		//Calls forkProcess function
		forkProcess();
	}
	//Resets background flag
	flagBackground = 0;
}




//Used for $$ process expansion using the shell PID
void expansion() {
	int size;
	char exCommands[MAX_CHARACTERS];
	//Stores length of input minus '$$'
	size = (strlen(input) - 2);
	strncpy(exCommands, input, size);
	strcpy(input, exCommands);
	sprintf(exCommands, "%d", getppid());
	strcat(input, exCommands);
}


//Handles SIGINT, printing out witch signo is received
void handle_SIGINT(int signo) {
	printf("Terminated by signal %d\n", signo);
	fflush(stdout);
}


//Handles SIGTSTP
void handle_SIGTSTP() {
	if (flagSIGTSTP == 0) {
		//Reset signal count
		signalCount = 0;
		flagBackground = 0;
		flagSIGTSTP = 1;
		//After shell receives signal, prints message to user
		char* message= "Entering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 49);
		fflush(stdout);
		signalCount += 1;
	}

	else {
		flagSIGTSTP = 0;
		//Exits forground mode and resets the SIGTSTP flag
		char* message= "Exiting foreground-only mode\n";
		write(STDOUT_FILENO, message, 30);
		fflush(stdout);
		signalCount += 1;
	}
}


//Checks if a process can run in the background
void checkBGfeasible() {
	char bgCommands[MAX_CHARACTERS];
	int size;
	//Store length of input minus space and '&' 
	size = (strlen(input) - 2);
	if (flagSIGTSTP == 0) {
		//Run command in background if no signal is recieved
		flagBackground = 1;
	}
	strncpy(bgCommands, input, size);
	strcpy(input, bgCommands);
}


int main() {
	startShell();
    return 0;
}
