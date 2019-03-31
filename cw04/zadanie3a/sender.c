#define _GNU_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

int received_signal_number = 0;
int send_signal_number = 0;


void signal_sigusr1(int sig_no, siginfo_t *info, void *ucontext){
    received_signal_number++;
}


void signal_sigusr2(int sig_no,siginfo_t *info, void *ucontext){
    if(info->si_code == SI_QUEUE){
        printf("Catcher received : %d\n", info->si_value.sival_int);
    }

    printf("Sent: %d signals\nReceived: %d signals\n", send_signal_number, received_signal_number);
    exit(0);
}

void senderHandler(pid_t child, int mode) {
    union sigval value;
    for(int i = 0; i < send_signal_number; i++){
        if(mode == 0){
            if(kill(child,SIGUSR1) != 0 )
                printf("Error while sending SIGUSR1\n");
        } else if(mode == 1){
            if(sigqueue(child, SIGUSR1, value) != 0)
                printf("Error while sending SIGUSR1\n");
        } else if(mode == 2){
            if(kill(child,SIGRTMIN + 12) != 0){}
        }
    }

    if(mode == 0){
        if (kill(child,SIGUSR2) != 0 )
            printf("Error while sending SIGUSR2\n");
    } else if(mode == 1){
        if(sigqueue(child,SIGUSR2,value) != 0)
            printf("Error while sending SIGUSR2\n");
    }else if(mode == 2){
        if(kill(child,SIGRTMIN + 13) != 0)
            printf("Error while sending SIGUSR2\n");
    }

}


int main(int argc, char** argv){
     int mode;
    if(argc != 4){
        printf("Wrong argument number, nothing happened.\n");
        exit(1);
    }
    char* tryb = argv[2];
    if(atoi(argv[1]) < 1){
        printf("Less than 1 signal, nothing happened.\n");
        exit(1);
    }
    if(strcmp(tryb, "KILL") != 0 && strcmp(tryb, "SIGQUEUE") != 0 && strcmp(tryb, "SIGRT") != 0){
        printf("Wrong mode, nothing happened.\n");
        exit(1);
    }
    else if(strcmp(tryb, "KILL") == 0) mode = 0;
    else if(strcmp(tryb, "SIGQUEUE") == 0) mode = 1;
    else if(strcmp(tryb, "SIGRT") == 0) mode = 2;

    struct sigaction act;

    act.sa_flags = SA_SIGINFO;

    act.sa_sigaction = signal_sigusr1;
    sigfillset(&act.sa_mask);
    sigdelset(&act.sa_mask, SIGUSR1);
    sigdelset(&act.sa_mask, SIGRTMIN + 12);
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGRTMIN + 12, &act, NULL);

    act.sa_sigaction = signal_sigusr2;
    sigfillset(&act.sa_mask);
    sigdelset(&act.sa_mask, SIGUSR2);
    sigdelset(&act.sa_mask, SIGRTMIN + 13);
    sigaction(SIGUSR2, &act, NULL);
    sigaction(SIGRTMIN + 13, &act, NULL);

    send_signal_number = atoi(argv[1]);;
    senderHandler(atoi(argv[3]), mode);

    while(1){
        sleep(1);
    }
  
  return 0;
}
