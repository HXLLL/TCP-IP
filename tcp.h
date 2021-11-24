#ifndef TCP_H
#define TCP_H

#include "utils.h"

#include <stdio.h>

#define MAX_PORT 65536
#define MAX_CONNECTION 16
#define MAX_BUFFER_PER_SOCKET 255
#define MAX_READ_BUFFER 65536


enum SOCKET_STATE {
    SOCKSTATE_CLOSED,
    SOCKSTATE_LISTEN,
    SOCKSTATE_SYN_SENT,
    SOCKSTATE_SYN_RECEIVED,
    SOCKSTATE_ESTABLISHED,
};

enum PENDING_OP {
    OP_NOP,
    OP_CONNECT,
    OP_ACCEPT,
    OP_READ,
};

/* Structures */
struct segment_t {
    void *data;
    size_t len;
    uint32_t peer_ip;
    int seq;
};

struct port_info_t;

/**
 * @brief structure for socket/connection information, * created with sock_new.
 * after creation, the following fields may need to be manually configured:
 * local_addr, local_port, remote_addr, remote_port, snd_una, snd_nxt, iss,
 * rcv_nxt, irs, res_f, ifa, if_port, callback
 */
struct sock_info_t {
    int id;
    int binded;
    int valid;
    int state;
    uint32_t local_addr;
    uint16_t local_port;
    uint32_t remote_addr;
    uint16_t remote_port;

    struct ring_buffer_t *in_buf, *out_buf;

    int read_head, read_tail, read_size;
    char *read_buffer;

    // refer to RFC 793 https://datatracker.ietf.org/doc/html/rfc793#section-3.2
    uint32_t snd_una, snd_nxt, iss, snd_last_seq;
    uint32_t rcv_nxt, irs;

    int pending_op;
    FILE *res_f;
    int pending_read_len;

    struct iface_t *ifa;
    struct port_info_t *if_port;
};

struct port_info_t {
    struct sock_info_t *binded_socket;
    struct sock_info_t *data_socket[16]; // TODO: use link list
    int data_socket_cnt;
};

struct iface_t {
    uint32_t ip;
    struct port_info_t port_info[MAX_PORT];
};


/* Global Variables */
extern struct iface_t interfaces[MAX_DEVICES];
extern int sock_cnt;
extern int iface_cnt;
extern struct iface_t *if_sink;
extern struct sock_info_t sock_info[MAX_SOCKET];


/* segment_t helper functions */
static struct segment_t *segment_new(size_t len, int seq) {
    struct segment_t *buf = malloc(sizeof(struct segment_t));
    buf->data = malloc(len);
    memset(buf->data, 0, len);
    buf->len = len;
    buf->seq = seq;
    return buf;
}
static void segment_free(void *buf) {
    struct segment_t *seg = buf;
    free(seg->data);
    free(seg);
}


/* iface_t helper functions */
static struct iface_t *find_iface_by_ip(uint32_t ip) {
    for (int i = 0; i != iface_cnt; ++i) {
        if (interfaces[i].ip == ip) return &interfaces[i];
    }
    return NULL;
}


/* sock_info_t helper function */
static int sock_new(int state) {
    struct sock_info_t *s = &sock_info[++sock_cnt];
    memset(s, 0, sizeof(struct sock_info_t));

    s->id = sock_cnt;
    s->valid = 1;
    s->binded = 0;
    s->state = state;
    s->pending_op = OP_NOP;

    s->in_buf = rb_new(MAX_BUFFER_PER_SOCKET);
    s->out_buf = rb_new(MAX_BUFFER_PER_SOCKET);

    s->read_buffer = malloc(MAX_READ_BUFFER);
    memset(s->read_buffer, 0, MAX_READ_BUFFER);

    s->read_head = s->read_tail = s->read_size = 0;

    return sock_cnt;
}

static void sock_free(int sid) {
    struct sock_info_t *s = &sock_info[sid];

    s->valid = 0;
    rb_free(s->in_buf, segment_free);
    rb_free(s->out_buf, segment_free);

    free(s->read_buffer);

    struct iface_t *ifa = s->ifa;
    struct port_info_t *ifa_port = &ifa->port_info[s->local_port];
    if (ifa_port->binded_socket == s) {
        ifa_port->binded_socket = NULL;
    } else { // delete this socket from ifa_port->data_socket
        int p = -1;
        for (int i = 0; i != ifa_port->data_socket_cnt; ++i)
            if (ifa_port->data_socket[i] == s) {
                p = i;
                break;
            }
        if (p != -1) {
            for (int i = p; i < ifa_port->data_socket_cnt - 1; ++i) {
                ifa_port->data_socket[i] = ifa_port->data_socket[i + 1];
            }
            --ifa_port->data_socket_cnt;
        }
    }
}

#endif