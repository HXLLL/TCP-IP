//TODO: run arp 
#include "arp.h"
#include "device.h"
#include "ip.h"
#include "utils.h"
#include "link_state.h"

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
struct bc_record *new_bc_record(uint32_t addr, uint16_t id, uint64_t timestamp) {
    struct bc_record* rec = malloc(sizeof(struct bc_record));
    memset(rec, 0, sizeof(struct bc_record));
    rec->id = ((uint64_t)addr << 32) + id;
    rec->timestamp = timestamp;
    return rec;
}

struct host_record {
    uint32_t addr;
    uint32_t id;
    UT_hash_handle hh_addr;
    UT_hash_handle hh_id;
};
struct host_record *new_host_record(uint32_t addr, uint32_t id) {
    struct host_record* rec = malloc(sizeof(struct host_record));
    memset(rec, 0, sizeof(struct host_record));
    rec->addr = addr;
    rec->id = id;
    return rec;
}

char dev_name[MAX_PORT][MAX_DEVICE_NAME];
int dev_cnt;
int dev_id[MAX_PORT];
struct sockaddr_in IP_addr; // big endian
struct MAC_addr mac_addr;
struct bc_record *bc_set;
struct host_record *host_by_addr, *host_by_id;
struct LinkState *ls;
int d[MAX_NETWORK_SIZE];
int bc_id;
int net_size;

void process_link_state(uint32_t s_addr, uint32_t *data);

static inline void add_host_record(struct host_record *rec) {
    HASH_ADD(hh_id, host_by_id, id, sizeof(int), rec);
    HASH_ADD(hh_addr, host_by_addr, addr, sizeof(int), rec);
}
static inline int add_host(uint32_t addr) {
    int old_net_size = net_size;
    struct host_record *rec = new_host_record(addr, net_size++);
    add_host_record(rec);
    return net_size;
}
static inline int query_id(uint32_t addr) {
    struct host_record *rec;
    HASH_FIND(hh_addr, host_by_addr, &addr, sizeof(int), rec);
    if (rec) return rec->id;
    else return -1;
}
static inline uint32_t query_addr(int id) {
    struct host_record *rec;
    HASH_FIND(hh_id, host_by_id, &id, sizeof(int), rec);
    if (rec) return rec->addr;
    else return -1;
}

void forward_broadcast(struct iphdr *hdr, const uint8_t *data, int len) {
    uint64_t bc_id = ((uint64_t)hdr->daddr << 32) + hdr->id;
    struct bc_record *bc_rec;
    HASH_FIND_PTR(bc_set, &bc_id, bc_rec);
    if (!bc_rec) { // not broadcasted before
        struct bc_record *set_bc_id =
            new_bc_record(hdr->daddr, hdr->id, 0); // TODO: add timestamp
        HASH_ADD_PTR(bc_set, id, set_bc_id);
    }
    struct in_addr src;
    src.s_addr = hdr->saddr;
    broadcastIPPacket(src, hdr->protocol, data,
                      len - ip_hdr_len((uint8_t*)hdr), hdr->id);
}

int ip_callback(const void *frame, int len) {
    printf("Receive data length: %d\n", len);
    struct iphdr *hdr = (struct iphdr *)frame;
    const uint8_t *data = ip_raw_content_const(frame);
    struct in_addr in_addr_src, in_addr_dst;
    in_addr_src.s_addr = hdr->saddr;
    in_addr_dst.s_addr = hdr->daddr;

    do {
        if (hdr->daddr == IP_addr.sin_addr.s_addr) {
            fwrite(data, 1, len - ip_hdr_len(frame), stdout);
            fwrite("\n", 1, 1, stdout);
        } else if (hdr->daddr == 0xffffffff) { // broadcast
            if (hdr->protocol == MY_CONTROL_PROTOCOl) {
                uint32_t my_protocol_type = *(uint32_t*)data;
                if (my_protocol_type == PROTO_LINKSTATE) {
                    process_link_state(hdr->saddr, (uint32_t*)data);
                }
            }
        } else { // forward
            int tid = query_id(hdr->daddr);
            if (tid != -1 && linkstate_next_hop(ls, tid) != -1) { // forward it
                int nxt = linkstate_next_hop(ls, tid);
                struct in_addr in_addr_nxt;
                in_addr_nxt.s_addr = query_addr(nxt);
                sendIPPacket(in_addr_src, in_addr_dst, in_addr_nxt, hdr->protocol, data, len - ip_hdr_len(frame));
            } else { // drop it
                fwrite("Drop a packet", 1, 13, stdout);
                fwrite("\n", 1, 1, stdout);
            }
        }
    } while (0);

    return 0;
}

void process_link_state(uint32_t s_addr, uint32_t *data) {
    int cnt = data[1];
    data += 2;

    int sid, tid;
    struct host_record *res;
    HASH_FIND(hh_addr, host_by_addr, &s_addr, sizeof(int), res);
    if (!res) sid = add_host(s_addr);
    else sid = res->id;

    for (int i=0;i!=net_size;++i) {
        ls->c[i][sid] = ls->c[sid][i] = -1;
    }
    for (int i=0;i!=cnt;++i) {
        uint32_t addr = data[i*2];
        HASH_FIND(hh_addr, host_by_addr, &addr, sizeof(int), res);
        if (!res) tid = add_host(data[2*i]);
        else tid = res->id;
        ls->c[sid][tid] = ls->c[tid][sid] = 1; // assume links are
                                               // bi-directional
    }
    linkstate_SPFA(ls);
}

void send_link_state() {
    int count = HASH_COUNT(arp_table);
    int len=count * sizeof(int) + 8;
    uint32_t *buf = malloc(len);
    struct arp_record *it, *tmp;
    buf[0] = PROTO_LINKSTATE;
    buf[1] = count;
    int j = 0;
    HASH_ITER(hh, arp_table, it, tmp) {
        ++j;
        buf[j * 2] = it->ip_addr;
        buf[j * 2 + 1] = 1;
    }
    broadcastIPPacket(IP_addr.sin_addr, MY_CONTROL_PROTOCOl, buf, len, ++bc_id);

    free(buf);
}

int router_init() {
    int ret;


    my_init();
    for (int i = 0; i != dev_cnt; ++i) {
        dev_id[i] = addDevice(dev_name[i]);
        CPE(dev_id[i] == -1, "Error adding device", dev_id[i]);
    }

    bc_set = NULL;

    host_by_addr = NULL;
    host_by_id = NULL;

    net_size = 1;
    struct host_record *host=new_host_record(IP_addr.sin_addr.s_addr, 0);
    add_host_record(host);

    ls = malloc(sizeof(struct LinkState));
    linkstate_init(ls, MAX_NETWORK_SIZE);

    ret = arp_init(IP_addr.sin_addr.s_addr, mac_addr);
    RCPE(ret == -1, -1, "Error initializing ARP");

    return 0;
}

int main(int argc, char *argv[]) {
    int ret, opt;

    FILE* action_file = NULL;

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

    if (action_file) {
        int n; fscanf(action_file, "%d",&n);
        for (int i=1;i<=n;++i) {
            char act[20]; scanf("%s",act);
            if (!strcmp(act, "sleep")) {
                double duration;
                fscanf(action_file, "%lf", &duration);
                usleep(duration * 1000000);
            } else if (!strcmp(act, "send")) {
                char ip[20], msg[255]; scanf("%s %s", ip, msg);
                struct in_addr dst;
                inet_pton(AF_INET, ip, &dst);

                return 0;

                int len=strlen(msg);
            }
            usleep(1000000);
        }
    } else {
        while (1) {
            usleep(1000000);
            send_link_state();
        }
    }
}