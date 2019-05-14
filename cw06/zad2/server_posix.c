#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>


#include "ipc_util.h"

typedef struct client {
    int client_id;              // Client ID used to identify client in the system
    int client_qid;             // Client's queue ID used to send it messages
    pid_t client_pid;           // Client's PID used to send it signals
    int friend_count;
    struct client **friends;
} client;

int running = 1;

// Protocol:
// Request: COMMAND_TYPE CID COMMAND_BODY   <QMSG_LEN bytes in total>
// Response: COMMAND_TYPE COMMAND_BODY      <QMSG_LEN bytes in total>

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
        fprintf(stderr, "Tried to add too many friends to client %d\n", c->client_id);
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
            mq_close(cs[i].client_qid);
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

    struct mq_attr queue_attribs;
    // Keep mq_maxmsg low, it's easy to exceed the system limit
    queue_attribs.mq_maxmsg = 4;
    queue_attribs.mq_msgsize = QMSG_LEN;
    char *srv_queue_path = "/srv_q";
    mq_unlink(srv_queue_path);      // Unlink just in case, what's the worst that could happen?
    mqd_t srv_qid = mq_open(srv_queue_path, O_EXCL | O_CREAT | O_RDONLY, 0666, &queue_attribs);
    if (srv_qid == -1) {
        fprintf(stderr, "Couldn't open server queue\n");
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

    char request[QMSG_LEN];
    char response[QMSG_LEN];
    while (running) {
        memset(request, '\0', QMSG_LEN);
        memset(response, '\0', QMSG_LEN);

        ssize_t received = pos_receive_command(srv_qid, request);
        if (!running) {     // SIGINT might have occurred
            break;
        }
        if (received == -1) {
            fprintf(stderr, "Error while receiving message\n");
            error_and_end_errno();
        }

        command type = strtol(strtok(request, " "), NULL, 10);
        int cid = (int) strtol(strtok(NULL, " "), NULL, 10);
        char *cmd_body = strtok(NULL, "\n");
        client *cl;

        cl = get_client_from_cid(clients, cid, curr_slot);
        if (cl == NULL && type != INIT) {
            fprintf(stderr, "Malformed client id: %d\n", cid);
            continue;
        }

        if (type == STOP) {
            remove_client(clients, cid, curr_slot);    // Search clients table up to the curr_slot index
            printf("* Removing client with id: %d\n", cid);
        } else if (type == INIT) {
            pid_t cpid = (pid_t) strtol(strtok(cmd_body, " "), NULL, 10);
            char *client_queue_path = strtok(NULL, "\n");

            mqd_t client_qid = mq_open(client_queue_path, O_WRONLY);
            if (client_qid == -1) {
                error_no_end("* Unable to open client queue. Invalid key.\n");
                continue;
            }

            add_client(client_qid, cpid, curr_slot, clients);
            snprintf(response, QMSG_LEN, "%d", curr_slot);      // Offset in clients table becomes client id
            curr_slot++;
            pos_send_command(client_qid, ACK, response);
            printf("* Handshake with new client: client_id: %d, client_pid: %d, client_qid: %d\n",
                    curr_slot - 1,
                    cpid,
                    client_qid);
        } else if (type == ECHO) {
            snprintf(response, QMSG_LEN, "%s", cmd_body);
            pos_send_command(cl->client_qid, ECHO, response);
            kill(cl->client_pid, SIGUSR1);
            printf("* Echoing message: '%s' to client: %d\n", response, cl->client_id);
        } else if (type == LIST) {
            int written = 0;
            for (int i = 1; i < curr_slot; i++) {
                if (clients[i].client_id != 0) {
                    if (written > QMSG_LEN) {
                        break;                  // In case response gets too long
                    }
                    written = snprintf(response + written,
                                       (size_t) QMSG_LEN - written,
                                       "Client id: %d, Client pid: %d\n",
                                       clients[i].client_id,
                                       clients[i].client_pid);
                }
            }
            pos_send_command(cl->client_qid, LIST, response);
            kill(cl->client_pid, SIGUSR1);
            printf("* Listing clients for client_id: %d\n", cid);
        } else if (type == FRIENDS || type == ADD) {
            if (type == FRIENDS) {
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
        } else if (type == TOALL) {
            snprintf(response, QMSG_LEN, "[%d] %s", cl->client_id, cmd_body);
            for (int i = 1; i < curr_slot; i++) {
                if (clients[i].client_id != 0 && clients[i].client_id != cl->client_id) {
                    pos_send_command(clients[i].client_qid, TOALL, response);
                    kill(clients[i].client_pid, SIGUSR1);
                }
            }
            printf("* Forwarding message to all from client_id: %d\n", cid);
        } else if (type == TOFRIENDS) {
            snprintf(response, QMSG_LEN, "[%d] %s", cl->client_id, cmd_body);
            for (int i = 0; i < cl->friend_count; i++) {
                if (cl->friends[i] && cl->friends[i]->client_id != 0) {
                    pos_send_command(cl->friends[i]->client_qid, TOFRIENDS, response);
                    kill(cl->friends[i]->client_pid, SIGUSR1);
                }
            }
            printf("* Forwarding message to friends from client_id: %d\n", cid);
        } else if (type == TOONE) {
            int target_id = (int) strtol(strtok(cmd_body, " "), NULL, 10);
            client *target = get_client_from_cid(clients, target_id, curr_slot);
            if (target == NULL) {
                error_no_end("* Cannot send message to non-existent client\n");
                continue;
            }
            snprintf(response, QMSG_LEN, "[%d] %s", cl->client_id, strtok(NULL, "\n"));
            pos_send_command(target->client_qid, TOONE, response);
            kill(target->client_pid, SIGUSR1);
            printf("* Forwarding message to client_id: %d from client_id: %d\n", target_id, cid);
        } else if (type == DEL) {
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
            fprintf(stderr, "* Malformed command: %s\n", cmd_body);
        }
    }

    // Inform clients about exit and clean up server queue
    fprintf(stderr, "* Informing clients about exit...\n");
    for (int i = 0; i < curr_slot; i++) {
        if (clients[i].client_id != 0) {
            kill(clients[i].client_pid, SIGINT);
        }
    }
    if (mq_close(srv_qid) == -1) {
        error_and_end_errno();
    }
    mq_unlink(srv_queue_path);

    return 0;
}