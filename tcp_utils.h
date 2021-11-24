#ifndef TCP_UTILS_H
#define TCP_UTILS_H

#include "tcp.h"
#include <netinet/tcp.h>

#define RETRES(val)                                                           \
    { result = val;                                                              \
    break;}

// TODO: set return to errno
static int can_bind(struct sock_info_t *s, uint32_t addr, uint16_t port) {
    if (!s->valid) {
        return -ENOTSOCK;
    }
    struct iface_t *iface = find_iface_by_ip(addr);

    if (!iface) {
        return -EADDRNOTAVAIL;
    }

    if (port >= 65536 || port < 0) {
        return -EADDRNOTAVAIL;
    }

    if (iface->port_info[port].binded_socket != NULL) {
        return -EADDRINUSE;
    }

    return 0;
}
static int can_listen(struct sock_info_t *s) {
    if (!s->valid) {
        return -ENOTSOCK;
    }

    if (!s->binded) {
        return -EINVAL;
    }
    return 0;
}
static int can_connect(struct sock_info_t *s) { return 0; }
static int can_accept(struct sock_info_t *s) {
    if (!s->valid) {
        return -ENOTSOCK;
    }

    return 0;
}
static int can_read(struct sock_info_t *s) {
    if (!s->valid) return -ENOTSOCK;
    return 0;
}
static int can_write(struct sock_info_t *s) { 
    if (!s->valid) return -ENOTSOCK;

    return 0; 
}

static int can_close(struct sock_info_t *s) {
    if (!s->valid) return -ENOTSOCK;

    return 0; 
}

static int allocate_free_port(struct iface_t *iface, uint16_t *port) {
    for (int i = 50000; i != 65536; ++i) {
        if (iface->port_info[i].data_socket_cnt == 0 &&
            iface->port_info[i].binded_socket == NULL) {
            *port = i;
            return 0;
        }
    }
    return -1;
}

#endif