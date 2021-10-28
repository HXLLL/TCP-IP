#ifndef ARP_H
#define ARP_H

#include "uthash/uthash.h"
#include "device.h"

#include <stdlib.h>

struct arp_data {
    struct MAC_addr sender_hw_addr;
    uint32_t sender_ip_addr;
    struct MAC_addr receiver_hw_addr;
    uint32_t receiver_ip_addr;
};
struct arp_record {
    uint32_t ip_addr;
    struct MAC_addr mac_addr;
    int port;
    UT_hash_handle hh;
};

extern struct arp_record *arp_table;

int arp_init(uint32_t ip_addr, struct  MAC_addr mac_addr);
struct arp_record *arp_query(uint32_t addr);
int ARP_advertise();

#endif