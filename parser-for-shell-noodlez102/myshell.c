#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "myshell_parser.h" // Include the parser header file

#define MAX_COMMAND_LENGTH 256

int doCommand(struct pipeline *pipeline, int input, bool end, bool background){
    int fd[2];
    pipe(fd);
    pid_t pid = fork();

    if (pid == 0) { //child
        if(pipeline->commands->redirect_out_path){     
            int fdout = open(pipeline->commands->redirect_out_path,  O_WRONLY | O_CREAT | O_TRUNC, 0666);   
            if( fdout< 0){
                perror("ERROR");
                exit(0);
            }
            
            if(dup2(fdout, STDOUT_FILENO)==-1){
                perror("ERROR");
                exit(0);
            }
            close(fdout);
        }
        if(pipeline->commands->redirect_in_path){
            int fdin = open(pipeline->commands->redirect_in_path, O_RDONLY);
            if(fdin < 0){
                perror("ERROR");
                exit(0);
            }
            if(dup2(fdin, STDIN_FILENO)==-1){
                perror("ERROR");
                exit(0);
            }
            close(fdin);
        }
        //background 
        if(pipeline->is_background){
            close(fd[1]);
        }
        
        if(!end&& input == 0){
            dup2(fd[1], STDOUT_FILENO);
        }
        else if(!end && input != 0){
            dup2(input, STDIN_FILENO);
            dup2(fd[1], STDOUT_FILENO);
        }
        else{
            dup2(input, STDIN_FILENO);
        }
        execvp(pipeline->commands->command_args[0], pipeline->commands->command_args);
        perror("ERROR");
        exit(0);

        close(fd[1]);
        close(STDIN_FILENO);
        dup2(input, STDIN_FILENO);
        close(fd[0]);
    } else { //handles the parent
        int status;
        if(!background){
            waitpid(pid, &status, 0); 
        }

        close(fd[1]);
    }
    
    if(input != 0){
        close(input);
    }

    if(end){
        close(fd[0]);
    }
    return fd[0];

}
            
void signalHandler() {
    pid_t pid;
    int status;
    while(1){
        pid = waitpid(-1, &status, WNOHANG);
        if(pid <= 0){
            return;
        }
    }
}

int main(int argc, char* argv[]){
    char input[MAX_COMMAND_LENGTH];
    struct pipeline *pipeline;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); // Initialize sa to 0

    // Set the signal handler
    sa.sa_handler = signalHandler;
    sigaction(SIGCHLD, &sa, NULL);

    while (1) {
        if((argc > 1) &&strcmp(argv[1],"-n")==0){
        }else{
            printf("my_shell$ ");
        }
        
        fgets(input, sizeof(input), stdin);
        if(feof(stdin)){ //so it ends when ctrl D is pressed
            break;
        }
        pipeline = pipeline_build(input);
        if(pipeline == NULL){
            printf("%s\n",input);
        }else{
            int input_fd = 0; 

        while(pipeline->commands->next != NULL){
            input_fd = doCommand(pipeline, input_fd, 0, pipeline->is_background);
            pipeline->commands = pipeline->commands->next;
                
        }
            
        input_fd = doCommand(pipeline, input_fd,  1, pipeline->is_background);
        }
        

    }
    pipeline_free(pipeline);

    return 0;
}