#ifndef ARP_H
#define ARP_H

#include "uthash/uthash.h"
#include "device.h"

#include <stdlib.h>

#define ARP_EXPIRE 2000  // in ms

struct arp_data {
    struct MAC_addr sender_hw_addr;
    uint32_t sender_ip_addr;
    struct MAC_addr receiver_hw_addr;
    uint32_t receiver_ip_addr;
};

struct arp_record {
    uint32_t ip_addr;
    struct MAC_addr mac_addr;
    uint64_t timestamp;
    UT_hash_handle hh;
};

struct arp_table {
    int device;
    struct MAC_addr local_mac_addr;
    uint32_t local_ip_addr;
    uint32_t local_ip_mask;
    struct arp_record *table;
};

extern struct arp_table *arp_t[MAX_DEVICES];
int arp_init();
int new_arp_table(int id);
struct arp_record *arp_query(int id, uint32_t addr);
int ARP_advertise(int id);

#endif