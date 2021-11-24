#define _DEFAULT_SOURCE

#include "arp.h"
#include "device.h"
#include "ip.h"
#include "link_state.h"
#include "socket.h"
#include "tcp.h"
#include "utils.h"
// #include "debug_utils.h"

#include "common_variable.h"
#include "utils.h"

#include "uthash/uthash.h"
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ROUTINE_INTERVAL 1000 // ms
#define BROADCAST_EXPIRE 500  // ms
#define MAX_NETWORK_SIZE 16

const int CALLBACK_DEBUG = 1;
const int DEBUG_SEPERATE_TICKS = 0;
const int LINKSTATE_ADVERTISE_DEBUG = 0;
const int LINKSTATE_RECV_DEBUG = 0;
const int FORWARD_DROP_DEBUG = 1;
const int BROADCAST_DEBUG = 0;
const int PIPE_DEBUG = 0;
const int PRINT_COMMAND = 1;
const int RT_DEBUG_DUMP = 0;
const int ARP_DEBUG_DUMP = 0;
const int TCP_DATA_DEBUG = 1;

const int RUN_LINKSTATE = 1;

// port info
static char p_dev_name[MAX_DEVICES][MAX_DEVICE_NAME];
static int p_dev_cnt;
static int p_dev_id[MAX_DEVICES];

static void initiate_broadcast(void *buf, int len);

pthread_mutex_t mtx;

/******************************************************************************
 *                       LinkState Routing Algorithm
 ******************************************************************************/

struct LinkState *ls;

static void process_link_state(void *data) {
    // Corner Case: might receive remote linkstate before neighbor
    uint32_t s_gid = GET4B(data, 8);

    if (LINKSTATE_RECV_DEBUG) {
        DRECV("Received linkstate info from %u", s_gid);
    }

    linkstate_process_advertise(ls, data);
}

static void send_link_state() {
    void *buf;

    size_t len = linkstate_make_advertisement(ls, &buf);
    if (LINKSTATE_ADVERTISE_DEBUG) {
        DSEND("Advertise link state info with gid %u", ls->router_gid);
    }

    initiate_broadcast(buf, len);

    free(buf);
}

/******************************************************************************
 *                     Broadcast Handling and Initiating
 ******************************************************************************/
struct bc_record {
    uint64_t id;
    uint64_t timestamp;
    UT_hash_handle hh;
};
static struct bc_record *new_bc_record(uint32_t addr, uint16_t id,
                                       uint64_t timestamp) {
    struct bc_record *rec = malloc(sizeof(struct bc_record));
    memset(rec, 0, sizeof(struct bc_record));
    rec->id = ((uint64_t)addr << 32) + id;
    rec->timestamp = timestamp;
    return rec;
}
// broadcast record
struct bc_record *bc_set;
int bc_id;

static void initiate_broadcast(void *buf, int len) {
    ++bc_id;
    uint64_t cur_time = gettime_ms();

    struct in_addr bc_src;
    bc_src.s_addr = dev_IP[0];

    struct bc_record *set_bc_id = new_bc_record(bc_src.s_addr, bc_id, cur_time);
    HASH_ADD_PTR(bc_set, id, set_bc_id);

    broadcastIPPacket(bc_src, MY_CONTROL_PROTOCOl, buf, len, bc_id);
}

static void handle_broadcast(struct iphdr *hdr, const uint8_t *data, int len) {
    uint64_t cur_time = gettime_ms();
    uint64_t bc_id = ((uint64_t)hdr->saddr << 32) + hdr->id;
    struct bc_record *bc_rec;
    HASH_FIND_PTR(bc_set, &bc_id, bc_rec);

    if (bc_rec &&
        cur_time - bc_rec->timestamp <= BROADCAST_EXPIRE) { // seen it before
        return;
    } else if (bc_rec) {
        bc_rec->timestamp = cur_time;
    } else {
        bc_rec = new_bc_record(hdr->saddr, hdr->id, cur_time);
        HASH_ADD_PTR(bc_set, id, bc_rec);
    }

    if (hdr->protocol == MY_CONTROL_PROTOCOl) {
        uint32_t my_protocol_type = *(uint32_t *)data;
        if (my_protocol_type == PROTO_LINKSTATE) {
            if (RUN_LINKSTATE) process_link_state((uint32_t *)data);
        }
    } else {
        DRECV("Unknown broadcast packet from %x", hdr->saddr);
    }

    if (BROADCAST_DEBUG) {
        DSEND("Forward broadcast from %d.%d.%d.%d, id: %d",
              GET1B(&hdr->saddr, 3), GET1B(&hdr->saddr, 2),
              GET1B(&hdr->saddr, 1), GET1B(&hdr->saddr, 0), hdr->id);
    }

    struct in_addr src;
    src.s_addr = hdr->saddr;
    broadcastIPPacket(src, hdr->protocol, data,
                      len - ip_hdr_len((uint8_t *)hdr), hdr->id);
}

/******************************************************************************
 *                              TCP Packet Handling
 ******************************************************************************/

static int tcp_send_segment(struct segment_t *seg, struct sock_info_t *sock) {
    int ret;

    struct tcphdr *hdr = (struct tcphdr *)seg->data;

    struct in_addr src, dst;
    src.s_addr = sock->local_addr;  // big endian
    dst.s_addr = sock->remote_addr; // big endian

    ret = sendIPPacket(src, dst, IPPROTO_TCP, seg->data, seg->len);

    if (TCP_DATA_DEBUG) {
        char src_ip[20], dst_ip[20];
        inet_ntop(AF_INET, &sock->local_addr, src_ip, 20);
        inet_ntop(AF_INET, &sock->remote_addr, dst_ip, 20);
        fprintf(stderr, "send packet, from %s:%d ---> %s:%d, type: %s, seq: %d, ack: %d\n", src_ip,
                sock->local_port, dst_ip, sock->remote_port, str_packet_type(hdr), hdr->seq, hdr->ack_seq);
    }

    return 0;
}

static int tcp_send_data(const void *buf, int len, struct sock_info_t *sock) {
    int ret;

    size_t total_len = len + sizeof(struct tcphdr);
    struct segment_t *send_buffer = segment_new(total_len, sock->snd_last_seq);
    // sock->snd_nxt += len;
    if (len) sock->snd_last_seq += 1; // TODO: go back n
    send_buffer->peer_ip = sock->remote_addr;

    struct tcphdr *hdr = (struct tcphdr *)send_buffer->data;

    // TODO: piggyback
    /* following is send in little endian */
    hdr->source = sock->local_port;
    hdr->dest = sock->remote_port;
    hdr->seq = send_buffer->seq;
    hdr->doff = 5;
    /* above is send in little endian */

    compute_tcp_checksum(hdr);

    void *data = tcp_raw_content(send_buffer->data);
    memcpy(data, buf, total_len);

    ret = rb_push(sock->out_buf, send_buffer);

    if (TCP_DATA_DEBUG) {
        DTCP("Pending outbound packet, sid=%d, seq=%d", sock->id, hdr->seq);
    }

    return ret;
}

static int tcp_send_ctrl_packet(struct sock_info_t *sock, uint16_t syn,
                                uint16_t ack, uint32_t ack_seq, uint16_t fin,
                                uint16_t rst) {
    int ret;

    size_t total_len = sizeof(struct tcphdr);
    struct segment_t *send_buffer = segment_new(total_len, sock->snd_last_seq);
    if (syn || fin) ++sock->snd_last_seq; // TODO: go back n

    struct tcphdr *hdr = (struct tcphdr *)send_buffer->data;

    /* following is send in little endian */
    hdr->source = sock->local_port;
    hdr->dest = sock->remote_port;
    hdr->syn = syn;
    hdr->seq = send_buffer->seq;
    hdr->ack = ack;
    hdr->ack_seq = ack_seq;
    hdr->fin = fin;
    hdr->rst = rst;
    hdr->doff = 5;
    /* above is send in little endian */

    compute_tcp_checksum(hdr);

    send_buffer->peer_ip = sock->remote_addr;

    ret = rb_push(sock->out_buf, send_buffer);

    if (TCP_DATA_DEBUG) {
        DTCP("Pending outbound ctrl packet, sid=%d, type=%s, seq: %d, ack: %d", sock->id, str_packet_type(hdr), hdr->seq, hdr->ack_seq);
    }

    return ret;
}

static int tcp_send_oob_ctrl_packet(struct sock_info_t *sock,
                                    uint32_t remote_ip, uint32_t remote_port,
                                    uint16_t syn, uint32_t seq, uint16_t ack,
                                    uint32_t ack_seq, uint16_t fin,
                                    uint16_t rst) {
    int ret;

    size_t total_len = sizeof(struct tcphdr);
    uint8_t *send_buffer = malloc(total_len);
    memset(send_buffer, 0, total_len);
    struct tcphdr *hdr = (struct tcphdr *)send_buffer;

    /* following is send in little endian */
    hdr->source = sock->local_port;
    hdr->dest = remote_port;
    hdr->syn = syn;
    hdr->seq = seq;
    hdr->ack = ack;
    hdr->ack_seq = ack_seq;
    hdr->fin = fin;
    hdr->rst = rst;
    hdr->doff = 5;
    /* above is send in little endian */

    compute_tcp_checksum(hdr);

    struct in_addr src, dst;
    src.s_addr = sock->local_addr; // big endian
    dst.s_addr = remote_ip;        // big endian

    ret = sendIPPacket(src, dst, IPPROTO_TCP, send_buffer, total_len);

    free(send_buffer);

    if (TCP_DATA_DEBUG) {
        char src_ip[20], dst_ip[20];
        inet_ntop(AF_INET, &sock->local_addr, src_ip, 20);
        inet_ntop(AF_INET, &sock->remote_addr, dst_ip, 20);
        fprintf(stderr, "sending oob ctrl packet, from %s:%d ---> %s:%d, seq: %d, ack: %d\n", src_ip,
                sock->local_port, dst_ip, sock->remote_port, hdr->seq, hdr->ack);
    }

    return ret;
}

static int tcp_send_syn(struct sock_info_t *sock) {
    return tcp_send_ctrl_packet(sock, 1, 0, 0, 0, 0);
}
static int tcp_send_ack(struct sock_info_t *sock) {
    return tcp_send_ctrl_packet(sock, 0, 1, sock->rcv_nxt, 0,
                                0); // TODO: consider seq number
}
static int tcp_send_fin(struct sock_info_t *sock) {
    return tcp_send_ctrl_packet(sock, 0, 0, 0, 1, 0);
}
static int tcp_send_synack(struct sock_info_t *sock) {
    return tcp_send_ctrl_packet(sock, 1, 1, sock->rcv_nxt, 0, 0);
}
static int tcp_send_rst(struct sock_info_t *sock) {
    return tcp_send_ctrl_packet(sock, 0, 0, 0, 0, 1);
}
static int tcp_send_oob_rst(struct sock_info_t *sock, uint32_t addr,
                            uint16_t port, uint32_t seq) {
    return tcp_send_oob_ctrl_packet(sock, addr, port, 0, seq, 0, 0, 0, 1);
}

static int tcp_callback_accept(struct sock_info_t *s, struct segment_t *seg);
static int tcp_callback_sent_synack(struct sock_info_t *s);
static int tcp_callback_connect(struct sock_info_t *s);

static int tcp_callback_accept(struct sock_info_t *s, struct segment_t *seg) {
    struct port_info_t *port = &(s->ifa->port_info[s->local_port]);
    struct tcphdr *hdr = seg->data;

    int sid_data = sock_new(SOCKSTATE_SYN_RECEIVED);
    struct sock_info_t *s_data = &sock_info[sid_data];

    if (CALLBACK_DEBUG) {
        fprintf(stderr,
                "[CALLBACK] socket %d accepted syn, sending synack from socket "
                "%d\n",
                s->id, s_data->id);
    }

    s_data->ifa = s->ifa;
    s_data->if_port = s->if_port;

    s->pending_op = OP_NOP;
    s_data->pending_op = OP_ACCEPT;
    s_data->res_f = s->res_f;
    s->res_f = NULL;

    s_data->local_addr = s->local_addr;
    s_data->local_port = s->local_port;
    s_data->remote_addr = seg->peer_ip;
    s_data->remote_port = hdr->source;
    s_data->irs = hdr->seq;
    s_data->rcv_nxt = hdr->seq + 1;

    s_data->iss = 1;
    s_data->snd_una = 1;
    s_data->snd_nxt = 2;
    s_data->snd_last_seq = 1;

    struct port_info_t *ifa_port =
        &(s_data->ifa->port_info[s_data->local_port]);

    ifa_port->data_socket[ifa_port->data_socket_cnt++] = s_data;

    tcp_send_synack(s_data);

    return 0;
}

static void daemon_return(FILE *res_f, uint32_t value) {
    int ret;

    fprintf(res_f, "%d\n", value);
    fflush(res_f);

    ret = fclose(res_f);
    CPEL(ret < 0);
}

static int tcp_callback_read(struct sock_info_t *s) {
    int ret;

    if (CALLBACK_DEBUG) {
        fprintf(stderr, "[CALLBACK] socket %d returning from read\n", s->id);
    }

    int read_len = min(s->pending_read_len, s->read_size);

    fprintf(s->res_f, "%d\n", read_len);
    if (s->read_head + read_len <= MAX_READ_BUFFER) {
        fwrite(&(s->read_buffer[s->read_head]), read_len, 1, s->res_f);
    } else {
        fwrite(&(s->read_buffer[s->read_head]),
               MAX_READ_BUFFER - s->read_head - read_len, 1, s->res_f);
        fwrite(s->read_buffer,
               read_len - (MAX_READ_BUFFER - s->read_head - read_len), 1,
               s->res_f);
    }
    s->read_head += read_len;
    s->read_size -= read_len;

    s->pending_op = OP_NOP;

    if (s->read_head >= MAX_READ_BUFFER) s->read_head -= MAX_READ_BUFFER;
    fflush(s->res_f);

    ret = fclose(s->res_f);
    CPEL(ret < 0);
    s->res_f = NULL;
    s->pending_read_len = 0;

    return 0;
}

static int tcp_callback_sent_synack(struct sock_info_t *s) {
    if (CALLBACK_DEBUG) {
        fprintf(stderr,
                "[CALLBACK] socket %d received ack for synack, returning from "
                "accept\n",
                s->id);
    }

    int ret;

    s->pending_op = OP_NOP;

    fprintf(s->res_f, "%d %x %d\n", s->id, s->remote_addr, s->remote_port);
    fflush(s->res_f);

    ret = fclose(s->res_f);
    CPEL(ret < 0);
    s->res_f = NULL;

    return 0;
}

static int tcp_callback_connect(struct sock_info_t *s) {
    if (CALLBACK_DEBUG) {
        fprintf(stderr,
                "[CALLBACK] socket %d received synack for syn, returning from "
                "connect\n",
                s->id);
    }

    int ret;

    s->pending_op = OP_NOP;

    fprintf(s->res_f, "%d\n", 0);
    fflush(s->res_f);

    ret = fclose(s->res_f);
    CPEL(ret < 0);
    s->res_f = NULL;

    return 0;
}

static int handle_tcp(const void *frame, size_t len, struct iface_t *iface,
                      uint32_t remote_ip) {
    pthread_mutex_lock(&mtx);
    int ret;
    const struct tcphdr *hdr = frame;
    const void *data = tcp_raw_content_const(frame);
    uint16_t s_port = hdr->source, d_port = hdr->dest;

    if (TCP_DATA_DEBUG) {
        DTCP("Receive packet from %x:%d, type: %s\n", remote_ip, s_port,
             str_packet_type(hdr));
    }

    struct port_info_t *if_port = &iface->port_info[d_port];
    struct sock_info_t *s = NULL;

    for (int i = 0; i != if_port->data_socket_cnt; ++i) {
        struct sock_info_t *s_data = if_port->data_socket[i];
        if (s_data->remote_addr == remote_ip && s_data->remote_port == s_port) {
            s = s_data;
            break;
        }
    }
    if (s == NULL && if_port->binded_socket &&
        if_port->binded_socket->state == SOCKSTATE_LISTEN) {
        s = if_port->binded_socket;
    }

    if (s == NULL && iface != if_sink) {
        pthread_mutex_unlock(&mtx);
        return handle_tcp(frame, len, if_sink, remote_ip);
    } else if (s == NULL) {
        return -1;
    }
    // if (hdr->seq != s->rcv_nxt) return -1;

    struct segment_t *seg = segment_new(len, hdr->seq);
    memcpy(seg->data, frame, len);
    seg->peer_ip = remote_ip;

    if (TCP_DATA_DEBUG) {
        DTCP("Pending inbound packet, sid=%d", s->id);
    }
    ret = rb_push(s->in_buf, seg);
    CPEL(ret < 0);

    pthread_mutex_unlock(&mtx);

    return ret;
}

static int tcp_process_packet(struct sock_info_t *s, struct segment_t *seg) {
    int ret;

    struct tcphdr *hdr = seg->data;

    switch (s->state) {
    case SOCKSTATE_CLOSED:
        if (hdr->rst) {
            tcp_send_oob_rst(s, seg->peer_ip, hdr->source, hdr->ack_seq);
            if (TCP_DATA_DEBUG) DTCP("packet handling case: 17");
            break;
        } else {
            tcp_send_oob_rst(s, seg->peer_ip, hdr->source, hdr->ack_seq);
            if (TCP_DATA_DEBUG) DTCP("packet handling case: 1");
            break;
        }
    case SOCKSTATE_LISTEN:
        if (hdr->rst) {
            if (TCP_DATA_DEBUG) DTCP("packet handling case: 2");
            break;
        }
        if (hdr->ack) {
            tcp_send_oob_rst(s, seg->peer_ip, hdr->source, hdr->ack_seq);
            if (TCP_DATA_DEBUG) DTCP("packet handling case: 3");
            break;
        }
        if (hdr->syn) {
            if (s->pending_op == OP_ACCEPT) {
                tcp_callback_accept(s, seg);
                if (TCP_DATA_DEBUG) DTCP("packet handling case: 4");
            } else {
                if (TCP_DATA_DEBUG) DTCP("packet handling case: 5");
            }
        }
        if (TCP_DATA_DEBUG) DTCP("packet handling case: 14");
        break;
    case SOCKSTATE_SYN_SENT:
        if (hdr->ack) {
            if (hdr->ack_seq == s->snd_una+1) { // TODO: go back n
                s->snd_una = hdr->ack_seq;
                if (TCP_DATA_DEBUG) DTCP("packet handling case: 15");
            } else {
                if (TCP_DATA_DEBUG) DTCP("packet handling case: 16");
            }
        }
        if (hdr->rst) {
            if (s->pending_op) {
                daemon_return(s->res_f, -ECONNRESET);
            }
            sock_free(s->id);
            if (TCP_DATA_DEBUG) DTCP("packet handling case: 6");
            break;
        }
        if (hdr->syn) {
            s->irs = hdr->seq;
            s->rcv_nxt = hdr->seq + 1;

            tcp_send_ack(s);

            if (s->snd_una > s->iss) {
                s->state = SOCKSTATE_ESTABLISHED;
                if (s->pending_op == OP_CONNECT) tcp_callback_connect(s);
                if (TCP_DATA_DEBUG) DTCP("packet handling case: 7");
            } else {
                s->state = SOCKSTATE_SYN_RECEIVED;
                if (TCP_DATA_DEBUG) DTCP("packet handling case: 8");
            }
        }
        break;
    case SOCKSTATE_SYN_RECEIVED:
    case SOCKSTATE_ESTABLISHED:
        if (hdr->rst) {
            if (s->pending_op) {
                daemon_return(s->res_f, -ECONNRESET);
            }
            sock_free(s->id);
            if (TCP_DATA_DEBUG) DTCP("packet handling case: 9");
            break;
        }
        if (hdr->seq != s->rcv_nxt) { // TODO: go back n
            tcp_send_ack(s);
            if (TCP_DATA_DEBUG) DTCP("packet handling case: 10");
            break;
        }
        if (hdr->ack) {
            if (s->state == SOCKSTATE_SYN_RECEIVED) {
                if (TCP_DATA_DEBUG) DTCP("packet handling case: 11");
                if (hdr->ack_seq == s->snd_una+1) {
                    s->state = SOCKSTATE_ESTABLISHED;
                    s->snd_una = hdr->ack_seq;
                    if (s->pending_op == OP_ACCEPT) {
                        tcp_callback_sent_synack(s);
                    } else if (s->pending_op == OP_CONNECT) {
                        tcp_callback_connect(s);
                    }
                } else {
                    CPEL(1);
                }
                break;
            } else if (s->state == SOCKSTATE_ESTABLISHED) {
                if (TCP_DATA_DEBUG) DTCP("packet handling case: 12");
                if (hdr->ack_seq == s->snd_una+1) {
                    s->snd_una = hdr->ack_seq;
                }
            }
        }
        if (s->state == SOCKSTATE_ESTABLISHED) {
            if (TCP_DATA_DEBUG) DTCP("packet handling case: 13");
            const uint8_t *data = tcp_raw_content_const(seg->data);
            size_t data_len = seg->len - tcp_hdr_len(hdr);

            if (TCP_DATA_DEBUG) {
                DTCP("Receive data of length %lu, adding to read buffer", data_len);
            }
            CPEL(data_len + s->read_size > MAX_READ_BUFFER);

            int i, j;
            for (i = 0, j = s->read_tail; i != data_len; ++i) {
                s->read_buffer[j] = data[i];
                ADD_MOD(j, MAX_READ_BUFFER);
            }
            s->read_size += data_len;
            s->read_tail = j;

            if (s->pending_op == OP_READ) {
                tcp_callback_read(s);
            }
        }
        break;
    default:
        CPEL(1);
    }

    return 0;
}

int poll_socket(struct sock_info_t *s) {
    pthread_mutex_lock(&mtx);
    while (!rb_empty(s->in_buf)) {
        if (TCP_DATA_DEBUG) {
            // DTCP("polling inbound packet, SND_UNA: %d, SND_NXT: %d, RCV_NXT: %d", s->snd_una, s->snd_nxt, s->rcv_nxt);
        }
        struct segment_t *seg = rb_front(s->in_buf);
        tcp_process_packet(s, seg);
        if (!s->valid) {
            return -1;
        }
        rb_pop(s->in_buf);
        segment_free(seg);
    }

    switch (s->pending_op) {
    case OP_READ:
        if (s->read_size != 0) {
            tcp_callback_read(s);
        }
    default:
        break;
    }

    if (!rb_empty(s->out_buf)) {
        // TODO: sliding window
        struct segment_t *seg = rb_front(s->out_buf);
        struct tcphdr *hdr = seg->data;

        if (TCP_DATA_DEBUG) {
            DTCP("polling outbound packet, SND_UNA: %d, SND_NXT: %d, RCV_NXT: %d", s->snd_una, s->snd_nxt, s->rcv_nxt);
        }

        if (s->state != SOCKSTATE_ESTABLISHED) {
            if (hdr->syn) {
                tcp_send_segment(seg, s);

                rb_pop(s->out_buf);
                segment_free(seg);
            }
        } else {
            if (s->snd_una == s->snd_nxt) {
                tcp_send_segment(seg, s);
                if (seg->len != tcp_hdr_len(seg->data) || hdr->syn || hdr->fin) ++s->snd_nxt;

                rb_pop(s->out_buf);
                segment_free(seg);
            }
        }
    }
    pthread_mutex_unlock(&mtx);
    return 0;
}

/******************************************************************************
 *                              IP Packet Handling
 ******************************************************************************/

static int ip_callback(const void *frame, int len) {
    int ret;
    struct iphdr *hdr = (struct iphdr *)frame;
    const uint8_t *data = ip_raw_content_const(frame);
    struct in_addr in_addr_src, in_addr_dst;
    in_addr_src.s_addr = hdr->saddr;
    in_addr_dst.s_addr = hdr->daddr;

    do {
        struct iface_t *iface = find_iface_by_ip(hdr->daddr);

        if (iface) {
            size_t data_len = len - ip_hdr_len(frame);

            if (hdr->protocol == IPPROTO_TCP) {
                handle_tcp(data, data_len, iface, hdr->saddr);
            } else {
                DRECV("Recv message from %x", in_addr_src.s_addr);
            }
        } else if (hdr->daddr == 0xffffffff) { // broadcast

            handle_broadcast(hdr, data, len);

        } else { // forward

            ret = sendIPPacket(in_addr_src, in_addr_dst, hdr->protocol, data,
                               len - ip_hdr_len(frame));

            if (ret == 0) { // forwarded it
                if (FORWARD_DROP_DEBUG) {
                    DSEND("Forward from %x to %x", hdr->saddr, hdr->daddr);
                }
            } else { // drop it
                if (FORWARD_DROP_DEBUG) {
                    DSEND("Drop packet from %x to %x", hdr->saddr, hdr->daddr);
                }
            }
        }
    } while (0);

    return 0;
}

/******************************************************************************
 *                        Maintain Data Structure
 ******************************************************************************/

static void update_routing_table() {
    struct ip_host_record *rec, *tmp;
    HASH_ITER(hh, ls->ip_rec, rec, tmp) {
        struct neigh_record *neigh_rec = linkstate_next_hop(ls, rec->ip);
        if (!neigh_rec) continue;

        struct in_addr dst, mask;
        dst.s_addr = rec->ip;
        mask.s_addr = 0xffffffff;

        setRoutingTable(dst, mask, neigh_rec->mac.data,
                        dev_names[neigh_rec->port]);
    }
}

static void routing_routine() {
    static int tick = 0;
    static uint64_t last_routine = 0;
    if (gettime_ms() - last_routine < ROUTINE_INTERVAL) return;
    last_routine = gettime_ms();

    if (DEBUG_SEPERATE_TICKS)
        fprintf(stderr, "===============time: %u==================\n", ++tick);

    for (int i = 0; i != total_dev; ++i)
        ARP_advertise(i);

    if (RUN_LINKSTATE) {
        // update linkstate, then update routing table based on linkstate
        linkstate_update(ls, total_dev, arp_t);
    }

    if (RUN_LINKSTATE) {
        update_routing_table();
        send_link_state();
    }

#ifdef DEBUG_UTILS_H
    if (RT_DEBUG_DUMP) {
        rt_dump(rt);
    }

    if (ARP_DEBUG_DUMP) {
        for (int i = 0; i != total_dev; ++i) {
            arp_dump(arp_t[i]);
        }
    }
#endif
}

/**
 * @brief starting from 1
 */
int sock_cnt;
int iface_cnt;
struct iface_t *if_sink;

struct iface_t interfaces[MAX_DEVICES];

struct sock_info_t sock_info[MAX_SOCKET];

#include "tcp_utils.h"

/******************************************************************************
 *                                  Init
 ******************************************************************************/

int tcp_daemon_init() {
    int ret;

    // init devices
    ret = device_init();
    RCPE(ret == -1, -1, "Error initializing devices");
    for (int i = 0; i != p_dev_cnt; ++i) {
        p_dev_id[i] = addDevice(p_dev_name[i]);
        CPE(p_dev_id[i] == -1, "Error adding device", p_dev_id[i]);
    }

    // init arp
    ret = arp_init();
    RCPE(ret == -1, -1, "Error initializing ARP");
    for (int i = 0; i != total_dev; ++i)
        new_arp_table(i);

    // init ip layer
    ret = ip_init();
    RCPE(ret == -1, -1, "Error initializing ip");

    // init broadcast set
    bc_set = NULL;

    if (RUN_LINKSTATE) {
        // init linkstate
        ls = malloc(sizeof(struct LinkState));
        linkstate_init(ls, total_dev, arp_t);
    }

    // init sockets
    sock_cnt = 0;

    // add virtual interface
    iface_cnt = total_dev;
    
    if_sink  = &interfaces[iface_cnt++];
    if_sink->ip = 0;

    // init interfaces[]
    for (int i = 0; i != iface_cnt; ++i) {
        if (i < total_dev) interfaces[i].ip = dev_IP[i];
        for (int j = 0; j != MAX_PORT; ++j) {
            interfaces[i].port_info[j].binded_socket = NULL;
            interfaces[i].port_info[j].data_socket_cnt = 0;
            memset(interfaces[i].port_info[j].data_socket, 0,
                   sizeof(interfaces[i].port_info[i].data_socket));
        }
    }

    pthread_mutex_init(&mtx, NULL);

    return 0;
}

int main(int argc, char *argv[]) {
    int ret, opt;

    // ===================== handle arguments ======================

    while ((opt = getopt(argc, argv, "d:")) != -1) {
        switch (opt) {
        case 'd':
            strncpy(p_dev_name[p_dev_cnt], optarg, MAX_DEVICE_NAME);
            ++p_dev_cnt;
            break;
        default:
            printf("Usage: %s -d [device]\n", argv[0]);
            return -1;
        }
    }

    // ============================ init ============================

    ret = mkfifo(REQ_PIPE_NAME, 0666);
    CPEL(ret == -1 && errno != EEXIST);

    int req_fd = open(REQ_PIPE_NAME, O_RDONLY | O_NONBLOCK);
    fd_set fds;
    struct timeval select_timeout;
    select_timeout.tv_sec = 0;
    select_timeout.tv_usec = 0;

    FILE *req_f = fdopen(req_fd, "r");
    CPEL(req_f == NULL);
    open(REQ_PIPE_NAME, O_WRONLY); //! hack

    ret = tcp_daemon_init();
    CPEL(ret == -1);

    fprintf(stderr, "Start listening...\n");
    setIPPacketReceiveCallback(ip_callback);

    // ======================= start the real work =======================

    while (1) {
        usleep(10000);

        routing_routine();

        for (int i = 1; i <= sock_cnt; ++i)
            if (sock_info[i].valid) poll_socket(&sock_info[i]);

        FD_ZERO(&fds);
        FD_SET(req_fd, &fds);
        int flag = select(255, &fds, NULL, NULL, &select_timeout);

        if (flag != 1) continue;

        char cmd[255];
        int result = 0, special_return = 0;
        ret = fscanf(req_f, "%s", cmd);

        // ====================== handle commands ==========================
        do {
            if (!strcmp(cmd, "socket")) {
                if (PRINT_COMMAND) DPIPE("Command: socket");

                int sid = sock_new(SOCKSTATE_CLOSED);

                result = sid;
            } else if (!strcmp(cmd, "bind")) {
                int sid;
                uint32_t ip_addr;
                uint16_t port;
                fscanf(req_f, "%d%x%hu", &sid, &ip_addr,
                       &port); // TODO: input validation

                if (PRINT_COMMAND)
                    DPIPE("Command: bind %d %x %d", sid, ip_addr, port);

                struct sock_info_t *s = &sock_info[sid];
                struct iface_t *ifa = find_iface_by_ip(ip_addr);

                ret = can_bind(s, ip_addr, port);
                if (ret < 0) {
                    result = ret;
                    break;
                }

                if (s->state != SOCKSTATE_CLOSED) RETRES(-EINVAL);

                s->local_addr = ip_addr;
                s->local_port = port;
                s->ifa = ifa;
                s->if_port = &ifa->port_info[port];
                s->binded = 1;

                ifa->port_info[port].binded_socket = s;

                result = 0;
            } else if (!strcmp(cmd, "listen")) {
                int sid, backlog;
                fscanf(req_f, "%d%d", &sid, &backlog);

                if (PRINT_COMMAND) DPIPE("Command: listen %d %d", sid, backlog);

                struct sock_info_t *s = &sock_info[sid];

                ret = can_listen(s);
                if (ret < 0) {
                    result = ret;
                    break;
                }

                if (s->state != SOCKSTATE_CLOSED &&
                    s->state != SOCKSTATE_LISTEN)
                    RETRES(-EINVAL); // TODO: check macro

                s->state = SOCKSTATE_LISTEN;

                result = 0;
            } else if (!strcmp(cmd, "connect")) {
                int sid;
                uint32_t ip_addr;
                uint16_t port;
                fscanf(req_f, "%d%x%hu", &sid, &ip_addr, &port);

                if (PRINT_COMMAND)
                    DPIPE("Command: connect %d %x %d", sid, ip_addr, port);

                struct sock_info_t *s = &sock_info[sid];

                ret = can_connect(s);
                if (ret < 0) {
                    result = ret;
                    break;
                }

                if (!s->binded) {
                    struct neigh_record *rec = linkstate_next_hop(ls, ip_addr);
                    if (rec == NULL) RETRES(-EADDRNOTAVAIL);

                    struct iface_t *ifa = &interfaces[rec->port];
                    s->local_addr = ifa->ip;
                    allocate_free_port(ifa, &(s->local_port));
                    s->ifa = ifa;
                    s->if_port = &s->ifa->port_info[s->local_port];
                    s->if_port->binded_socket = s;

                    s->binded = 1;
                }

                char res_f_name[255];
                fscanf(req_f, "%s", res_f_name);
                FILE *res_f = fopen(res_f_name, "w");
                CPEL(res_f == NULL);

                s->state = SOCKSTATE_SYN_SENT;
                s->res_f = res_f;
                s->pending_op = OP_CONNECT;

                s->remote_addr = ip_addr;
                s->remote_port = port;

                s->iss = 1;
                s->snd_una = 1;
                s->snd_nxt = 2;
                s->snd_last_seq = 1;

                s->if_port->data_socket[s->if_port->data_socket_cnt++] = s;

                tcp_send_syn(s);

                special_return = 1;
            } else if (!strcmp(cmd, "accept")) {
                int sid;
                fscanf(req_f, "%d", &sid);

                if (PRINT_COMMAND) DPIPE("Command: accept %d", sid);

                struct sock_info_t *s = &sock_info[sid];

                ret = can_accept(s);
                if (ret < 0) {
                    result = ret;
                    break;
                }

                char res_f_name[255];
                fscanf(req_f, "%s", res_f_name);
                FILE *res_f = fopen(res_f_name, "w");
                CPEL(res_f == NULL);

                if (PIPE_DEBUG) DPIPE("opening %s", res_f_name);

                s->res_f = res_f;
                s->pending_op = OP_ACCEPT;

                special_return = 1;
            } else if (!strcmp(cmd, "read")) {
                int sid, read_len;
                fscanf(req_f, "%d %d\n", &sid, &read_len);

                if (PRINT_COMMAND) DPIPE("Command: read %d %d", sid, read_len);

                struct sock_info_t *s = &sock_info[sid];

                ret = can_read(s);
                if (ret < 0) {
                    result = ret;
                    break;
                }

                char res_f_name[255];
                fscanf(req_f, "%s", res_f_name);
                FILE *res_f = fopen(res_f_name, "w");
                CPEL(res_f == NULL);

                s->res_f = res_f;
                s->pending_read_len = read_len;
                s->pending_op = OP_READ;

                special_return = 1;
            } else if (!strcmp(cmd, "write")) {
                int sid, write_len;
                fscanf(req_f, "%d %d\n", &sid, &write_len);

                if (PRINT_COMMAND)
                    DPIPE("Command: write %d %d", sid, write_len);

                struct sock_info_t *s = &sock_info[sid];

                ret = can_write(s);
                if (ret < 0) {
                    result = ret;
                    break;
                }

                char *buffer = malloc(write_len);
                fread(buffer, 1, write_len, req_f);

                tcp_send_data(buffer, write_len, s);

                free(buffer);

                result = write_len;
            } else if (!strcmp(cmd, "close")) {
                int sid;
                fscanf(req_f, "%d", &sid);

                if (PRINT_COMMAND) DPIPE("Command: close %d", sid);

                struct sock_info_t *s = &sock_info[sid];

                ret = can_close(s);
                if (ret < 0) {
                    result = ret;
                    break;
                }

                switch (s->state) {
                case SOCKSTATE_SYN_RECEIVED:
                case SOCKSTATE_ESTABLISHED:
                    tcp_send_oob_rst(s, s->remote_addr, s->remote_port, 0);
                case SOCKSTATE_CLOSED:
                case SOCKSTATE_LISTEN:
                case SOCKSTATE_SYN_SENT:
                    sock_free(s->id);
                    break;
                default:
                    CPEL(1);
                }

            } else if (!strcmp(cmd, "nop")) {

                if (PRINT_COMMAND) DPIPE("Command: nop");

                result = 0x7fffffff;

                special_return = 1;
            }
        } while (0);

        if (special_return) continue;

        // ================= then return the result =======================
        char res_f_name[255];
        fscanf(req_f, "%s", res_f_name);

        FILE *res_f = fopen(res_f_name, "w");
        CPEL(res_f == NULL);

        if (PIPE_DEBUG) DPIPE("opening %s", res_f_name);

        if (PRINT_COMMAND) DPIPE("Returning %d", result);

        fprintf(res_f, "%d\n", result);
        fflush(res_f);

        ret = fclose(res_f);
        CPEL(ret < 0);
    }
}