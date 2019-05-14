#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

#include "ipc_util.h"

typedef struct client {
    int client_id;
    int client_qid;
    pid_t client_pid;
    int friend_count;
    struct client **friends;
} client;

int running = 1;

// Protocol:
// Request: CID COMMAND_BODY   <QMSG_LEN bytes in total>
// Response: COMMAND_BODY      <QMSG_LEN bytes in total>

static void sigint_handler(int signo) {
    if (signo == SIGINT) {
        fprintf(stderr, "Got SIGINT\n");
        running = 0;
    }
}

void add_client(int client_qid, pid_t client_pid, int avail_id, client *c) {
    if (avail_id > MAX_CLIENTS) {
        fprintf(stderr, "* Too many clients. Can't add another. Queue id: %d, Pid: %d\n", client_qid, client_pid);
        kill(client_pid, SIGINT);
        return;
    }
    c[avail_id].client_id = avail_id;
    c[avail_id].client_qid = client_qid;
    c[avail_id].client_pid = client_pid;
    c[avail_id].friend_count = 0;
    c[avail_id].friends = calloc(0, MAX_CLIENTS * sizeof(client*));
}

void add_friend(client *c, client *friend) {
    if (c->friend_count >= MAX_CLIENTS) {
        return;
    }
    c->friends[c->friend_count] = friend;
    c->friend_count++;
}

void delete_friend(client *c, client *friend) {
    for (int i = 0; i < c->friend_count; i++) {
        if (c->friends[i] && c->friends[i] == friend) {
            c->friends[i] = NULL;
        }
    }
}

void remove_client(client *cs, int client_id, int num_of_clients) {
    for (int i = 0; i < num_of_clients; i++) {
        if (cs[i].client_id == client_id) {     // client_id == 0 marks slot as unused
            cs[i].client_id = 0;
            free(cs[i].friends);
        }
    }
}

client *get_client_from_cid(client *cs, int cid, int last) {
    for (int i = 0; i < last; i++) {
        if (cs[i].client_id == cid) {
            return (cs + i);
        }
    }
    return NULL;
}

int main() {
    // Turn off stdout buffering so messages are visible immediately
    if (setvbuf(stdout, NULL, _IONBF, 0)) {
        error_and_end_errno();
    }

    key_t srv_key = get_server_key();
    int srv_qid = msgget(srv_key, IPC_CREAT | 0666);        // Try to open queue
    if (srv_qid == -1) {
        error_and_end_errno();
    }
    printf("* Opened server queue, qid: %d\n", srv_qid);

    client clients[MAX_CLIENTS + 1];                        // One slot more because numeration begins with 1
    memset(clients, 0, (MAX_CLIENTS+1) * sizeof(client));
    int curr_slot = 1;                                      // Begin with 1 so we have strictly positive ids
                                                            // We can use client_is == 0 to mark unused slots
    // Register own SIGINT handler
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        error_and_end_errno();
    }

    msgbuf request;
    char response[QMSG_LEN];
    while (running) {
        memset(request.mtext, '\0', QMSG_LEN);
        memset(response, '\0', QMSG_LEN);

        // Perform series on non-blocking calls to receive top priority commands
        ssize_t received = receive_command_noblock(srv_qid, &request, STOP);
        if (received == -1) {
            if (errno == ENOMSG) {
                received = receive_command_noblock(srv_qid, &request, LIST);
                if (received == -1) {
                    if (errno == ENOMSG) {
                        received = receive_command_noblock(srv_qid, &request, FRIENDS);
                        if (received == -1) {
                            if (errno == ENOMSG) {
                                receive_command(srv_qid, &request, 0);
                                if (!running) {     // During the blocking receive_command a SIGINT might have occurred
                                    break;
                                }
                            } else {
                                error_and_end_errno();
                            }
                        }
                    } else {
                        error_and_end_errno();
                    }
                }
            } else {
                error_and_end_errno();                
            }
        }

        int cid = (int) strtol(strtok(request.mtext, " "), NULL, 10);
        char *cmd_body = strtok(NULL, "\n");
        client *cl;

        cl = get_client_from_cid(clients, cid, curr_slot);
        if (cl == NULL && request.mtype != INIT) {
            fprintf(stderr, "Malformed client id: %d\n", cid);
            continue;
        }

        if (request.mtype == STOP) {
            if (cid < curr_slot) {
                remove_client(clients, cid, curr_slot);    // Search clients table up to the curr_slot index
                printf("* Removing client with id: %d\n", cid);
            } else {
                fprintf(stderr, "* Tried to remove illegal client_id: %d\n", cid);
            }
        } else if (request.mtype == INIT) {
            pid_t cpid = (pid_t) strtol(strtok(cmd_body, " "), NULL, 10);
            int client_qid = (int) strtol(strtok(NULL, "\0"), NULL, 10);
            if (client_qid == -1) {
                error_no_end("* Unable to open client queue. Invalid key.\n");
                continue;
            }
            add_client(client_qid, cpid, curr_slot, clients);
            snprintf(response, QMSG_LEN, "%d", curr_slot);
            curr_slot++;
            send_command(client_qid, ACK, response);
            printf("* Handshake with new client: client_id: %d, client_qid: %d\n", curr_slot - 1, client_qid);
        } else if (request.mtype == ECHO) {
            snprintf(response, QMSG_LEN, "%s", cmd_body);
            send_command(cl->client_qid, ECHO, response);
            kill(cl->client_pid, SIGUSR1);
            printf("* Echoing message: '%s' to client: %d\n", response, cl->client_id);
        } else if (request.mtype == LIST) {
            int written = 0;
            for (int i = 1; i < curr_slot; i++) {
                if (clients[i].client_id != 0) {
                    if (written > QMSG_LEN) {
                        break;                  // In case response gets too long
                    }
                    written = snprintf(response + written,
                                       (size_t) QMSG_LEN - written,
                                       "Client id: %d, Client qid: %d\n", clients[i].client_id, clients[i].client_qid);
                }
            }
            send_command(cl->client_qid, LIST, response);
            kill(cl->client_pid, SIGUSR1);
            printf("* Listing clients for client_id: %d\n", cid);
        } else if (request.mtype == FRIENDS || request.mtype == ADD) {
            if (request.mtype == FRIENDS) {
                memset(cl->friends, 0, MAX_CLIENTS * sizeof(client *));
                cl->friend_count = 0;
            }
            char *str_id = strtok(cmd_body, " ");
            while (str_id != NULL) {
                int friend_id = (int) strtol(str_id, NULL, 10);
                client *friend = get_client_from_cid(clients, friend_id, curr_slot);
                if (friend == NULL) {
                    error_no_end("* Couldn't add non-existent friend\n");
                    continue;
                }
                add_friend(cl, friend);
                str_id = strtok(NULL, " ");
            }
            printf("* Adding friends for client_id: %d\n", cid);
        } else if (request.mtype == TOALL) {
            snprintf(response, QMSG_LEN, "[%d] %s", cl->client_id, cmd_body);
            for (int i = 1; i < curr_slot; i++) {
                if (clients[i].client_id != 0 && clients[i].client_id != cl->client_id) {
                    send_command(clients[i].client_qid, TOALL, response);
                    kill(clients[i].client_pid, SIGUSR1);
                }
            }
            printf("* Forwarding message to all from client_id: %d\n", cid);
        } else if (request.mtype == TOFRIENDS) {
            snprintf(response, QMSG_LEN, "[%d] %s", cl->client_id, cmd_body);
            for (int i = 0; i < cl->friend_count; i++) {
                if (cl->friends[i] && cl->friends[i]->client_id != 0) {
                    send_command(cl->friends[i]->client_qid, TOFRIENDS, response);
                    kill(cl->friends[i]->client_pid, SIGUSR1);
                }
            }
            printf("* Forwarding message to friends from client_id: %d\n", cid);
        } else if (request.mtype == TOONE) {
            int target_id = (int) strtol(strtok(cmd_body, " "), NULL, 10);
            client *target = get_client_from_cid(clients, target_id, curr_slot);
            if (target == NULL) {
                error_no_end("* Cannot send message to non-existent client\n");
                continue;
            }
            snprintf(response, QMSG_LEN, "[%d] %s", cl->client_id, strtok(NULL, "\n"));
            send_command(target->client_qid, TOONE, response);
            kill(target->client_pid, SIGUSR1);
            printf("* Forwarding message to client_id: %d from client_id: %d\n", target_id, cid);
        } else if (request.mtype == DEL) {
            char *str_id = strtok(cmd_body, " ");
            while (str_id != NULL) {
                int friend_id = (int) strtol(str_id, NULL, 10);
                client *friend = get_client_from_cid(clients, friend_id, curr_slot);
                if (friend == NULL) {
                    error_no_end("* Couldn't delete non-existent friend\n");
                    continue;
                }
                delete_friend(cl, friend);
                str_id = strtok(NULL, " ");
            }
            printf("* Deleting friends for client_id: %d\n", cid);
        }  else {
            fprintf(stderr, "* Malformed command: %s\n", request.mtext);
        }
    }

    // Inform clients about exit and clean up server queue
    fprintf(stderr, "* Informing clients about exit...\n");
    for (int i = 0; i < curr_slot; i++) {
        if (clients[i].client_id != 0) {
            kill(clients[i].client_pid, SIGINT);
            receive_command(srv_qid, &request, STOP);
        }
    }
    if (msgctl(srv_qid, IPC_RMID, NULL) == -1) {
        error_and_end_errno();
    }

    return 0;
}
