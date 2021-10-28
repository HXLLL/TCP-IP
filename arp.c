#include "uthash/uthash.h"
#include "arp.h"
#include "device.h"
#include "packetio.h"
#include "utils.h"

#include <stdlib.h>

struct arp_record *arp_table;
uint32_t my_ip;
struct MAC_addr my_mac;

struct arp_record* new_arp_record(uint32_t ip_addr, struct MAC_addr mac_addr, int port) {
    struct arp_record *rec = malloc(sizeof(struct arp_record));
    rec->ip_addr = ip_addr;
    memcpy(&rec->mac_addr, &mac_addr.data, ETH_HLEN);
    rec->port = port;
    return rec;
}

int arp_frame_handler(const void* buf, int len, int id) {
    const struct ethhdr *hdr = buf;
    const struct arp_data *data = eth_raw_content_const(buf);

    printf("Received ARP frame\n");
    printf("Addr: "); print_ip(stdout, data->sender_ip_addr); printf("\n");
    printf("MAC: "); print_mac(stdout, data->sender_hw_addr.data); printf("\n");
    
    struct arp_record *res;
    HASH_FIND_INT(arp_table, &data->sender_ip_addr, res);
    if (res) {
        res->mac_addr = data->sender_hw_addr;
        res->port = id;
    } else {
        struct arp_record *rec = new_arp_record(data->sender_ip_addr, data->sender_hw_addr, id);
        HASH_ADD_INT(arp_table, ip_addr, rec);
    }

    return 0;
}

int arp_init(uint32_t ip_addr, struct  MAC_addr mac_addr) {
    setFrameReceiveCallback(arp_frame_handler, ETH_P_ARP);

    arp_table = NULL;

    my_ip = ip_addr;
    my_mac = mac_addr;
}

int ARP_advertise() {
    int ret;
    struct arp_data *data = (struct arp_data*)malloc(sizeof(struct arp_data));

    data->sender_hw_addr = my_mac;
    data->sender_ip_addr = my_ip;

    fprintf(stderr, "Advertised my mac address\n");

    for (int i=0;i!=total_dev;++i) {
        ret = broadcastFrame(data, sizeof(struct arp_data), ETH_P_ARP, i);
        RCPE(ret == -1, -1, "Error broadcasting ARP frame");
    }

    free(data);

    return 0;
}

struct arp_record *arp_query(uint32_t addr) {
    struct arp_record *rec;
    HASH_FIND_INT(arp_table, &addr, rec);
    return rec;
}