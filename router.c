#include "arp.h"
#include "device.h"
#include "ip.h"
#include "link_state.h"
#include "utils.h"

#include "uthash/uthash.h"
#include <assert.h>
#include <getopt.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_NETWORK_SIZE 16
#define MAX_PORT 16

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

// AS record
int router_gid;
struct LinkState *ls;

struct ip_record {
    uint32_t gid;
    uint32_t ip;
    UT_hash_handle hh_ip;
};
struct ip_record *ip2gid;
struct ip_record *new_ip_record(uint32_t ip, uint32_t gid) {
    struct ip_record *rec = malloc(sizeof(struct ip_record));
    memset(rec, 0, sizeof(struct ip_record));
    rec->gid = gid;
    rec->ip = ip;
    return rec;
}
static inline void add_ip_record(struct ip_record *rec) {
    HASH_ADD(hh_ip, ip2gid, ip, sizeof(int), rec);
}
static inline uint32_t query_gid_by_ip(uint32_t ip) {
    struct ip_record *rec;
    HASH_FIND(hh_ip, ip2gid, &ip, sizeof(int), rec);
    if (rec)
        return rec->gid;
    else
        return -1;
}

// neighbor info
struct neigh_record {
    uint32_t ip;
    uint32_t gid;
    int port;
    int dis;
    struct MAC_addr mac;
};
struct neigh_record neigh[MAX_PORT][MAX_NETWORK_SIZE];
int neigh_size[MAX_PORT];

struct neigh_record *get_neigh_by_gid(int gid) {
    for (int i = 0; i != total_dev; ++i)
        for (int j = 0; j != neigh_size[i]; ++j) {
            if (neigh[i][j].gid == gid) return &neigh[i][j];
        }
    return NULL;
}

// port info
char dev_name[MAX_PORT][MAX_DEVICE_NAME];
int dev_cnt;
int dev_id[MAX_PORT];

void process_link_state(void *data);

void initiate_broadcast(void *buf, int len) {
    ++bc_id;

    // use port 0 to broadcast, TODO: find a more elegant way
    // for (int i = 0; i != total_dev; ++i) {
    struct in_addr bc_src;
    bc_src.s_addr = dev_IP[0];

    struct bc_record *set_bc_id =
        new_bc_record(bc_src.s_addr, bc_id, 0); // TODO: add timestamp
    HASH_ADD_PTR(bc_set, id, set_bc_id);

    broadcastIPPacket(bc_src, MY_CONTROL_PROTOCOl, buf, len, bc_id);
    // }
}

void handle_broadcast(struct iphdr *hdr, const uint8_t *data, int len) {
    uint64_t bc_id = ((uint64_t)hdr->saddr << 32) + hdr->id;
    struct bc_record *bc_rec;
    HASH_FIND_PTR(bc_set, &bc_id, bc_rec);

    if (bc_rec) return; // seen this packet before

    struct bc_record *set_bc_id =
        new_bc_record(hdr->saddr, hdr->id, 0); // TODO: add timestamp
    HASH_ADD_PTR(bc_set, id, set_bc_id);

    if (hdr->protocol == MY_CONTROL_PROTOCOl) {
        uint32_t my_protocol_type = *(uint32_t *)data;
        if (my_protocol_type == PROTO_LINKSTATE) {
            process_link_state((uint32_t *)data);
        }
    } else {
        DRECV("Unknown broadcast packet from %x", hdr->saddr);
    }

    // DSEND("Forward broadcast from %d.%d.%d.%d, id: %d",
    // GET1B(&hdr->saddr,3),GET1B(&hdr->saddr,2),GET1B(&hdr->saddr,1),GET1B(&hdr->saddr,0),
    // hdr->id);

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

            int t_gid = query_gid_by_ip(hdr->daddr);
            int nxt = t_gid != -1 ? linkstate_next_hop(ls, t_gid) : -1;
            if (nxt != -1) { // forward it
                struct in_addr in_addr_nxt;
                struct neigh_record *rec = get_neigh_by_gid(nxt);
                int nxt_port = rec->port;
                in_addr_nxt.s_addr = rec->ip;

                DSEND("Forward from %x to %x, to port %d", hdr->saddr,
                      hdr->daddr, nxt_port);
                sendIPPacket(in_addr_src, in_addr_dst, hdr->protocol, data,
                             len - ip_hdr_len(frame));
            } else { // drop it
                DSEND("Drop packet from %x to %x", hdr->saddr, hdr->daddr);
            }
        }
    } while (0);

    return 0;
}

/*****
 * @brief update neigh info from arp table, then
 * update native linkstate record based on neigh info
 */
void update_neigh_info() {
    uint64_t cur_time = gettime_ms();
    uint32_t neigh_count = 0;

    for (int i = 0; i != total_dev; ++i) {
        struct arp_record *rec, *tmp;
        int j = 0;
        HASH_ITER(hh, arp_t[i]->table, rec, tmp) {

            // if (cur_time - rec->timestamp > ARP_EXPIRE) continue;

            uint32_t t_gid = query_gid_by_ip(rec->ip_addr);
            if (t_gid == -1) continue;
            neigh[i][j].ip = rec->ip_addr;
            neigh[i][j].port = i;
            neigh[i][j].gid = t_gid;
            neigh[i][j].dis = 1;
            neigh[i][j].mac = rec->mac_addr;
            ++j;
        }
        neigh_size[i] = j;
        neigh_count += j;
    }

    struct linkstate_record *rec = new_linkstate_record(total_dev, neigh_count);
    rec->gid = router_gid;

    for (int i = 0; i != total_dev; ++i) {
        rec->ip_list[i] = dev_IP[i];
        rec->ip_mask_list[i] = dev_IP_mask[i];
    }

    for (int i = 0, k = 0; i != total_dev; ++i) {
        for (int j = 0; j != neigh_size[i]; ++j) {
            rec->link_list[k] = neigh[i][j].gid;
            rec->dis_list[k++] = neigh[i][j].dis;
        }
    }

    linkstate_add_rec(ls, rec);
}

// Corner Case: might receive remote linkstate before neighbor
void process_link_state(void *data) {
    uint32_t s_gid = GET4B(data, 8);
    int j = 16;

    DRECV("Received linkstate info from %u", s_gid);

    int neigh_count = GET2B(data, 4);
    int ip_count = GET2B(data, 6);

    struct linkstate_record *rec = new_linkstate_record(ip_count, neigh_count);
    rec->gid = s_gid;

    // acquire ip info
    for (int i = 0; i != ip_count; ++i, j += 8) {
        uint32_t s_ip = GET4B(data, j);
        if (query_gid_by_ip(s_ip) == -1) {
            add_ip_record(new_ip_record(s_ip, s_gid));
        }
        rec->ip_list[i] = s_ip;
        rec->ip_mask_list[i] = GET4B(data, j + 4);
    }

    // acquire link info
    for (int i = 0; i != neigh_count; ++i, j += 8) {
        uint32_t t_gid = GET4B(data, j);
        uint32_t dis = GET4B(data, j + 4);
        rec->link_list[i] = t_gid;
        rec->dis_list[i] = dis;
    }

    linkstate_add_rec(ls, rec);
}

/****
 *   -0------------16----------32----------48----------64
 *   -0------------2-----------4-----------6-----------8
 *   0| PROTO_LINKSTATE        |neigh_count| ip_count  |
 *   8| router_gid             | reserved              |
 *  16| ip[1]                  | ip_mask[1]            |
 *    | .....                                          |
 *    | ip[ip_count]           | ip_mask[ip_count]     |
 *    | neigh[1].addr          | neigh[cnt].dis        |
 *    | .....                                          |
 *    | neigh[neigh_count].addr| ~~~.dis               |
 *    |
 * */
void send_link_state() {
    update_neigh_info();

    int ip_count = total_dev;
    int neigh_count = 0;
    for (int i = 0; i != total_dev; ++i) {
        neigh_count += neigh_size[i];
    }

    int len = (neigh_count + ip_count) * sizeof(int) * 2 + 16;

    void *buf = malloc(len);

    PUT4B(buf, 0, PROTO_LINKSTATE);
    PUT2B(buf, 4, neigh_count);
    PUT2B(buf, 6, ip_count);

    PUT4B(buf, 8, router_gid);
    PUT4B(buf, 12, 0);

    int j = 16;

    for (int i = 0; i != ip_count; ++i) {
        PUT4B(buf, j, dev_IP[i]);
        PUT4B(buf, j + 4, dev_IP_mask[i]);
        j += 8;
    }

    for (int i = 0; i != total_dev; ++i) {
        for (int k = 0; k != neigh_size[i]; ++k) {
            uint32_t t_gid = neigh[i][k].gid;
            PUT4B(buf, j, t_gid);
            PUT4B(buf, j + 4, 1);
            j += 8;
        }
    }

    DSEND("Advertise link state info with gid %u", router_gid);
    initiate_broadcast(buf, len);

    free(buf);
}

void update_routing_table() {
    struct ip_record *rec, *tmp;
    HASH_ITER(hh_ip, ip2gid, rec, tmp) {
        int port, next_hop;

        next_hop = linkstate_next_hop(ls, rec->gid);
        if (next_hop == -1) continue;

        struct neigh_record *neigh_rec = get_neigh_by_gid(next_hop);
        if (!neigh_rec) continue;

        port = neigh_rec->port;

        struct in_addr dst, mask;
        dst.s_addr = rec->ip;
        mask.s_addr = 0xffffffff;

        setRoutingTable(dst, mask, neigh_rec->mac.data, dev_names[port]);
    }
}

void routine() {
    static int tick = 0;
    fprintf(stderr, "===============time: %u==================\n", ++tick);

    for (int i = 0; i != total_dev; ++i)
        ARP_advertise(i);

    // update linkstate, then update routing table based on linkstate
    linkstate_update(ls);

    update_routing_table();

    send_link_state();

    usleep(1000000);
}

int router_init() {
    int ret;

    // init devices
    ret = device_init();
    RCPE(ret == -1, -1, "Error initializing devices");
    for (int i = 0; i != dev_cnt; ++i) {
        dev_id[i] = addDevice(dev_name[i]);
        CPE(dev_id[i] == -1, "Error adding device", dev_id[i]);
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

    // TODO: detect collision
    router_gid = random_ex();

    // init linkstate
    ls = malloc(sizeof(struct LinkState));
    linkstate_init(ls);

    update_neigh_info();

    fprintf(stderr, "Router Initialized, with GID=%u\n", router_gid);

    return 0;
}

int main(int argc, char *argv[]) {
    int ret, opt;

    FILE *action_file = NULL;

    while ((opt = getopt(argc, argv, "d:f:")) != -1) { // only support -d
        switch (opt) {
        case 'd':
            strncpy(dev_name[dev_cnt], optarg, MAX_DEVICE_NAME);
            ++dev_cnt;
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