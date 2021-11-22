#ifndef TCP_UTILS_H
#define TCP_UTILS_H

#include "tcp.h"

// TODO: set return to errno
static int can_bind(struct socket_info_t *s, uint32_t addr, uint16_t port) {
    if (!s->valid) {
        return -ENOTSOCK;
    }
    if (s->type != SOCKTYPE_CONNECTION) {
        return -EINVAL;
    }
    if (s->state != SOCKSTATE_UNBOUNDED) {
        return -EINVAL;
    }
    if (0) { // TODO: detect Address already in use
        return -EALREADY;
    }
    struct iface_t *iface = find_iface_by_ip(addr);

    if (!iface) {
        return -EADDRNOTAVAIL;
    }

    if (port >= 65536 || port < 0) {
        return -EADDRNOTAVAIL;
    }

    if (iface->port_info[port].connection_socket != -1) {
        return -EADDRINUSE;
    }

    return 0;
}
static int can_listen(struct socket_info_t *s) {
    if (!s->valid) {
        return -ENOTSOCK;
    }

    if (s->type != SOCKTYPE_CONNECTION) {
        return -EINVAL;
    }

    if (s->state != SOCKSTATE_BINDED) {
        return -EINVAL;
    }
    return 0;
}
static int can_connect(struct socket_info_t *s) { return 0; }
static int can_accept(struct socket_info_t *s) { return 0; }

#endif