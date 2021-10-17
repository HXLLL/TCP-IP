#include "device.h"
#include "ip.h"
#include "utils.h"

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

struct neigh_record {
    uint32_t addr;
    uint32_t id;
    UT_hash_handle hh_addr;
    UT_hash_handle hh_id;
};
struct neigh_record *new_neigh_record(uint32_t addr, uint32_t id) {
    struct neigh_record* rec = malloc(sizeof(struct neigh_record));
    memset(rec, 0, sizeof(struct neigh_record));
    rec->addr = addr;
    rec->id = id;
    return rec;
}

char dev_name[MAX_PORT][MAX_DEVICE_NAME];
int dev_cnt;
int dev_id[MAX_PORT];
struct sockaddr_in IP_addr; // big endian
struct bc_record *bc_set;
struct neigh_record *neigh_ht_by_addr, *neigh_ht_by_id;
int d[MAX_NETWORK_SIZE];
int net_size;

int ip_callback(const void *frame, int len) {
    printf("Receive data length: %d\n", len);
    struct iphdr *hdr = (struct iphdr *)frame;
    const uint8_t *data = ip_raw_content_const(frame);

    if (hdr->daddr == IP_addr.sin_addr.s_addr) {
        fwrite(data, 1, len - ip_hdr_len(frame), stdout);
        fwrite("\n", 1, 1, stdout);
    } else if (hdr->daddr == 0xffffffff) { // broadcast
        uint64_t bc_id = (uint64_t)hdr->daddr << 32 + hdr->id;
        struct bc_record *bc_rec;
        HASH_FIND_PTR(bc_set, &bc_id, bc_rec);
        if (!bc_rec) { // not broadcasted before
            struct bc_record *set_bc_id = new_bc_record(hdr->daddr, hdr->id, 0);        // TODO: add timestamp
            HASH_ADD_PTR(bc_set, id, set_bc_id);
        }

        struct neigh_record *it, *tmp;
        HASH_ITER(hh_id, neigh_ht_by_id, it, tmp) {
            struct in_addr dst_addr;
            dst_addr.s_addr = it->addr;
            /************************** broadcast ***************************/
        }
    } else { // forward
            /************************** forward *****************************/
    }

    return 0;
}

void send_distance_vector() {

}

int router_init() {
    int ret;

    ret = get_IP(
        dev_id[0],
        (struct sockaddr *)&IP_addr); // TODO: support multiple ip address
    RCPE(ret == -1, -1, "Error getting IP");

    bc_set = NULL;

    neigh_ht_by_addr = NULL;
    neigh_ht_by_id = NULL;

    net_size = 1;

    return 0;
}

int main(int argc, char *argv[]) {
    int ret, opt;

    while ((opt = getopt(argc, argv, "d:")) != -1) { // only support -d
        switch (opt) {
        case 'd':
            strncpy(dev_name[dev_cnt], optarg, MAX_DEVICE_NAME);
            ++dev_cnt;
            break;
        default:
            printf("Usage: %s -d [device]\n", argv[0]);
            return -1;
        }
    }

    my_init();
    for (int i = 0; i != dev_cnt; ++i) {
        dev_id[i] = addDevice(dev_name[i]);
        CPE(dev_id[i] == -1, "Error adding device", dev_id[i]);
    }

    ret = router_init();
    CPE(ret == -1, "Error initiating router", ret);

    setIPPacketReceiveCallback(ip_callback);

    while (1) {
        usleep(1000000);
        send_distance_vector();
    }
}