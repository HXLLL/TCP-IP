#include "uthash/uthash.h"
#include "arp.h"
#include "device.h"
#include "packetio.h"
#include "utils.h"

#include <stdlib.h>

const int ARP_DEBUG = 0;

struct arp_table *arp_t[MAX_DEVICES];

struct arp_record* new_arp_record(uint32_t ip_addr, struct MAC_addr mac_addr, int port) {
    struct arp_record *rec = malloc(sizeof(struct arp_record));
    rec->ip_addr = ip_addr;
    memcpy(&rec->mac_addr, &mac_addr.data, ETH_HLEN);
    return rec;
}

int arp_frame_handler(const void* buf, int len, int id) {
    const struct ethhdr *hdr = buf;
    const struct arp_data *data = eth_raw_content_const(buf);

    if (ARP_DEBUG) {
        printf("Received ARP frame\n");
        printf("Addr: "); print_ip(stdout, data->sender_ip_addr); printf("\n");
        printf("MAC: "); print_mac(stdout, data->sender_hw_addr.data); printf("\n");
    }
    
    struct arp_record *rec;
    HASH_FIND_INT(arp_t[id]->table, &data->sender_ip_addr, rec);
    if (rec) {
        rec->mac_addr = data->sender_hw_addr;
    } else {
        new_arp_record(data->sender_ip_addr, data->sender_hw_addr, id);
        HASH_ADD_INT(arp_t[id]->table, ip_addr, rec);
    }
    rec->timestamp = gettime_ms();

    return 0;
}

int arp_init() {
    setFrameReceiveCallback(arp_frame_handler, ETH_P_ARP);

    // set function in event loop
    return 0;
}

int new_arp_table(int id) {
    arp_t[id] = malloc(sizeof(struct arp_table));
    arp_t[id]->device = id;
    get_MAC(id, &arp_t[id]->local_mac_addr);
    get_IP(id, &arp_t[id]->local_ip_addr);
    arp_t[id]->table = NULL;
    return 0;
}

int ARP_advertise(int id) {
    int ret;
    struct arp_data *data = (struct arp_data*)malloc(sizeof(struct arp_data));

    data->sender_hw_addr = arp_t[id]->local_mac_addr;
    data->sender_ip_addr = arp_t[id]->local_ip_addr;

    if (ARP_DEBUG) {
        fprintf(stderr, "Advertised my mac address\n");
        print_mac(stderr, arp_t[id]->local_mac_addr.data);
        printf("\n");
    }

    for (int i=0;i!=total_dev;++i) {
        ret = broadcastFrame(data, sizeof(struct arp_data), ETH_P_ARP, i);
        RCPE(ret == -1, -1, "Error broadcasting ARP frame");
    }

    free(data);

    return 0;
}

struct arp_record *arp_query(int id, uint32_t addr) {
    struct arp_record *rec;
    HASH_FIND_INT(arp_t[id]->table, &addr, rec);
    return rec;
}