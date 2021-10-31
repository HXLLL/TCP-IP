#include "arp.h"
#include "device.h"
#include "ip.h"
#include "link_state.h"
#include "utils.h"
// #include "debug_utils.h"

#include "uthash/uthash.h"
#include <assert.h>
#include <getopt.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BROADCAST_EXPIRE 500
#define MAX_NETWORK_SIZE 16
#define MAX_PORT 16

const int LINKSTATE_ADVERTISE_DEBUG = 0;
const int LINKSTATE_RECV_DEBUG = 0;
const int FORWARD_DROP_DEBUG = 1;
const int BROADCAST_DEBUG = 0;

const int RT_DEBUG_DUMP = 0;
const int ARP_DEBUG_DUMP = 0;

struct bc_record {
    uint64_t id;
    uint64_t timestamp;
    UT_hash_handle hh;
};
struct bc_record *new_bc_record(uint32_t addr, uint16_t id,
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

// LinkState Structure
struct LinkState *ls;

// port info
char p_dev_name[MAX_PORT][MAX_DEVICE_NAME];
int p_dev_cnt;
int p_dev_id[MAX_PORT];
uint32_t p_gid = -1;

void process_link_state(void *data);

void initiate_broadcast(void *buf, int len) {
    ++bc_id;
    uint64_t cur_time = gettime_ms();

    struct in_addr bc_src;
    bc_src.s_addr = dev_IP[0];

    struct bc_record *set_bc_id =
        new_bc_record(bc_src.s_addr, bc_id, cur_time);
    HASH_ADD_PTR(bc_set, id, set_bc_id);

    broadcastIPPacket(bc_src, MY_CONTROL_PROTOCOl, buf, len, bc_id);
}

void handle_broadcast(struct iphdr *hdr, const uint8_t *data, int len) {
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
            process_link_state((uint32_t *)data);
        }
    } else {
        DRECV("Unknown broadcast packet from %x", hdr->saddr);
    }

    if (BROADCAST_DEBUG) {
        DSEND("Forward broadcast from %d.%d.%d.%d, id: %d",
        GET1B(&hdr->saddr,3),GET1B(&hdr->saddr,2),GET1B(&hdr->saddr,1),GET1B(&hdr->saddr,0),
        hdr->id);
    }

    struct in_addr src;
    src.s_addr = hdr->saddr;
    broadcastIPPacket(src, hdr->protocol, data,
                      len - ip_hdr_len((uint8_t *)hdr), hdr->id);
}

int is_for_me(uint32_t addr) {
    int flag = -1;
    for (int i = 0; i != total_dev; ++i)
        if (addr == dev_IP[i]) flag = i;
    return flag;
}

int ip_callback(const void *frame, int len) {
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

// Corner Case: might receive remote linkstate before neighbor
void process_link_state(void *data) {
    uint32_t s_gid = GET4B(data, 8);
    int j = 16;

    if (LINKSTATE_RECV_DEBUG) {
        DRECV("Received linkstate info from %u", s_gid);
    }

    linkstate_process_advertise(ls, data);
}

void send_link_state() {
    void *buf;

    size_t len = linkstate_make_advertisement(ls, &buf);
    if (LINKSTATE_ADVERTISE_DEBUG) {
        DSEND("Advertise link state info with gid %u", ls->router_gid);
    }

    initiate_broadcast(buf, len);

    free(buf);
}

void update_routing_table() {
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

void routine() {
    static int tick = 0;
    fprintf(stderr, "===============time: %u==================\n", ++tick);

    for (int i = 0; i != total_dev; ++i)
        ARP_advertise(i);

    // update linkstate, then update routing table based on linkstate
    linkstate_update(ls, total_dev, arp_t);

    update_routing_table();

    send_link_state();

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

    usleep(1000000);
}

int router_init() {
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

    // init linkstate
    ls = malloc(sizeof(struct LinkState));
    linkstate_init(ls, total_dev, arp_t, p_gid);

    return 0;
}

int main(int argc, char *argv[]) {
    int ret, opt;

    FILE *action_file = NULL;

    while ((opt = getopt(argc, argv, "d:f:g:")) != -1) {
        switch (opt) {
        case 'g':
            p_gid = strtol(optarg, NULL, 10);
            break;
        case 'd':
            strncpy(p_dev_name[p_dev_cnt], optarg, MAX_DEVICE_NAME);
            ++p_dev_cnt;
            break;
        case 'f':
            action_file = fopen(optarg, "r");
            CPE(action_file == NULL, "Can't open action file", -1);
            break;
        default:
            printf("Usage: %s -d [device]\n", argv[0]);
            return -1;
        }
    }

    ret = router_init();
    CPE(ret == -1, "Error initiating router", ret);

    fprintf(stderr, "Start listening...\n");
    setIPPacketReceiveCallback(ip_callback);

    if (action_file) {
        int n;
        fscanf(action_file, "%d", &n);
        for (int i = 1; i <= n; ++i) {
            char act[20];
            fscanf(action_file, "%s", act);
            do {
                if (!strcmp(act, "nop")) {
                    fprintf(stderr, "nop\n");
                    break;
                } else if (!strcmp(act, "send")) {
                    int port, len;
                    char ip[20], msg[255];
                    fscanf(action_file, "%d %s %s", &port, ip, msg);

                    struct in_addr dst;
                    inet_pton(AF_INET, ip, &dst);
                    struct in_addr src;
                    src.s_addr = dev_IP[port];

                    len = strlen(msg);

                    ret = sendIPPacket(src, dst, 100, msg, len);
                    if (ret == 0) {
                        fprintf(stderr, "send from port %d to ip %s: %s\n",
                                port, ip, msg);
                    } else {
                        fprintf(stderr, "Can't send to %s\n", ip);
                    }
                }
            } while (0);

            routine();
        }
    } else {
        while (1) {
            routine();
        }
    }
    while (1)
        ;
}