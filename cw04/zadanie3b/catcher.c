#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

static short mode;
static int received;

void signal_sigusr1(int no, siginfo_t *info, void *ucontext){
  received+=1;
  pid_t pid = info->si_pid;
  union sigval value;
  if(mode == 0){
    if (kill(pid,SIGUSR1) != 0 )
     printf("ERROR WHILE SENDING SIGUSR1\n");
  }else if( mode == 1 ){
    if(sigqueue(pid, SIGUSR1, value) != 0)
      printf("ERROR WHILE SENDING SIGUSR1\n");
  }else if (mode == 2){
    if(kill(pid,SIGRTMIN + 12) != 0){}
  }
}

void signal_sigusr2(int no, siginfo_t *info, void *ucontext){
  pid_t pid = info->si_pid;
  union sigval value;
  value.sival_int=received;
  if(mode == 0){
    if (kill(pid,SIGUSR2) != 0 )
     printf("ERROR WHILE SENDING SIGUSR1\n");
  }else if( mode == 1 ){
    if(sigqueue(pid, SIGUSR2, value) != 0)
      printf("ERROR WHILE SENDING SIGUSR1\n");
  }else if (mode == 2){
    if(kill(pid,SIGRTMIN + 13) != 0){}
  }
  received=0;
}

int main(int argc, char** argv){

  printf("CATCHER PID :%d\n",getpid());

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
 if(strcmp(argv[1],"KILL") ==0){
   mode=0;
 } else if(strcmp(argv[1],"SIGQUEUE")==0){
   mode=1;
 } else if(strcmp(argv[1],"SIGRTL") ==0){
   mode=2;
 }else{
   printf("Wrong value of first argument\n");
   printf("[MODE {KILL,SIGQUEUE,SIGRTL}]\n");
 }

 while(1){}
 return 0;
}
