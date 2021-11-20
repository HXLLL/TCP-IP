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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>

#define ROUTINE_INTERVAL 1000 // ms
#define BROADCAST_EXPIRE 500 // ms
#define MAX_NETWORK_SIZE 16

const int LINKSTATE_ADVERTISE_DEBUG = 0;
const int LINKSTATE_RECV_DEBUG = 0;
const int FORWARD_DROP_DEBUG = 1;
const int BROADCAST_DEBUG = 0;
const int PIPE_DEBUG = 1;
const int RT_DEBUG_DUMP = 0;
const int ARP_DEBUG_DUMP = 0;

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
 *                              IP Packet Handling
 ******************************************************************************/

static int is_for_me(uint32_t addr) {
    int flag = -1;
    for (int i = 0; i != total_dev; ++i)
        if (addr == dev_IP[i]) flag = i;
    return flag;
}

static int ip_callback(const void *frame, int len) {
    int ret;
    struct iphdr *hdr = (struct iphdr *)frame;
    const uint8_t *data = ip_raw_content_const(frame);
    struct in_addr in_addr_src, in_addr_dst;
    in_addr_src.s_addr = hdr->saddr;
    in_addr_dst.s_addr = hdr->daddr;

    do {
        if (is_for_me(hdr->daddr) != -1) {
            DRECV("Recv message from %x", in_addr_src.s_addr);

            fwrite(data, 1, len - ip_hdr_len(frame), stdout);
            fwrite("\n", 1, 1, stdout);

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

struct socket_info_t sock_info[MAX_SOCKET];
struct port_info_t port_info[MAX_PORT];

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

    int req_fd = open(REQ_PIPE_NAME, O_RDONLY|O_NONBLOCK);
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
        int result = 0;
        ret = fscanf(req_f, "%s", cmd);

        // ====================== handle commands ==========================
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

                if (PIPE_DEBUG)
                    DPIPE("Command: bind %d %x %d", sid, ip_addr, port);

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

                s->state = SOCKSTATE_LISTEN; // TODO: carefully consider state
                                             // transition

                result = 0;
            } else if (!strcmp(cmd, "connect")) {
            } else if (!strcmp(cmd, "accept")) {
            } else if (!strcmp(cmd, "read")) {
            } else if (!strcmp(cmd, "write")) {
            } else if (!strcmp(cmd, "close")) {
            } else if (!strcmp(cmd, "nop")) {

                if (PIPE_DEBUG) DPIPE("Command: nop");

                result = 0x7fffffff;

                break;
            }
        } while (0);

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