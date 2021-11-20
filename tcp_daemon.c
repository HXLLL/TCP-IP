#include "socket.h"

#include "common_variable.h"
#include "utils.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const int PIPE_DEBUG = 1;

enum TCP_STATE{
    LISTEN,
};

struct socket_info_t {
    int type;
    int state;
};

/**
 * @brief starting from 1
 */
int socket_id_cnt = 0;

int main(int argc, char *argv[]) {
    int ret;

    ret = mkfifo(REQ_PIPE_NAME, 0666);
    if (ret == -1) {
        char s[100];
        perror(s);
        printf("%s\n", s);
    }
    // TODO: allow existing pipe

    FILE *req_f = fopen(REQ_PIPE_NAME, "r");
    CPEL(req_f == NULL);

    while (1) {
        char cmd[20]; int result;
        ret = fscanf(req_f, "%s", cmd);

        if (ret == EOF) {
            ret = fclose(req_f);
            CPEL(ret == -1);

            FILE *req_f = fopen(REQ_PIPE_NAME, "r");
            CPEL(req_f == NULL);

            if (PIPE_DEBUG) DPIPE("Reopening shared pipe");

            continue;
        }

        if (!strcmp(cmd, "socket")) {
            if (PIPE_DEBUG) DPIPE("Command: socket");

            result = ++socket_id_cnt;
        } else if (!strcmp(cmd, "bind")) {
        } else if (!strcmp(cmd, "listen")) {
            int sid, backlog;
            fscanf(req_f, "%d%d", &sid, &backlog);

            if (PIPE_DEBUG) DPIPE("Command: listen %d %d", sid, backlog);

            result = 0;
        } else if (!strcmp(cmd, "connect")) {
        } else if (!strcmp(cmd, "accept")) {
        } else if (!strcmp(cmd, "read")) {
        } else if (!strcmp(cmd, "write")) {
        } else if (!strcmp(cmd, "close")) {
        }

        char res_f_name[255];
        fscanf(req_f, "%s", res_f_name);

        FILE *res_f = fopen(res_f_name, "w");
        CPEL(res_f == NULL);

        if (PIPE_DEBUG) DPIPE("opening %s", res_f_name);

        fprintf(res_f, "%d\n", result);
        fflush(res_f);

        ret = fclose(res_f);
        CPEL(ret < 0);
    }
}