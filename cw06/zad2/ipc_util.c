#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "ipc_util.h"


void error_and_end(const char *error_msg) {
    assert(error_msg);
    fprintf(stderr, "%s\n", error_msg);
    exit(1);
}

void error_and_end_errno() {
    error_and_end(strerror(errno));
}

void error_no_end(const char *error_msg) {
    fputs(error_msg, stderr);
}

void error_no_end_errno() {
    error_no_end(strerror(errno));
}

char *read_file(FILE *fd) {
    // Returns new memory block containing file contents
    // Make sure to free after use
    assert(fd);
    // FILE* file = fopen(file_name, "r");
    // if (!file) {
    //     halt_and_catch_fire_errno();
    // }

    fseek(fd, 0 , SEEK_END);
    size_t file_size = (size_t) ftell(fd);
    fseek(fd, 0 , SEEK_SET);

    char *file_buffer = malloc(file_size + 1);
    if (!file_buffer) {
        error_and_end_errno();
    }

    size_t chars_read = fread(file_buffer, sizeof(char), file_size, fd);

    if (chars_read != file_size) {
        error_and_end("Unable to read file");
    }
    file_buffer[file_size] = '\0';
    // if (fclose(fd) != 0) {
    //     halt_and_catch_fire_errno();
    // }

    return file_buffer;
}
int count_lines(FILE *fd) {
    assert(fd);
    long curr_pos = ftell(fd);  // Save current position to restore it later
    fseek(fd, 0L, SEEK_SET);

    int lines = 1;              // Every file has at least one line
    char c;
    do {
        c = (char) fgetc(fd);   // Safe cast of an unsigned int to char
        if (c == '\n') {
            lines++;
        }
    } while (c != EOF);

    fseek(fd, curr_pos, SEEK_SET);
    return lines;
}



key_t get_server_key() {
    // Use the same path and id to obtain the key to create IPC structure
    return ftok(FTOK_PATH, FTOK_ID);
}

void pos_send_command(int qid, command cmd, char *msg) {
    // msg must be exactly QMSG_LEN bytes long, padded with '\0'
    char snd_txt[QMSG_LEN];
    snprintf(snd_txt, QMSG_LEN, "%d %s", cmd, msg);
    if (mq_send(qid, snd_txt, QMSG_LEN, 1) == -1) {
        error_and_end_errno();
    }
}

ssize_t pos_receive_command(int qid, char *mbuf) {
    ssize_t received = mq_receive(qid, mbuf, QMSG_LEN, NULL);
    if (received == -1) {
        if (errno == EINTR) {       // During waiting for a message SIGINT occurred
            return -1;
        }
        error_and_end_errno();
    }
    return received;
}

ssize_t pos_receive_command_noblock(int qid, char *mbuf) {
    // Don't check for negative return value as it indicates no messages and needs to be considered upstream
    struct timespec tm;
    clock_gettime(CLOCK_REALTIME, &tm);
    tm.tv_sec++;
    return mq_timedreceive(qid, mbuf, QMSG_LEN, NULL, &tm);
}
