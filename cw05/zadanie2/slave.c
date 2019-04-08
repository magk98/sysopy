#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define LINE_MAX 256

int main(int argc, char **argv) {
    srand(time(NULL));
    if (argc != 3) {
        printf("Pass pipe name and integer");
        exit(1);
    }

    char date_res[LINE_MAX];
    char pipe_string[LINE_MAX + 20];
    int count = (int) strtol(argv[2], NULL, 10);
    FILE *pipe = fopen(argv[1], "w");
    if (!pipe) {
        printf("Error.\n");
        exit(1);
    }

    printf("Slave PID: %d\n", (int) getpid());
    for (int i = 0; i < count; i++) {
        FILE *date = popen("date", "r");
        fgets(date_res, LINE_MAX, date);
        if (sprintf(pipe_string, "%d: %s", (int) getpid(), date_res) < 0) {
            printf("Problem while building string");
            exit(1);
        }
        if (fwrite(pipe_string, sizeof(char), strlen(pipe_string), pipe) != strlen(pipe_string)) {
            printf("Failed to write the whole pipe string");
        }
        fflush(pipe);
        sleep((rand() % 4) + 2);
    }

    fclose(pipe);
    return 0;
}