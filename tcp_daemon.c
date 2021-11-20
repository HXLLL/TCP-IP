#include "socket.h"
#include "tcp.h"

#include "common_variable.h"
#include "utils.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const int PIPE_DEBUG = 1;

/**
 * @brief starting from 1
 */
int socket_id_cnt = 0;

struct socket_info_t sock_info[MAX_SOCKET];
struct port_info_t port_info[MAX_PORT];

#include "tcp_utils.h"

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
    open(REQ_PIPE_NAME, O_WRONLY); //! hack
    CPEL(req_f == NULL);

    while (1) {
        char cmd[20];
        int result;
        ret = fscanf(req_f, "%s", cmd);

        do {
            if (!strcmp(cmd, "socket")) {
                if (PIPE_DEBUG) DPIPE("Command: socket");

                ++socket_id_cnt;

                struct socket_info_t *s = &sock_info[socket_id_cnt];

                s->state = SOCKSTATE_UNBOUNDED;
                s->valid = 1;
                s->type = SOCKTYPE_CONNECTION;

                result = socket_id_cnt;
            } else if (!strcmp(cmd, "bind")) {
                int sid;
                uint32_t ip_addr;
                uint16_t port;

                fscanf(req_f, "%d%x%hu", &sid, &ip_addr, &port);

                if (PIPE_DEBUG) DPIPE("Command: bind %d %x %d", sid, ip_addr, port);

                struct socket_info_t *s = &sock_info[sid];

                ret = can_bind(s);
                if (ret < 0) {
                    result = ret;
                    break;
                }

                s->addr = ip_addr;
                s->port = port;
                s->state = SOCKSTATE_BINDED;
                port_info[s->port].binded_socket = sid;

                result = 0;
            } else if (!strcmp(cmd, "listen")) {
                int sid, backlog;
                fscanf(req_f, "%d%d", &sid, &backlog);

                if (PIPE_DEBUG) DPIPE("Command: listen %d %d", sid, backlog);

                struct socket_info_t *s = &sock_info[sid];

                ret = can_listen(s);
                if (ret < 0) {
                    result = ret;
                    break;
                }

                s->state = SOCKSTATE_LISTEN; // TODO: carefully consider state transition

                result = 0;
            } else if (!strcmp(cmd, "connect")) {
            } else if (!strcmp(cmd, "accept")) {
            } else if (!strcmp(cmd, "read")) {
            } else if (!strcmp(cmd, "write")) {
            } else if (!strcmp(cmd, "close")) {
            }
        } while (0);

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