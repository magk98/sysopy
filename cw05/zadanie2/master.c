#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define LINE_MAX 256

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Pass the name of the pipe");
        exit(1);
    }

    char line[LINE_MAX];
    if (mkfifo(argv[1], S_IWUSR | S_IRUSR) < 0) {
        printf("Error.\n");
        exit(1);
    }

    FILE *pipe = fopen(argv[1], "r");
    if (!pipe) {
        printf("Error.\n");
        exit(1);
    }

    while (fgets(line, LINE_MAX, pipe) != NULL) {
        fputs(line, stdout);
        fflush(stdout);
    }

    fclose(pipe);
    return 0;
}