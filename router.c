#include "arp.h"
#include "device.h"
#include "ip.h"
#include "link_state.h"
#include "utils.h"

#include <assert.h>
#include "uthash/uthash.h"
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
struct host_record *host_by_id, *host_by_gid;
struct ip_record *ip2gid;
struct LinkState *ls;
int net_size;
#include "host_table.h" // to split file

// neighbor info
struct neigh_record {
    uint32_t ip;
    uint32_t gid;
    int port;
    int dis;
};
struct neigh_record neigh[MAX_PORT][MAX_NETWORK_SIZE];
int neigh_size[MAX_PORT];
struct neigh_record *next_hop_port[MAX_NETWORK_SIZE];

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

    if (bc_rec) return;

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

    DSEND("Forward broadcast from %x, id: %d", hdr->saddr, hdr->id);
    struct in_addr src;
    src.s_addr = hdr->saddr;
    broadcastIPPacket(src, hdr->protocol, data,
                      len - ip_hdr_len((uint8_t *)hdr), hdr->id);
}

int ip_callback(const void *frame, int len) {
    struct iphdr *hdr = (struct iphdr *)frame;
    const uint8_t *data = ip_raw_content_const(frame);
    struct in_addr in_addr_src, in_addr_dst;
    in_addr_src.s_addr = hdr->saddr;
    in_addr_dst.s_addr = hdr->daddr;

    do {
        int flag = -1;
        for (int i = 0; i != total_dev; ++i)
            if (hdr->daddr == dev_IP[i]) flag = i;
        if (flag != -1) {
            DRECV("Recv message from %x", in_addr_src.s_addr);

            fwrite(data, 1, len - ip_hdr_len(frame), stdout);
            fwrite("\n", 1, 1, stdout);
        } else if (hdr->daddr == 0xffffffff) { // broadcast
            handle_broadcast(hdr, data, len);
        } else { // forward
            int t_gid = query_gid_by_ip(hdr->daddr);
            int tid = t_gid != -1 ? query_id(t_gid) : -1;
            if (tid != -1 && linkstate_next_hop(ls, tid) != -1) { // forward it
                int nxt = linkstate_next_hop(ls, tid);       // neigh id
                assert(next_hop_port[nxt] != NULL);
                struct in_addr in_addr_nxt;
                int nxt_port = next_hop_port[nxt]->port;
                in_addr_nxt.s_addr = next_hop_port[nxt]->ip;

                DSEND("Forward from %x to %x, to port %d", hdr->saddr, hdr->daddr, nxt_port);
                sendIPPacket(in_addr_src, in_addr_dst,
                             hdr->protocol, data, len - ip_hdr_len(frame));
            } else { // drop it
                DSEND("Drop packet from %x to %x", hdr->saddr, hdr->daddr);
            }
        }
    } while (0);

    return 0;
}

void update_neigh_info() {
    for (int i=0;i!=total_dev;++i) {
        neigh_size[i] = HASH_COUNT(arp_t[i]->table);
        struct arp_record *rec, *tmp;
        int j = 0;
        HASH_ITER(hh, arp_t[i]->table, rec, tmp) {
            uint32_t t_gid = query_gid_by_ip(rec->ip_addr);
            if (t_gid == -1) continue;
            neigh[i][j].ip = rec->ip_addr;
            neigh[i][j].port = i;
            neigh[i][j].gid = t_gid;
            neigh[i][j].dis = 1;
            ++j;
        }
    }
}

void process_link_state(void *data) {
    update_neigh_info();

    uint32_t s_gid = GET4B(data, 8);

    DRECV("Received linkstate info from %u", s_gid);

    int neigh_count = GET2B(data, 4);
    int ip_count = GET2B(data, 6);

    int sid, tid;
    struct host_record *res;
    HASH_FIND(hh_gid, host_by_gid, &s_gid, sizeof(uint32_t), res);
    sid = res ? res->id : add_host(s_gid);

    HASH_FIND(hh_gid, host_by_gid, &s_gid, sizeof(uint32_t), res);
    memset(res->ip_list, 0, sizeof(res->ip_list));
    memset(res->ip_mask_list, 0, sizeof(res->ip_mask_list));
    res->ip_count = 0;

    int j = 16;

    for (int i = 0; i != ip_count; ++i, j += 8) {
        uint32_t s_ip = GET4B(data, j);
        if (query_gid_by_ip(s_ip) == -1) {
            add_ip_record(new_ip_record(s_ip, s_gid));
        }
        res->ip_list[i] = s_ip;
        res->ip_mask_list[i] = GET4B(data, j + 4);
    }

    for (int i = 0; i != net_size; ++i)
        ls->c[i][sid] = ls->c[sid][i] = -1;
    for (int i=0;i!=net_size;++i) {
        ls->c[i][0] = ls->c[0][i] = -1;
        next_hop_port[i] = NULL;
    }

    for (int i=0;i!=total_dev;++i)
        for (int j=0;j!=neigh_size[i];++j) {
            HASH_FIND(hh_gid, host_by_gid, &neigh[i][j].gid, sizeof(uint32_t), res);
            assert(res != NULL);
            tid = res->id;
            ls->c[0][tid] = ls->c[tid][0] = neigh[i][j].dis;
            next_hop_port[tid] = &neigh[i][j];
        }

    for (int i = 0; i != neigh_count; ++i, j += 8) {
        uint32_t t_gid = GET4B(data, j);
        uint32_t dis = GET4B(data, j + 4);
        HASH_FIND(hh_gid, host_by_gid, &t_gid, sizeof(uint32_t), res);
        if (res) {
            tid = res->id;
            ls->c[sid][tid] = ls->c[tid][sid] = dis; // assume links are
        }                                            // bi-directional
    }

    linkstate_SPFA(ls);
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

    int neigh_count = 0;
    for (int i = 0; i != total_dev; ++i) {
        neigh_count += neigh_size[i];
    }

    int ip_count = total_dev;

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
        for (int k=0;k!=neigh_size[i];++k) {
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

int router_init() {
    int ret;

    ret = device_init();
    RCPE(ret == -1, -1, "Error initializing devices");
    for (int i = 0; i != dev_cnt; ++i) {
        dev_id[i] = addDevice(dev_name[i]);
        CPE(dev_id[i] == -1, "Error adding device", dev_id[i]);
    }

    ret = ip_init();
    RCPE(ret == -1, -1, "Error initializing ip");

    bc_set = NULL;

    host_by_gid = NULL;
    host_by_id = NULL;

    // TODO: detect collision
    router_gid = random_ex();

    net_size = 1;
    struct host_record *host = new_host_record(0, 0);
    add_host_record(host);

    ls = malloc(sizeof(struct LinkState));
    linkstate_init(ls, MAX_NETWORK_SIZE);

    ret = arp_init();
    RCPE(ret == -1, -1, "Error initializing ARP");

    for (int i = 0; i != total_dev; ++i) {
        new_arp_table(i);
    }

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
            break;
        default:
            printf("Usage: %s -d [device]\n", argv[0]);
            return -1;
        }
    }

    ret = router_init();
    CPE(ret == -1, "Error initiating router", ret);

    setIPPacketReceiveCallback(ip_callback);

    uint32_t tick = 0;
    if (action_file) {
        int n;
        fscanf(action_file, "%d", &n);
        for (int i = 1; i <= n; ++i) {
            char act[20];
            scanf("%s", act);
            if (!strcmp(act, "sleep")) {
                double duration;
                fscanf(action_file, "%lf", &duration);
                usleep(duration * 1000000);
            } else if (!strcmp(act, "send")) {
                char ip[20], msg[255];
                scanf("%s %s", ip, msg);
                struct in_addr dst;
                inet_pton(AF_INET, ip, &dst);

                return 0;

                int len = strlen(msg);
            }
            usleep(1000000);
        }
    } else {
        while (1) {
            fprintf(stderr, "===============time: %u==================\n", ++tick);
            usleep(1000000);
            for (int i = 0; i != total_dev; ++i) {
                ARP_advertise(i);
            }
            send_link_state();
        }
    }
}