#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

int is_awaiting = 0;
int is_dead_process = 0;
pid_t pid = 0;

void au(int sig_no){
    if(is_awaiting == 0) {
        printf("\nOczekuję na CTRL+Z - kontynuacja albo CTR+C - zakonczenie programu\n");
    }
    is_awaiting = is_awaiting == 1 ? 0 : 1;
}

void init_signal(int sig_no){
    printf("\nOdebrano sygnał SIGINT.\n");
    exit(EXIT_SUCCESS);
}


int main(int argc, char** argv){
    struct sigaction act;
    act.sa_handler = au;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    pid = fork();

    if(pid == 0){
        execl("./skrypt.sh", "./skrypt.sh", NULL);
        exit(EXIT_SUCCESS);
    }

    while(1){
        sigaction(SIGTSTP, &act, NULL);
        signal(SIGINT, init_signal);

        if(is_awaiting == 0) {
            if(is_dead_process){
                is_dead_process = 0;
                pid = fork();
                if(pid == 0){
                    execl("./skrypt.sh", "./skrypt.sh", NULL);
                    exit(EXIT_SUCCESS);
                }
            }
        }
        else{
            if(is_dead_process == 0){
                kill(pid, SIGKILL);
                is_dead_process = 1;
            }
        }
    }
}