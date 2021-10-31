#ifndef LINK_STATE_H
#define LINK_STATE_H

#include "arp.h"

#include "uthash/uthash.h"
#include <stdlib.h>
#include <string.h>

#define LINKSTATE_EXPIRE 4000

struct linkstate_record {
    // public
    uint32_t gid;

    int ip_count;
    uint32_t *ip_list;
    uint32_t *ip_mask_list;

    int link_count;
    uint32_t *link_list;
    uint32_t *dis_list;

    // private
    uint32_t id;
    uint64_t timestamp;
    UT_hash_handle hh_gid;
    UT_hash_handle hh_id;
};

struct ip_host_record {
    uint32_t gid;
    uint32_t ip;
    UT_hash_handle hh;
};

// neighbor info
struct neigh_record {
    uint32_t ip;
    uint32_t gid;
    int port;
    int dis;
    struct MAC_addr mac;
};

struct LinkState {
    // private
    pthread_mutex_t ls_mutex;

    int size, capacity;

    uint32_t router_gid;

    // neighbor information
    int neigh_net_cnt;
    struct arp_table **arp_t;
    struct neigh_record neigh[16][16];
    int neigh_size[16];

    // ip host table
    struct ip_host_record *ip_rec;

    // advertisement table
    struct linkstate_record *recs_gid;
    struct linkstate_record *recs_id;

    // neighbor table
    // TODO: optimize

    int *next_hop;
};

/**
 * @brief initiate Linkstate Structure
 *
 * @param ls preallocated linkstate struct
 * @return int
 * 0 on sccuess
 */
int linkstate_init(struct LinkState *ls, int neigh_net_cnt,
                   struct arp_table **arp_t);

/**
 * @brief
 *  expire old records, update with new records,
 *  call it periodically in main event loop.
 *  thread safe
 *
 * @param ls linkstate structure to be updated
 * @return int 0 on success
 */
int linkstate_update(struct LinkState *ls, int total_dev,
                     struct arp_table **arp_t);

struct neigh_record *linkstate_next_hop(struct LinkState *ls, uint32_t dst_ip);

int linkstate_process_advertise(struct LinkState *ls, void *data);

int linkstate_make_advertisement(struct LinkState *ls, void **res_buf);

#endif
