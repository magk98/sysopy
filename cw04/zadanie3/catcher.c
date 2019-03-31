#define _GNU_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

volatile pid_t child;
int received_signal_number_catcher = 0;
int received_signal_number_sender = 0;


void sender(pid_t catcher, int domain_signal_number, char* tryb){
    printf("A");
    sigset_t user_signals;
    sigemptyset(&user_signals);
    sigaddset(&user_signals, SIGUSR1);
    sigaddset(&user_signals, SIGUSR2);
    printf("B");
    for(int i = 0; i < domain_signal_number; i++) {
        printf("C");
        kill(catcher, SIGUSR1);
    }
    printf("D");
    kill(catcher, SIGUSR2);
    printf("Sender send %d signals SIGUSR1 and one SIGUSR2.\n", domain_signal_number);
    int status = 0;
    waitpid(catcher, &status, 0);
    if (WIFEXITED(status))
        received_signal_number_catcher = WEXITSTATUS(status);
    else {
        printf("Error with termination of Child!\n");
        exit(1);
    }
}

void senderHandler(int signum, siginfo_t *info, void *context) {
    if (info->si_pid != child) return;
    if (signum == SIGUSR1) {
        received_signal_number_sender++;
        printf("Sender: Received SIGUSR1 from Catcher.\n");
    }
}

void childHandler(int sig_no, siginfo_t *info, void *context){
    printf("eee\n");
    if (info->si_pid != getppid())
        return;
    if(sig_no == SIGUSR1) {
        received_signal_number_catcher++;
        printf("macarena\n");
    }
    if(sig_no == SIGUSR2){
        for(int i = 0; i < received_signal_number_catcher; i++){
            kill(getppid(), SIGUSR1);
        }
        printf("Catcher received %d signals. Sending signals back.\n", received_signal_number_catcher);
        exit((unsigned) received_signal_number_catcher);
    }
}

void childProcess() {
    printf("dziecko1\n");
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = childHandler;

    if (sigaction(SIGUSR1, &act, NULL) == -1){
        printf("Can't catch SIGUSR1.\n");
        exit(1);
    }
    if (sigaction(SIGUSR2, &act, NULL) == -1){
        printf("Can't catch SIGUSR2.\n");
        exit(1);
    }

    while (1) {
        sleep(1);
    }
}

int main(int argc, char** argv){
    if(argc != 3){
        printf("Wrong argument number, nothing happened.\n");
        exit(1);
    }
    int domain_signal_number = atol(argv[1]);
    char* tryb = argv[2];
    if(domain_signal_number < 1){
        printf("Less than 1 signal, nothing happened.\n");
        exit(1);
    }
    if(strcmp(tryb, "KILL") != 0 && strcmp(tryb, "SIGQUEUE") != 0 && strcmp(tryb, "SIGRT") != 0){
        printf("Wrong mode, nothing happened.\n");
        exit(1);
    }
    printf("Catcher PID: %d\n", getpid());
    child = fork();
    printf("Sender PID: %d\n", child);
    if(child < 0){
        printf("Error while forking, nothing happened.\n");
        exit(1);
    }
    if(child == 0){
        printf("sialala == 0\n");
        sender(getpid(), domain_signal_number, tryb);
    }
    if(child > 0){
        printf("sialala > 0\n");
        childProcess();
    }



    return 0;
}