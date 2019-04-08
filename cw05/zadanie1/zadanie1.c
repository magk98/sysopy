#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <memory.h>
#include <sys/types.h>

#define max_parameters 64
#define max_line 256
#define max_commands 100


char* trim_line(char* pipe){
    char* buffer = malloc(sizeof(char) * 200);
    char* i = pipe;
    while(*i == ' ') i++;
    int j = 0;
    while(*i != 0){
        while((*i != 0) && (*i != ' ')){
            buffer[j++] = *i;
            i++;
        }
        if(*i == ' '){
            while(*i == ' ') i++;
            if(*i != 0){
                buffer[j++] = ' ';
            }
        }
    }
    buffer[j+1] = 0;
    return buffer;
}

char** parse_arguments(char *line){
    int size = 0;
    char** args = NULL;
    char delimiters[3] = {' ', '\n', '\t'};
    char *delim = strtok(line, delimiters);
    while(delim != NULL){
        size++;
        args = realloc(args, sizeof(char*) * size);
        if(args == NULL){
            exit(EXIT_FAILURE);
        }
        args[size-1] = delim;
        delim = strtok(NULL, delimiters);
    }
    args = realloc(args, sizeof(char*) * (size+1));
    if(args == NULL){
        exit(EXIT_FAILURE);
    }
    args[size] = NULL;

    return args;
}

int execute_line(char* params){
    int command_number = 0;
    int pipes[2][2];
    char* cmds[max_commands];

    while((cmds[command_number] = strtok(command_number == 0 ? params : NULL, "|")) != NULL){
        command_number++;
    };
    int i;
    for(i = 0; i < command_number; i++){

        if(i > 0){
            close(pipes[i%2][0]);
            close(pipes[i%2][1]);
        }

        if(pipe(pipes[i%2]) == -1){
            printf("Error.\n");
            exit(EXIT_FAILURE);
        }
        pid_t child = fork();
        if(child == 0){
            char ** exec_params = parse_arguments(trim_line(cmds[i]));

            if(i != command_number - 1){
                close(pipes[i%2][0]);
                if(dup2(pipes[i%2][1], STDOUT_FILENO) < 0)
                    exit(EXIT_FAILURE);
            }
            if(i != 0){
                close(pipes[(i + 1) % 2][1]);
                if(dup2(pipes[(i + 1) % 2][0], STDOUT_FILENO) < 0)
                    close(EXIT_FAILURE);
            }
            execvp(exec_params[0], exec_params);

            exit(EXIT_SUCCESS);
        }
    }
    close(pipes[i % 2][0]);
    close(pipes[i % 2][1]);
    wait(NULL);
    exit(0);
}

int main(int argc, char** argv){
    if(argc < 2){
        printf("Not enough arguments.\n");
        exit(1);
    }
    FILE* file = fopen(argv[1], "r");
    if (!file) {
        printf("Error while opening file.\n🙄");
        return 1;
    }
    char temp_registry[max_line];
    char *params[max_parameters];
    int argument_number = 0;
    while(fgets(temp_registry, max_line, file)){
        argument_number = 0;
        pid_t pid = fork();
        if(pid == 0) {
            execute_line(temp_registry);
            exit(EXIT_SUCCESS);
        }
        int status;
        wait(&status);
        if (status) {
            printf( "Error while executing.\n");
            for (int i = 0; i < argument_number; i++) {
                printf("%s ", params[i]);
            }
            return 1;
        }
    }
    fclose(file);
    return 0;
}