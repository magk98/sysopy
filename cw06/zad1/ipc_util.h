#ifndef SYSOPY_LAB6_IPC_UTIL_H
#define SYSOPY_LAB6_IPC_UTIL_H

#include <stdio.h>


#define FTOK_PATH "$HOME"
#define FTOK_ID 42
#define MAX_CLIENTS 16
// For ease of implementation all messages in queue are of fixed length (QMSG_LEN), padded with '\0'
#define QMSG_LEN 128

void error_and_end(const char *);
void error_and_end_errno();
void error_no_end(const char *);
void error_no_end_errno();

char *read_file(FILE *);
int count_lines(FILE *);

// Command type gets translated to message type to mark its priority
typedef enum command {
    INIT = 1,
    ACK = 2,
    ECHO = 3,
    LIST = 4,
    FRIENDS = 5,
    TOALL = 6,
    TOFRIENDS = 7,
    TOONE = 8,
    STOP = 9,
    ADD = 10,
    DEL = 11
} command;

typedef struct msgbuf {
    long mtype;
    char mtext[QMSG_LEN];
} msgbuf;

key_t get_server_key();
void send_command(int qid, command cmd, char *msg);
ssize_t receive_command(int qid, msgbuf *mbuf, long msgtype);
ssize_t receive_command_noblock(int qid, msgbuf *mbuf, long msgtype);

void pos_send_command(int qid, command cmd, char *msg);
ssize_t pos_receive_command(int qid, char *mbuf);
ssize_t pos_receive_command_noblock(int qid, char *mbuf);

#endif //SYSOPY_LAB6_IPC_UTIL_H
