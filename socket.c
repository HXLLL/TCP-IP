
#include "socket.h"

#include "utils.h"
#include "socket_utils.h"
#include "common_variable.h"

#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_MY_FDESC 65536

/**
 * @brief my_fdesc, starting at 65536
 */
int my_fdesc_id = 65536;
int my_fdesc_socket_id[MAX_MY_FDESC];

static char req_string_buffer[MAX_REQ_LEN];

static int call_daemon(const char *format, ...) {
    int result, ret;
    va_list args;

    // TODO: make req_f and res_f consistent
    int req_fd = open(REQ_PIPE_NAME, O_WRONLY);
    CPEL(req_fd == -1);

    char result_pipe_name[255];
    sprintf(result_pipe_name, RES_PIPE_NAME, getpid());

    ret = mkfifo(result_pipe_name, 0666);
    // CPEL(ret == -1);
    // TODO: allow existing pipe

    va_start(args, format);
    vsprintf(req_string_buffer, format, args);
    va_end(args);

    strcat(req_string_buffer, " ");
    strcat(req_string_buffer, result_pipe_name);
    strcat(req_string_buffer, "\n");
    int len = strlen(req_string_buffer);

    write(req_fd, req_string_buffer, len);

    FILE *res_f = fopen(result_pipe_name, "r");
    CPEL(res_f == NULL);

    ret = fscanf(res_f, "%d", &result);
    CPEL(ret == 0);

    ret = fclose(res_f);
    CPEL(ret < 0);

    ret = close(req_fd);
    CPEL(ret < 0);

    return result;
}

int __wrap_socket(int domain, int type, int protocol) {
    int ret; 

    if (domain == AF_INET && type == SOCK_STREAM && protocol == 0) {
        int socket_id = call_daemon("socket");

        my_fdesc_socket_id[my_fdesc_id] = socket_id;
        
        return my_fdesc_id++;
    } else {
        return socket(domain, type, protocol);
    }
}

int __wrap_bind(int socket, const struct sockaddr *address,
                socklen_t address_len) {
    return bind(socket, address, address_len);
}

int __wrap_listen(int socket, int backlog) { // TODO: recognize backlog
    int ret;

    ret = call_daemon("listen %d %d", my_fdesc_socket_id[socket], backlog);

    return ret;
}

int __wrap_connect(int socket, const struct sockaddr *address,
                   socklen_t address_len);

int __wrap_accept(int socket, struct sockaddr *address, socklen_t *address_len);

ssize_t __wrap_read(int fildes, void *buf, size_t nbyte);

ssize_t __wrap_write(int fildes, const void *buf, size_t nbyte);

int __wrap_close(int fildes);

int __wrap_getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints, struct addrinfo **res) {
    return getaddrinfo(node, service, hints, res);
}
