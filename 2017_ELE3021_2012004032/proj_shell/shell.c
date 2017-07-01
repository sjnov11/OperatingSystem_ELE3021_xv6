#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


/**
  * MakeTokenizedList function is used to tokenize given string 
    and append each token to the token_list.

    @param[in]  string : given string.
    @           token : string which will use to tokenize.
    @           token_list : list of token.
    @return     Length of token_list.

  */
int MakeTokenizedList(char *string, char *token, char *token_list[]) {
    int index = 1;
    token_list[0] = strtok(string, token);
    if (token_list[0] == NULL) {
        return 0;
    }
    while (1) {
        token_list[index] = strtok(NULL, token);
        if (token_list[index] == NULL) {
            break;
        }
        index++;
    }
    return index;
}


/**
  * ExecuteCommands function is used to execute multiple commands which user entered 
    It seperates each commands by ";" and seperates each arguments of command by " "
    It creates child processes as many as commands 
    Child processes execute the commands.
    Calling process just creates child processes and wait until all of them have done
    
    @param[in]  full_command : given commands string.
    @return     0 : if full_command has no command input.
                -1 : if it has exit command.
                1 : else
  */
int ExecuteCommands(char *full_command) {
    char *command[1000];        // storage of tokenize full command by ";"
    char *part[1000];           // storage of tokenize command by " "
    int command_counter, part_counter;
    int i;
    int flag_exit = 0;          // flag for exit 

    // tokenize full command by ";"
    command_counter = MakeTokenizedList(full_command, ";", command);
    if (command_counter == 0) {
        return 0;
    }

    // execute child processor the number of commands times.
    for (i = 0; i < command_counter; i++) {            
        // tokenize user command by " "        
        part_counter = MakeTokenizedList(command[i], " ", part);           
        if (part_counter == 0) {
            return 0;
        }       

        // if there is command "exit", do not create child process for command 
        // and set flag for exit 1
        if (strcmp(part[0], "exit") == 0) {
            flag_exit = 1;
            continue;
        }
            
            
        // create multiple child processes
        int rc = fork();
        if (rc < 0) {
            fprintf(stderr, "fork falied\n");
            exit(1);
        } else if (rc == 0) {                   // child process            
            if (execvp(part[0], part) < 0) {     // execute command
                // Error handler
                // if errno is ENOENT which means no such file or dir
                // to execute arg1 program, but it means
                // there is no such command in here.. 
                printf("%s: %s\n", part[0], 
                        errno == ENOENT ? "Command not found"
                        : strerror(errno));
                exit(errno);
            }
        } 
    }
    // Calling Processor waits for child process
    while (wait(NULL) > 0);
    // if there is exit command return -1
    if (flag_exit == 1) {
        return -1;
    }
    // After wait, Parent Processor can do anything
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        char full_command[1000];      // storage of user command
        
        while (1) {
            printf("prompt> ");
            fgets(full_command, sizeof(full_command), stdin);
            full_command[strlen(full_command)-1] = '\0';    // remove '\n' 
            if (ExecuteCommands(full_command) == -1) {
                break;   
            }
        }
    } else if (argc > 1) {
        FILE *fp;
        char full_command[1000];
        int i;

        for (i = 1; i < argc; i++) {
            fp = fopen(argv[i], "r");
            if (fp == NULL) {
                printf("Can not find the file..\n");
                return -1;
            }
            while (fgets(full_command, sizeof(full_command), fp) != NULL) {
                full_command[strlen(full_command) - 1] = '\0';  // remove '\n' 
                printf("%s\n", full_command);
                ExecuteCommands(full_command);
            }
        }
   } 
   return 0;
}
