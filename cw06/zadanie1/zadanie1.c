#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <memory.h>
#include <sys/types.h>

#define max_parameters = 64;


void perform_command(char* commands){

}

char* trim_commands(char* commands){
    char* copy = malloc(sizeof(commands));
    strcpy(copy, commands);
    char** result = malloc(sizeof(copy));
    char* token = strtok(copy, "|");
    int i = 0;
    while (token) {
        //printf("token: %s\n", token);
        result[i++] = *token;
        token = strtok(NULL, "|");
    }
    result[i] = '0';
    return *result;
}

char* trim_line(char* pipe){
    char* buffer = malloc(sizeof(char) * 100);
    char* i = pipe;
    while(*i == ' ') i++;
    int j = 0;
    while(*i != 0){
        while(*i != 0 && *i != ' '){
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
    for(int k = 0; k < j; k++) {
        printf("%c\n", buffer[k]);
    }
    return buffer;
}


char* read_example(char* file1){
    FILE* f1 = fopen(file1, "r");
    char* buffer = malloc(sizeof(char) * 50);
    if (!f1) {
        printf("Error while opening the file.\n🙄");
        exit(1);
    }
    if(!buffer){
        printf("Error while allocating memory for buffer.\n");
        exit(1);
    }
    fseek(f1, 0, SEEK_END);
    size_t file_size = ftell(f1);
    fseek(f1, 0, SEEK_SET);
    int read_char = fread(buffer, sizeof(char), file_size, f1);
    if(read_char != file_size){
        printf("Wrong characters number read.\n");
        exit(1);
    }

    printf("Char read: %d\n",read_char);
    return trim_commands(buffer);
}


int main(int argc, char** argv){
    if(argc < 2){
        printf("Not enough arguments.\n");
        exit(1);
    }

    int fd[2];
    pipe(fd);
    pid_t pid = fork();
    char* buffer = malloc(sizeof(char) * 50);
    if(pid == 0){
        buffer = read_example(argv[1]);
printf("%c\n",buffer);
        //exit(EXIT_SUCCESS);
    }

    return 0;
}