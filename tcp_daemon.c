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

const int DEBUG_SEPERATE_TICKS = 0;
const int LINKSTATE_ADVERTISE_DEBUG = 0;
const int LINKSTATE_RECV_DEBUG = 0;
const int FORWARD_DROP_DEBUG = 1;
const int BROADCAST_DEBUG = 0;
const int PIPE_DEBUG = 0;
const int PRINT_COMMAND = 1;
const int RT_DEBUG_DUMP = 0;
const int ARP_DEBUG_DUMP = 0;
const int TCP_DATA_DEBUG = 0;

const int RUN_LINKSTATE = 1;

// port info
char p_dev_name[MAX_DEVICES][MAX_DEVICE_NAME];
int p_dev_cnt;
int p_dev_id[MAX_DEVICES];

static void initiate_broadcast(void *buf, int len);

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

static int tcp_send_data(struct segment_t *seg, struct socket_info_t *sock) {
    int ret;
    size_t total_len = sizeof(struct tcphdr) + seg->len;
    uint8_t *send_buffer = malloc(total_len);
    memset(send_buffer, 0, total_len);
    struct tcphdr *hdr = (struct tcphdr *)send_buffer;

    /* following is send in little endian */
    hdr->source = sock->local_port;
    hdr->dest = sock->remote_port;
    hdr->seq = seg->seq;
    hdr->doff = 5;
    /* above is send in little endian */

    compute_tcp_checksum(hdr);

    void *data = tcp_raw_content(send_buffer);
    memcpy(data, seg->data, seg->len);

    struct in_addr src, dst;
    src.s_addr = sock->local_addr;  // big endian
    dst.s_addr = sock->remote_addr; // big endian

    sendIPPacket(src, dst, IPPROTO_TCP, send_buffer, total_len);

    if (TCP_DATA_DEBUG) {
        char src_ip[20], dst_ip[20];
        inet_ntop(AF_INET, &sock->local_addr, src_ip, 20);
        inet_ntop(AF_INET, &sock->remote_addr, dst_ip, 20);
        fprintf(stderr, "send packet, from %s:%d ---> %s:%d\n", src_ip,
                sock->local_port, dst_ip, sock->remote_port);
    }

    free(send_buffer);
    return 0;
}

static int tcp_send_ctrl_packet(struct socket_info_t *sock, uint16_t syn,
                                uint32_t seq, uint16_t ack, uint32_t ack_seq) {
    int ret;
    size_t total_len = sizeof(struct tcphdr);
    uint8_t *send_buffer = malloc(total_len);
    memset(send_buffer, 0, total_len);
    struct tcphdr *hdr = (struct tcphdr *)send_buffer;

    /* following is send in little endian */
    hdr->source = sock->local_port;
    hdr->dest = sock->remote_port;
    hdr->syn = syn;
    hdr->seq = seq;
    hdr->ack = ack;
    hdr->ack_seq = ack_seq;
    hdr->doff = 5;
    /* above is send in little endian */

    compute_tcp_checksum(hdr);

    struct in_addr src, dst;
    src.s_addr = sock->local_addr;  // big endian
    dst.s_addr = sock->remote_addr; // big endian

    sendIPPacket(src, dst, IPPROTO_TCP, send_buffer, total_len);

    if (TCP_DATA_DEBUG) {
        char src_ip[20], dst_ip[20];
        inet_ntop(AF_INET, &sock->local_addr, src_ip, 20);
        inet_ntop(AF_INET, &sock->remote_addr, dst_ip, 20);
        fprintf(stderr, "send packet, from %s:%d ---> %s:%d\n", src_ip,
                sock->local_port, dst_ip, sock->remote_port);
    }

    free(send_buffer);
    return 0;
}

static int tcp_send_syn(struct socket_info_t *sock, uint32_t seq) {
    return tcp_send_ctrl_packet(sock, 1, seq, 0, 0);
}
static int tcp_send_ack(struct socket_info_t *sock, uint32_t ack_seq) {
    return tcp_send_ctrl_packet(sock, 0, 0, 1, ack_seq);
}

static int tcp_callback_accept(struct socket_info_t *s) {
    struct port_info_t *port = &(s->ifa->port_info[s->local_port]);
    struct pending_connection_t *conn = rb_front(port->conn_queue);
    rb_pop(port->conn_queue);

    int sid_data = new_socket(SOCKTYPE_DATA, SOCKSTATE_SYN_RECEIVED);
    struct socket_info_t *s_data = &sock_info[sid_data];

    s_data->local_addr = s->local_addr;
    s_data->local_port = s->local_port;
    s_data->remote_addr = conn->remote_addr;
    s_data->remote_port = conn->remote_port;
    s_data->irs = conn->irs;
    s_data->rcv_nxt = conn->irs + 1;

    s_data->iss = 1;
    s_data->snd_una = 1;
    s_data->snd_nxt = 2;

    free(conn);

    tcp_send_ctrl_packet(s_data, 1, s_data->iss, 1, s_data->rcv_nxt);

    s->callback_state = CALLBACK_NONE;
    s_data->callback_state = CALLBACK_SENT_SYNACK;
    s_data->res_f = s->res_f;
    s->res_f = NULL;

    return 0;
}

static int tcp_callback_sent_synack(struct socket_info_t *s) {
    int ret;

    fprintf(s->res_f, "%d %x %d\n", 0, s->remote_addr,
            s->remote_port);
    fflush(s->res_f);

    ret = fclose(s->res_f);
    CPEL(ret < 0);

    return 0;
}

static int handle_tcp(const void *frame, size_t len, struct iface_t *iface,
                      uint32_t remote_ip) {
    int ret;
    const struct tcphdr *hdr = frame;
    const void *data = tcp_raw_content_const(frame);
    int data_len = len - tcp_hdr_len(frame);
    uint16_t s_port = ntohs(hdr->source), d_port = ntohs(hdr->dest);

    struct port_info_t *port_info = &iface->port_info[d_port];

    int sock_id = -1;
    for (int i = 0; i != port_info->data_socket_cnt; ++i) {
        int sid = port_info->data_socket[i];
        if (sock_info[sid].remote_addr == remote_ip &&
            sock_info[sid].remote_port == s_port) {
            sock_id = sid;
            break;
        }
    }

    if (sock_id != -1) { // new data
        struct socket_info_t *sock = &sock_info[sock_id];
        if (sock->state == SOCKSTATE_ESTABLISHED) {
            // TODO: consider piggyback
            if (hdr->ack) {                          // ACK
                if (hdr->ack_seq == sock->snd_nxt) { // TODO: sliding window
                    if ((void **)sock->nxt_send !=
                        sock->out_buf->data + sock->out_buf->tail) {
                        struct segment_t *nxt_send = *(sock->nxt_send);

                        sock->snd_nxt = nxt_send->seq + nxt_send->len;
                        sock->snd_una = nxt_send->seq;

                        tcp_send_data(nxt_send, sock);
                        sock->nxt_send =
                            rb_nxt(sock->out_buf, (void **)sock->nxt_send);
                    }
                }
            } else { // NORMAL
                if (hdr->seq != sock->rcv_nxt) return -1;
                struct segment_t *seg = new_segment(data_len, hdr->seq);
                memcpy(seg->data, data, data_len);

                // buffer the packet
                ret = rb_push(sock->in_buf, seg);
                if (ret == -1) return -1;
                sock->rcv_nxt = seg->seq + seg->len;

                // send ack
                tcp_send_ack(sock, sock->rcv_nxt);
            }
        } else if (sock->state == SOCKSTATE_SYN_SENT) {
            if (hdr->syn && hdr->ack && hdr->ack_seq == sock->snd_nxt) {
                // send ack
                sock->irs = hdr->seq;
                sock->rcv_nxt = hdr->seq + 1;

                tcp_send_ack(sock, sock->rcv_nxt);

                sock->state = SOCKSTATE_ESTABLISHED;
            } else if (hdr->syn) {
                // send ack
                sock->irs = hdr->seq;
                sock->rcv_nxt = hdr->seq + 1;

                tcp_send_ack(sock, sock->rcv_nxt);

                sock->state = SOCKSTATE_SYN_RECEIVED;
            }
        } else if (sock->state == SOCKSTATE_SYN_RECEIVED) {
            if (hdr->ack && hdr->ack_seq == sock->snd_nxt) {
                // send ack
                sock->irs = hdr->seq;
                sock->rcv_nxt = hdr->seq + 1;

                tcp_send_ack(sock, sock->rcv_nxt);

                sock->state = SOCKSTATE_ESTABLISHED;

                if (sock->callback_state == CALLBACK_SENT_SYNACK) {
                    tcp_callback_sent_synack(sock);
                } else if (sock->callback)
            }
        }
    } else {
        if (port_info->connection_socket != -1 && hdr->syn) { // new connection
            struct socket_info_t *sock =
                &sock_info[port_info->connection_socket];
            if (sock->state != SOCKSTATE_LISTEN) return -1;

            struct pending_connection_t *conn =
                malloc(sizeof(struct pending_connection_t));
            *conn = (struct pending_connection_t){
                .remote_addr = remote_ip,
                .remote_port = hdr->source,
                .irs = hdr->seq,
            };

            // buffer the packet
            ret = rb_push(port_info->conn_queue, conn);
            if (ret == -1) return -1;

            if (sock->callback_state == CALLBACK_ACCEPT) {
                tcp_callback_accept(sock);
            }
        } else { // drop it
            return -1;
        }
    }

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
            DRECV("Recv message from %x", in_addr_src.s_addr);

            size_t data_len = len - ip_hdr_len(frame);

            fwrite(data, 1, data_len, stdout);
            fwrite("\n", 1, 1, stdout);

            if (hdr->protocol == IPPROTO_TCP) {
                handle_tcp(data, data_len, iface, hdr->saddr);
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

static void routine() {
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
int socket_id_cnt = 0;

struct iface_t interfaces[MAX_DEVICES];

struct socket_info_t sock_info[MAX_SOCKET];

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

    // init sock_info[]
    for (int i = 0; i != MAX_SOCKET; ++i) {
        sock_info[i].valid = 0;
    }

    // init interfaces[]
    for (int i = 0; i != total_dev; ++i) {
        interfaces[i].ip = dev_IP[i];
        for (int j = 0; j != MAX_PORT; ++j) {
            interfaces[i].port_info[j].connection_socket = -1;
            interfaces[i].port_info[j].data_socket_cnt = 0;
            memset(interfaces[i].port_info[j].data_socket, 0,
                   sizeof(interfaces[i].port_info[i].data_socket));
            interfaces[i].port_info[j].conn_queue = rb_new(16);
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int ret, opt;

    // ======================== handle arguments =============================

    FILE *action_file = NULL;

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

    // =============================== init ==================================

    ret = mkfifo(REQ_PIPE_NAME, 0666);
    CPEL(ret == -1 && errno != EEXIST);

    int req_fd = open(REQ_PIPE_NAME, O_RDONLY | O_NONBLOCK);
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(req_fd, &fds);
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

    // ======================= start the real work ===========================

    while (1) {
        usleep(100000);

        routine();

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

                int sid = new_socket(SOCKTYPE_CONNECTION, SOCKSTATE_UNBOUNDED);

                result = sid;
            } else if (!strcmp(cmd, "bind")) {
                int sid;
                uint32_t ip_addr;
                uint16_t port;
                fscanf(req_f, "%d%x%hu", &sid, &ip_addr, &port);

                if (PRINT_COMMAND)
                    DPIPE("Command: bind %d %x %d", sid, ip_addr, port);

                struct socket_info_t *s = &sock_info[sid];
                struct iface_t *ifa = find_iface_by_ip(ip_addr);

                ret = can_bind(s, ip_addr, port);
                if (ret < 0) {
                    result = ret;
                    break;
                }

                s->local_addr = ip_addr;
                s->local_port = port;
                s->state = SOCKSTATE_BINDED;
                s->ifa = ifa;

                ifa->port_info[port].connection_socket = sid;

                result = 0;
            } else if (!strcmp(cmd, "listen")) {
                int sid, backlog;
                fscanf(req_f, "%d%d", &sid, &backlog);

                if (PRINT_COMMAND) DPIPE("Command: listen %d %d", sid, backlog);

                struct socket_info_t *s = &sock_info[sid];

                ret = can_listen(s);
                if (ret < 0) {
                    result = ret;
                    break;
                }

                s->state = SOCKSTATE_LISTEN;

                result = 0;
            } else if (!strcmp(cmd, "connect")) {
                int sid;
                uint32_t ip_addr;
                uint16_t port;
                fscanf(req_f, "%d%x%hu", &sid, &ip_addr, &port);

                if (PRINT_COMMAND)
                    DPIPE("Command: connect %d %x %d", sid, ip_addr, port);

                struct socket_info_t *s = &sock_info[sid];

                ret = can_connect(s);
                if (ret < 0) {
                    result = ret;
                    break;
                }

                s->type = SOCKTYPE_DATA;
                if (s->state == SOCKSTATE_UNBOUNDED) {
                    struct neigh_record *rec = linkstate_next_hop(ls, ip_addr);
                    if (rec == NULL) {
                        result = -1;
                        break;
                    }

                    struct iface_t *ifa = &interfaces[rec->port];
                    s->local_addr = ifa->ip;
                    allocate_free_port(ifa, &(s->local_port));
                    s->ifa = ifa;

                    struct port_info_t *ifa_port =
                        &(ifa->port_info[s->local_port]);
                    ifa_port->data_socket[ifa_port->data_socket_cnt++] = sid;

                    s->state = SOCKSTATE_BINDED;
                }

                s->iss = 1;
                s->snd_una = 1;
                s->snd_nxt = 2;

                tcp_send_syn(s, s->iss);
                s->state = SOCKSTATE_SYN_SENT;

                while (s->state != SOCKSTATE_ESTABLISHED) {
                    usleep(10000);
                }

                result = 0;
            } else if (!strcmp(cmd, "accept")) {
                int sid;
                fscanf(req_f, "%d", &sid);

                if (PRINT_COMMAND) DPIPE("Command: accept %d", sid);

                struct socket_info_t *s = &sock_info[sid];

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

                struct port_info_t *port = &(s->ifa->port_info[s->local_port]);
                if (!rb_empty(port->conn_queue)) {
                    tcp_callback_accept(s);
                } else {
                    s->callback_state = CALLBACK_ACCEPT;
                }

                special_return = 1;
            } else if (!strcmp(cmd, "read")) {
            } else if (!strcmp(cmd, "write")) {
            } else if (!strcmp(cmd, "close")) {
            } else if (!strcmp(cmd, "nop")) {

                if (PRINT_COMMAND) DPIPE("Command: nop");

                result = 0x7fffffff;

                break;
            }
        } while (0);

        if (special_return) continue;

        // ================= then return the result =======================
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