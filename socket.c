
#include "socket.h"

#include "common_variable.h"
#include "socket_utils.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>

#define MAX_MY_FDESC 65536
#define MY_FDESC_START 65536

const int PIPE_DEBUG = 1;

/**
 * @brief my_fdesc, starting at 65536
 */
int my_fdesc_id = MY_FDESC_START;
int my_fdesc_socket_id[MAX_MY_FDESC];

static char req_string_buffer[MAX_REQ_LEN];
static char result_pipe_name[255];

static FILE *res_f;

static int call_daemon(const char *format, ...) {
    int write_ret, ret;
    va_list args;

    // TODO: make req_f and res_f consistent
    int req_fd = open(REQ_PIPE_NAME, O_WRONLY);
    CPEL(req_fd == -1);

    if (!res_f) {
        sprintf(result_pipe_name, RES_PIPE_NAME, getpid());
        ret = mkfifo(result_pipe_name, 0666);
        CPEL(ret == -1 && errno != EEXIST);
    }

    va_start(args, format);
    vsprintf(req_string_buffer, format, args);
    va_end(args);

    strcat(req_string_buffer, " ");
    strcat(req_string_buffer, result_pipe_name);
    strcat(req_string_buffer, "\n");
    int len = strlen(req_string_buffer);

    write_ret = write(req_fd, req_string_buffer, len);

    if (!res_f) {
        res_f = fopen(result_pipe_name, "r");
        CPEL(res_f == NULL);
        open(result_pipe_name, O_WRONLY); //! hack
    }

    ret = close(req_fd);
    CPEL(ret < 0);

    return write_ret;
}

static int get_daemon_ret(const char *format, ...) {
    int ret;
    va_list args;

    va_start(args, format);
    ret = vfscanf(res_f, format, args);
    va_end(args);

    return ret;
}

int __wrap_socket(int domain, int type, int protocol) {
    int ret;

    if (domain == AF_INET && type == SOCK_STREAM && protocol == 0) {
        call_daemon("socket");
        int socket_id;
        get_daemon_ret("%d", &socket_id);

        my_fdesc_socket_id[my_fdesc_id - MY_FDESC_START] = socket_id;

        return my_fdesc_id++;
    } else {
        return socket(domain, type, protocol);
    }
}

int __wrap_bind(int socket, const struct sockaddr *address,
                socklen_t address_len) {
    int ret;
    if (socket >= MY_FDESC_START) {
        const struct sockaddr_in *addr = (const struct sockaddr_in *)address;

        call_daemon("bind %d %x %d",
                          my_fdesc_socket_id[socket - MY_FDESC_START],
                          addr->sin_addr.s_addr, addr->sin_port);
        get_daemon_ret("%d", &ret);

        if (addr->sin_family != AF_INET) {
            CPEL(1); // TODO: modify errno
        }

        if (ret < 0) {
            errno = -ret;
            return -1;
        } else {
            return 0;
        }
    } else {
        return bind(socket, address, address_len);
    }
}

int __wrap_listen(int socket, int backlog) { // TODO: recognize backlog
    int ret;

    if (socket >= MY_FDESC_START) {
        call_daemon("listen %d %d",
                          my_fdesc_socket_id[socket - MY_FDESC_START], backlog);
        get_daemon_ret("%d", &ret);

        if (ret < 0) {
            errno = -ret;
            return -1;
        } else {
            return 0;
        }
    } else {
        return listen(socket, backlog);
    }
}

int __wrap_connect(int socket, const struct sockaddr *address,
                   socklen_t address_len) {
    int ret;

    if (socket >= MY_FDESC_START) {
        const struct sockaddr_in *addr = (const struct sockaddr_in *)address;

        call_daemon("connect %d %x %d",
                          my_fdesc_socket_id[socket - MY_FDESC_START],
                          addr->sin_addr.s_addr, addr->sin_port);
        get_daemon_ret("%d", &ret);

        if (ret < 0) {
            errno = -ret;
            return -1;
        } else {
            return 0;
        }
    } else {
        return connect(socket, address, address_len);
    }
}

int __wrap_accept(int socket, struct sockaddr *address, socklen_t *address_len) {
    int ret;

    if (socket >= MY_FDESC_START) {
        call_daemon("accept %d",
                          my_fdesc_socket_id[socket - MY_FDESC_START]);
        uint32_t remote_addr; uint16_t remote_port;
        get_daemon_ret("%d%x%hu", &ret, &remote_addr, &remote_port);

        if (address) {
            struct sockaddr_in *addr = (struct sockaddr_in *)address;
            memset(addr, 0, sizeof(struct sockaddr_in));
            addr->sin_addr.s_addr = remote_addr;
            addr->sin_port = remote_port;
            addr->sin_family = AF_INET;
        }
        if (address_len) {
            *address_len = sizeof(struct sockaddr_in);
        }

        if (ret < 0) {
            errno = -ret;
            return -1;
        } else {
            return 0;
        }
    } else {
        return accept(socket, address, address_len);
    }
}

ssize_t __wrap_read(int fildes, void *buf, size_t nbyte);

ssize_t __wrap_write(int fildes, const void *buf, size_t nbyte);

int __wrap_close(int fildes);

int __wrap_getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints, struct addrinfo **res) {
    return getaddrinfo(node, service, hints, res);
}
