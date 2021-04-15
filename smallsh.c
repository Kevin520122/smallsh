#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

//argArr is to hold the command line word by word
char* argArr[512];
//numArg is a counter for numArg
int numArg = 0;
int numProcess = 0;
//isBackground is to track if the process runs in background.
int isBackground = 0;
//foregroundOnly is to check the mode 
int foregroundOnly = 0;
//isSignal is to track if the program is terminated abnormally
int isSignal = 0;
//input is to get the input file name
char* input = NULL;
//output is to get the output file name
char* output = NULL;

//This function is to clean up the dynamic memory 
void deleteList(){
    for(int i = 0; i< numArg;++i){
        free(argArr[i]);
        argArr[i] = NULL;
    }
    if(input != NULL){
            free(input);
            input = NULL;
    }
    if(output != NULL){
        free(output);
        output = NULL;
    }
}

//This function is to execute corresponding code when receiving signal SIGTSTP
void execSIGTSTP(){
    if(foregroundOnly == 0){
        char prompt[] = "Entering foreground-only mode (& is now ignored)\n";
        //prompt contains 49 chars
        //According to hints from the assignment guide, to use write() which is reentrant
        write(1, prompt, 49);
        fflush(stdout);
        //Turn on
        foregroundOnly = 1;
    }else{
        char prompt[] = "Exiting foreground-only mode\n";
        write(1, prompt, 29);
        fflush(stdout);
        //Turn off
        foregroundOnly = 0;
    }
}

//This function is to convert $$ to current PID
//Source Citation: https://github.com/Davidqww/smallsh/blob/master/smallsh.c
//According to his ideas, I modified his alogorithm to myself.
void convertToPid(char command[]){
    //Duplicate the content of command line
    //char* buffer = strdup(command);
    char buffer[2048] = "";
    strcpy(buffer, command);
    //Find if exists $$
    for(int i = 0;i<strlen(command) - 1; ++i){
        
        if((buffer[i] == '$') && (buffer[i+1] == '$')){
            //Chnage $$ with %d to form the format of sprintf
            buffer[i] = '%';
            buffer[i+1] = 'd';
            //Replace $$ with pid
            sprintf(command, buffer, getpid());
        }
    }
}
//This function is to read the command line form the user's input
void readCommand(){
    //Counter of arguments in the command line
    int ctr = 0;
    int idx = 0;
    //Hold the content of commanf line
    char command[2048] = "";
    printf(": ");
	fflush(stdout);
    //Get the command
	fgets(command, 2048, stdin);
    //Remove the newLine
    strtok(command, "\n");
    convertToPid(command);
    //Read the command line spliting by string
    char* token = strtok(command, " ");
    while(token != NULL){
        if(strcmp(token, "<") == 0){
            //Move to the next to get fileName
            token = strtok(NULL, " ");
            //get the input file name
            input = strdup(token);
        }else if(strcmp(token, ">") == 0){
            token = strtok(NULL, " ");
            //get the output file name
            output = strdup(token);
            
            //printf("%s\n", out);
        }else{
            //Put each argument in argArr
            argArr[ctr] = strdup(token);
            ctr += 1;
        }
        token = strtok(NULL, " ");
    }
    //Assign the final count to numArg
    numArg = ctr;
    if(strcmp(argArr[numArg - 1], "&") == 0){
        //Run at the back
        if(foregroundOnly == 0){isBackground = 1;}
        free(argArr[numArg-1]);
        numArg -= 1;
    }
    //ctr counts how many args each time
    argArr[numArg] = NULL; //Mark the end
}

//This function is to run the process in the background
void background(int wstats){
    pid_t tempPid = waitpid(-1, &wstats, WNOHANG);
    while(tempPid > 0 ){
        if (WIFEXITED(wstats) != 0) {// Checks if exited
				printf("background pid %d is done: exit value %d\n", tempPid, WEXITSTATUS(wstats));
        }
        if (WIFSIGNALED(wstats) != 0) {// If it was not exited, then it must have been killed by a signal
				printf("background pid %d is done: terminated by signal %d\n", tempPid, WTERMSIG(wstats));
        }
        tempPid = waitpid(-1, &wstats, WNOHANG);
    }
}

//This function will redirect to other directory by the user's input
void fileRedirect(){
    int inFd, outFd, tempFd, res;
    //isIn is to check if input file name is empty
    int isIn = 0;
    //isOut is to check if output file name is empty
    int isOut = 0;
    //Check input
    if(input != NULL){
        isIn = 1;
        //get the input file descriptor
        inFd = open(input, O_RDONLY);
        //If file doesn't exist
        if(inFd == -1){
            printf("cannot open %s for input\n", input);
            exit(1);
        }
        //Redirect from stdin to target
        res = dup2(inFd, 0);
        if(res == -1){
            perror("Fail to redirect!");
            exit(1);
        }
        close(inFd);
    }else{
        //Background Redirect without specified name
        if(isBackground == 1){
            tempFd = open("/dev/null", O_RDONLY);
            if(tempFd == -1){
                perror("cannot open null space\n");
                exit(1);
            }
            if(dup2(tempFd, 0) == -1){
                perror("Fail to redirect!\n");
                exit(1);
            }
        }
    }
    //Check output
    if(output != NULL){
        isOut = 1;
        //Get the output file descriptor
        outFd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0744);
        if(outFd == -1){
            printf("cannot open %s for output\n", output);
            exit(1);
        }
        //Redirect from stdout to target
        res = dup2(outFd, 1);
        if(res == -1){
            perror("Fail to redirect!\n");
            exit(1);
        }
        close(outFd);
    }else{
        //Background Redirect without specified name
        if(isBackground == 1){
            tempFd = open("/dev/null", O_RDONLY);
            if(tempFd == -1){
                perror("cannot open null space\n");
                exit(1);
            }
            if(dup2(tempFd, 1) == -1){
                perror("Fail to redirect!\n");
                exit(1);
            }
        }
    }
}

//This function is to print the current program status
void printStatus(int wstats){
    if(isSignal == 0){
        //If exit normally
        printf("exit value %d\n", wstats);
        fflush(stdout);
    }else{
        //If terminated abnormally
        printf("terminated by signal %d\n", WTERMSIG(wstats));
        fflush(stdout);
    }
}

//This function is to help the child process ignore signal SIGTSTP
void ignSigTstp(){
    struct sigaction sigtstp;
    sigtstp.sa_handler = SIG_IGN;
    sigfillset(&sigtstp.sa_mask);
    sigtstp.sa_flags = 0;
    sigaction(SIGTSTP, &sigtstp, NULL);
}

//This function is to execute non built-in command
void otherCommand(int* wstats, struct sigaction sigint){
    pid_t spawnPid = -5;
    pid_t childPid;
    //Create a new process
    spawnPid = fork();
    switch(spawnPid){
        case -1:
            *wstats = 1;
            perror("err!\n");
            exit(1);
            break;
        //Child Process
        case 0:
            //Ignore signal SIGTSTP
            ignSigTstp();
            //If the process is in the foreground
            if(isBackground == 0){
                sigint.sa_handler = SIG_DFL;
                sigaction(SIGINT, &sigint, NULL); //Run the action
            }
            //Redirect to the traget file
            fileRedirect();
            //Execute the command
            if (execvp(argArr[0], argArr)) {
				printf("%s: no such file or directory\n", argArr[0]);
				fflush(stdout);
                *wstats = 1;
				exit(1);
			}
            break;
        //Parent Process
        default:
            if(isBackground == 1){
                //The shell will not wait for background process
                waitpid(spawnPid, wstats, WNOHANG);
                printf("background pid is %d\n", spawnPid);
                fflush(stdout);
            }else{
                childPid = waitpid(spawnPid, wstats, 0);
                if(childPid != 0){
                    //Exit normally
                    if (WIFEXITED(*wstats)){
                        if(WEXITSTATUS(*wstats) == 0){
                            *wstats = 0;
                        }else{
                            *wstats = 1;
                        }
                        isSignal = 0;
                    }else{	// Check if child process was killed by signal
                        printf("terminated by signal %d\n", WTERMSIG(*wstats));
                        fflush(stdout);
                        *wstats = WTERMSIG(*wstats);
                        isSignal = 1;
                    }
                }
                //Check background process
                background(*wstats);
                break;
            }
    }
}

int main(){
    //isExit is to check if the user exits the program
    int isExit = 0;
    //wstats stands for the status of program
    int wstats = 0;
    
    //The parenet process must ignore Control-C
    struct sigaction sigint = {0};
    sigint.sa_handler = SIG_IGN;
    sigfillset(&sigint.sa_mask);
    sigaction(SIGINT, &sigint, NULL); //Run the handler function

    //Control-Z
    struct sigaction sigtstp = {0};
    sigtstp.sa_handler = execSIGTSTP;
    sigfillset(&sigint.sa_mask);
    sigtstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sigtstp, NULL);

    //Enter to the smallsh
    while(isExit == 0){
        //Read the command from the user's input
        readCommand();
        //Comments
        if(strcmp(argArr[0], "#") == 0 || strcmp(argArr[0], "\n") == 0){
           //Make sure built-in command ignore &
            isBackground = 0;
            continue;
        }else if(strcmp(argArr[0], "exit") == 0){
            //Make sure built-in command ignore &
            isBackground = 0;
            //Free the memory
            deleteList();
            //Exit
            isExit = 1;
        }else if(strcmp(argArr[0], "cd") == 0){
            //Make sure built-in command ignore &
            isBackground = 0;
            //Tha case directory is empty.
            if(numArg == 1){
                chdir(getenv("HOME"));
            }else{
                //Change directory
                if(chdir(argArr[1]) == -1){
                    printf("Directory not found.\n");
					fflush(stdout);
                }
            }
        }else if(strcmp(argArr[0], "status") == 0){
            //Make sure built-in command ignore &
            isBackground = 0;
            printStatus(wstats);
        }else{
            //This is for foregound-only mode, ignore &
            otherCommand(&wstats, sigint);
        }
        //Every time, initialize again
        deleteList();
        isBackground = 0;
        isSignal = 0;
    }
}