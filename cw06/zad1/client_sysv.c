#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "ipc_util.h"

int running = 1;    // Change to 0 when it's time to end
int client_id = 0;  // 0 is forbidden as client_id; client_id == 0 indicates non-registered client
int client_qid;

// Protocol:
// Request: CID COMMAND_BODY   <QMSG_LEN bytes in total>
// Response: COMMAND_BODY      <QMSG_LEN bytes in total>

// Doing IO in signal handler is generally a bad idea, as it is not atomic and takes a long time
// Furthermore, library functions like printf() use static internal data structures which might lead to unexpected
// results should they get interrupted and called again in the handler
// I do not care
static void sig_handler(int signo) {
    if (signo == SIGINT) {
        fprintf(stderr, "Got SIGINT\n");
        running = 0;
    } else if (signo == SIGUSR1) {
        msgbuf mbuf;
        while (receive_command_noblock(client_qid, &mbuf, 0) != -1) {
            if (mbuf.mtype == STOP) {
                running = 0;
            } else {
                printf("%s\n", mbuf.mtext);
            }
        }
    }
}

void await_client_id(int client_qid) {
    msgbuf mbuf;
    receive_command(client_qid, &mbuf, ACK);
    if (running == 0) { // Received SIGINT while waiting for ACK message
        return;
    }
    printf("* Handshake complete, start typing commands:\n");
    client_id = (int) strtol(mbuf.mtext, NULL, 10);
}

// Evaluates one line and sends appropriate message to the server
void evaluate(int srv_qid, char *msg, char *console_dump) {
    if (strcmp(console_dump, "STOP\n") == 0) {
        running = 0;
    } else if (strcmp(console_dump, "LIST\n") == 0) {
        snprintf(msg, QMSG_LEN, "%d", client_id);
        send_command(srv_qid, LIST, msg);
    } else if (strncmp(console_dump, "ECHO ", 5) == 0) {
        snprintf(msg, QMSG_LEN, "%d %s", client_id, console_dump+5);
        send_command(srv_qid, ECHO, msg);
    } else if (strncmp(console_dump, "FRIENDS", 7) == 0) {    // Don't check for space at the end, the list may be empty
        snprintf(msg, QMSG_LEN, "%d %s", client_id, console_dump+8);
        send_command(srv_qid, FRIENDS, msg);
    } else if (strncmp(console_dump, "2ALL ", 5) == 0) {
        snprintf(msg, QMSG_LEN, "%d %s", client_id, console_dump+5);
        send_command(srv_qid, TOALL, msg);
    } else if (strncmp(console_dump, "2FRIENDS ", 9) == 0) {
        snprintf(msg, QMSG_LEN, "%d %s", client_id, console_dump+9);
        send_command(srv_qid, TOFRIENDS, msg);
    } else if (strncmp(console_dump, "2ONE ", 5) == 0) {
        snprintf(msg, QMSG_LEN, "%d %s", client_id, console_dump+5);
        send_command(srv_qid, TOONE, msg);
    } else if (strncmp(console_dump, "ADD ", 4) == 0) {
        snprintf(msg, QMSG_LEN, "%d %s", client_id, console_dump+4);
        send_command(srv_qid, ADD, msg);
    } else if (strncmp(console_dump, "DEL ", 4) == 0) {
        snprintf(msg, QMSG_LEN, "%d %s", client_id, console_dump+4);
        send_command(srv_qid, DEL, msg);
    } else if (strncmp(console_dump, "READ ", 5) == 0) {
        char *rel_path = strtok(console_dump + 5, "\n");
        char *abs_path = realpath(rel_path, NULL);
        if (abs_path == NULL) {
            error_and_end_errno();
        }
        FILE *comm_file = fopen(abs_path, "r");
        if (comm_file == NULL) {
            error_no_end("Couldn't open command file\n");
            error_no_end_errno();
            return;
        }
        ssize_t read = 0;
        size_t len = QMSG_LEN + 16; // Length of console_dump
        while (1) {                 // Will exit when EOF is read
            memset(msg, '\0', QMSG_LEN);
            memset(console_dump, '\0', len);
            read = getline(&console_dump, &len, comm_file); // TODO: Check if len wasn't modified
            if (read == -1) {
                break;
            }
            evaluate(srv_qid, msg, console_dump);
        }

        free(abs_path);
    } else if (strcmp(console_dump, "\n") == 0) {
            return;
    } else {
            error_no_end("Unknown command\n");
    }
}

int main() {
    // Turn off stdout buffering so messages are visible immediately
    if (setvbuf(stdout, NULL, _IONBF, 0)) {
        error_and_end_errno();
    }

    key_t srv_key = get_server_key();
    int srv_qid = msgget(srv_key, 0);    // Try to open server queue, must exist
    if (srv_qid == -1) {
        fprintf(stderr, "Failed to open server queue, make sure server is running.\n");
        error_and_end_errno();
    }

    // Create client queue
    client_qid = msgget(IPC_PRIVATE, 0666);
    if (client_qid == -1) {
        fprintf(stderr, "Couldn't create client queue\n");
        error_and_end_errno();
    }
    printf("* Opened client queue, qid: %d\n", client_qid);

    // Register own SIGINT and SIGUSR1 handler
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGUSR1, &sa, NULL) == -1) {
        error_and_end_errno();
    }

    // Inform server about new client
    char *init_msg = calloc('\0', QMSG_LEN);
    // Protocol requires to send some client_id, so 0 is sent (doesn't really matter)
    snprintf(init_msg, QMSG_LEN, "%d %d %d", 0, getpid(), client_qid);
    send_command(srv_qid, INIT, init_msg);
    free(init_msg);
    await_client_id(client_qid);        // Wait for server to acknowledge client's existence

    char msg[QMSG_LEN];                 // Message text only
    char console_dump[QMSG_LEN + 16];   // COMMAND + msg; read from console
    while (running) {
        memset(msg, '\0', QMSG_LEN);
        memset(console_dump, '\0', QMSG_LEN + 16);

        fgets(console_dump, QMSG_LEN+16, stdin);
        if (!running) {                 // SIGINT might have occurred
            break;
        }

        if (*console_dump != '\0') {    // If signal occurred console_dump is empty
            evaluate(srv_qid, msg, console_dump);
        }
    }

    // Inform server about client exit
    // client_id might equal 0 if client couldn't register itself, in which case it doesn't need to unregister
    if (client_id) {
        char *exit_msg = calloc('\0', QMSG_LEN);
        snprintf(exit_msg, QMSG_LEN-1, "%d", client_id);
        send_command(srv_qid, STOP, exit_msg);
        free(exit_msg);
    }

    // Destroy client's own queue
    if (msgctl(client_qid, IPC_RMID, NULL) == -1) {
        error_and_end_errno();
    }

    return 0;
}
