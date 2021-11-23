#ifndef TCP_H
#define TCP_H

#include "utils.h"

#define MAX_PORT 65536
#define MAX_CONNECTION 16
#define MAX_BUFFER_PER_SOCKET 255

enum SOCKET_TYPE {
    SOCKTYPE_CONNECTION,
    SOCKTYPE_DATA,
};

enum SOCKET_STATE {
    SOCKSTATE_UNBOUNDED,
    SOCKSTATE_BINDED,
    SOCKSTATE_LISTEN,
    SOCKSTATE_SYN_SENT,
    SOCKSTATE_SYN_RECEIVED,
    SOCKSTATE_ESTABLISHED,
};

struct segment_t {
    void *data;
    size_t len;
    int seq;
};
static struct segment_t *new_segment(size_t len, int seq) {
    struct segment_t *buf = malloc(sizeof(struct segment_t));
    buf->data = malloc(sizeof(len));
    memset(buf->data, 0, len);
    buf->len = len;
    buf->seq = seq;
    return buf;
}
static void free_segment(struct segment_t *buf) {
    free(buf->data);
    free(buf);
}

struct pending_connection_t {
    uint32_t remote_addr;
    uint16_t remote_port;
    uint32_t irs;
};

struct iface_t;

struct socket_info_t {
    struct iface_t *ifa;
    int id;
    int valid;
    int type;
    int state;
    int (*callback)(struct socket_info_t*);
    uint32_t local_addr;
    uint16_t local_port;
    uint32_t remote_addr;
    uint16_t remote_port;

    struct ring_buffer_t *in_buf, *out_buf;
    struct segment_t **nxt_send;

    // refer to RFC 793 https://datatracker.ietf.org/doc/html/rfc793#section-3.2
    uint32_t snd_una, snd_nxt, iss;
    uint32_t rcv_nxt, irs;

    FILE* res_f;
};

extern int socket_id_cnt;
extern struct socket_info_t sock_info[MAX_SOCKET];

struct port_info_t {
    int connection_socket;
    int data_socket[16]; // TODO: use link list
    int data_socket_cnt;
    struct ring_buffer_t *conn_queue;
};

struct iface_t {
    uint32_t ip;
    struct port_info_t port_info[MAX_PORT];
};

extern struct iface_t interfaces[MAX_DEVICES];

static struct iface_t *find_iface_by_ip(uint32_t ip) {
    for (int i = 0; i != total_dev; ++i) {
        if (interfaces[i].ip == ip) return &interfaces[i];
    }
    return NULL;
}

static int new_socket(int type, int state) {
    ++socket_id_cnt;

    struct socket_info_t *sock = &sock_info[socket_id_cnt];

    memset(sock, 0, sizeof(struct socket_info_t));

    sock->id = socket_id_cnt;
    sock->type = type;
    sock->valid = 1;
    sock->state = state;
    sock->callback = NULL;

    sock->in_buf = rb_new(MAX_BUFFER_PER_SOCKET);
    sock->out_buf = rb_new(MAX_BUFFER_PER_SOCKET);

    sock->nxt_send = (struct segment_t**)sock->out_buf->data;

    return socket_id_cnt;
}
static int free_socket() {
    // TODO
    return -1;
}

static int allocate_free_port(struct iface_t *iface, uint16_t *port) {
    for (int i=50000;i!=65536;++i) {
        if (iface->port_info[i].data_socket_cnt == 0 && iface->port_info[i].connection_socket == -1) {
            *port = i;
            return 0;
        }
    }
    return -1;
}

#endif