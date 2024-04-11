#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_SUB_COMMANDS 5
#define MAX_ARGS 10

// define subcommand data structure
struct SubCommand {
    char *line;
    char *argv[MAX_ARGS];
};

// define command data structure
struct Command {
    struct SubCommand sub_commands[MAX_SUB_COMMANDS];
    char *stdin_redirect;
    char *stdout_redirect;
    int background;
    int num_sub_commands;
};

// function to read arguments from input string
void ReadArgs(char *in, char **argv, int size) {

    // declare token variable
    char *token;

    // tokenize input string using space as delimiter
    token = strtok(in, " ");

    // loop through tokens and add to argv array
    int argc = 0;
    while (token != NULL && argc < size) {

        // add token to argv array
        argv[argc] = strdup(token);
        argc++;

        // get next token
        token = strtok(NULL, " ");
    }

    // null terminate argv array
    argv[argc] = NULL;
    

}

// function to print out arguments
void PrintArgs(char **argv) {
    int iter = 0;
    // loop through argv array and print out by value until null value reached
    while (argv[iter] != NULL) {
        printf("argv[%d] = '%s'\n", iter, argv[iter]);
        iter++;
    }
}

void ReadRedirectsAndBackground(struct Command *command) {
    // set stdin_redirect, stdout_redirect, and background fields of command to NULL and 0,
    // values will be overwritten if they are found in the subcommands
    command->stdin_redirect = NULL;
    command->stdout_redirect = NULL;
    command->background = 0;

    // loop through subcommands in reverse order
    for (int i = command->num_sub_commands - 1; i >= 0; i--) {

        // declare iterator value
        int argc = 0;

        // loop through argument arrays until null value reached
        while (command->sub_commands[i].argv[argc] != NULL) {

            // check if argument is "<", set stdin_redirect to next argument and set current argument to NULL.
            if (strcmp(command->sub_commands[i].argv[argc], "<") == 0) {
                command->stdin_redirect = command->sub_commands[i].argv[argc + 1];
                command->sub_commands[i].argv[argc] = NULL;

            // check if argument is ">", set stdout_redirect to next argument and set current argument to NULL.
            } else if (strcmp(command->sub_commands[i].argv[argc], ">") == 0) {
                command->stdout_redirect = command->sub_commands[i].argv[argc + 1];
                command->sub_commands[i].argv[argc] = NULL;

            // check if argument is "&", set background to 1 and set current argument to NULL.
            } else if (strcmp(command->sub_commands[i].argv[argc], "&") == 0) {
                command->background = 1;
                command->sub_commands[i].argv[argc] = NULL;
            }

            // increment argc
            argc++;
        }
    }
}

// function to read command from input string
void ReadCommand(char *line, struct Command *command) {
    
        // declare token variable
        char *token;
    
        // tokenize input string using | as delimiter
        token = strtok(line, "|");
    
        // loop through tokens and add to argv array
        int argc = 0;
        while (token != NULL && argc < MAX_SUB_COMMANDS) {
    
            // add token to argv array
            command->sub_commands[argc].line = strdup(token);
            argc++;
    
            // get next token
            token = strtok(NULL, "|");
        }
    
        // set number of sub commands
        command->num_sub_commands = argc;

        // call ReadArgs on each subcommand line
        for (int i = 0; i < command->num_sub_commands; i++) {
            ReadArgs(command->sub_commands[i].line, command->sub_commands[i].argv, MAX_ARGS);
        }

        // call ReadRedirectsAndBackground to set stdin_redirect, stdout_redirect, 
        // and background fields of command
        ReadRedirectsAndBackground(command);

}


// @todo: no need for this function
// function to print out command data structure
void PrintCommand(struct Command *command) {

    // loop through subcommands and print out by value
    for (int i = 0; i < command->num_sub_commands; i++) {
        printf("Command %d:\n", i);
        PrintArgs(command->sub_commands[i].argv);
        printf("\n");
    }

    // print out stdin_redirect value
    if (command->stdin_redirect != NULL) {
        printf("Redirect stdin: %s\n", command->stdin_redirect);
    } 
    else {
        printf("Redirect stdin: NULL\n");
    }

    // print out stdout_redirect value
    if (command->stdout_redirect != NULL) {
        printf("Redirect stdout: %s\n", command->stdout_redirect);
    } 
    else {
        printf("Redirect stdout: NULL\n");
    }

    // print out background, yes or no based on value
    if (command->background == 1) {
        printf("Background: yes\n");
    } 
    else {
        printf("Background: no\n");
    }

}

void handle_sigchld(int sig) {
    // wait for all dead processes
    // we use a non-blocking call to waitpid (WNOHANG) to avoid blocking if a child was created
    // with fork() but has not yet exited.
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
}


void ExecuteCommand(struct Command *command){
    int fd[2];
    pid_t pid;
    int in = 0;
    int i;

    for (i = 0; i < command->num_sub_commands; i++) {
        pipe(fd);
        pid = fork();
        if (pid == 0) {
            if (i == 0 && command->stdin_redirect != NULL) {

                // open file in read-only mode
                int fd = open(command->stdin_redirect, O_RDONLY);

                // check if file opened successfully
                if (fd == -1) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }

                // redirect stdin to file
                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

                // close file
                close(fd);
            }

                    // handle output redirection
            if (i == command->num_sub_commands - 1 && command->stdout_redirect != NULL) {

                // open file in write-only mode, create if it doesn't exist, truncate if it does
                int fd = open(command->stdout_redirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);

                // check if file opened successfully
                if (fd == -1) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }

                // redirect stdout to file
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

                // close file
                close(fd);
            }

            dup2(in, 0); //change the input according to the old one
            if (i < command->num_sub_commands - 1) {
                dup2(fd[1], 1);
            }
            close(fd[0]);
            execvp(command->sub_commands[i].argv[0], command->sub_commands[i].argv);

            // if execvp returns, an error occurred
            char *err = command->sub_commands[i].argv[0];
            printf("%s: Command not found\n", err);
            exit(1);
        } else {
            if(command->background){
                if(i == command->num_sub_commands - 1){
                    printf("[%d]\n", pid);
                }
            } else{
                waitpid(pid, NULL, 0);
            }
            close(fd[1]);
            in = fd[0];
        }
    }
}

int main() {
    
    // declare variables
    char line[100];
    struct Command command;

    // loop until user enters "exit"
    while (1) {
        // prompt user for input
        printf("$ ");

        // read input from user
        fgets(line, sizeof(line), stdin);

        // if we enter a enter key, continue
        if (line[0] == '\n') {
            continue;
        }

        // remove newline character from input
        line[strcspn(line, "\n")] = '\0';

        // check if user entered "exit"
        if (strcmp(line, "exit") == 0) {
            break;
        }

        // read command from input
        ReadCommand(line, &command);

        //PrintCommand(&command);

        ExecuteCommand(&command);

        // kill all zombies
        signal(SIGCHLD, handle_sigchld);
    }

        return 0;
}
