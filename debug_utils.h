#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include "link_state.h"
#include "routing_table.h"
#include "uthash/uthash.h"
#include "utils.h"

static const int LINKSTATE_DEBUG_RECORD = 1;
static const int LINKSTATE_DEBUG_IP_HOST = 1;
static const int LINKSTATE_DEBUG_NEIGH = 1;

static char buffer[256];
static char *mac_str(void *mac) {
    sprintf(buffer, "%02x:%02x:%02x:%02x:%02x:%02x", GET1B(mac, 0),
            GET1B(mac, 1), GET1B(mac, 2), GET1B(mac, 3), GET1B(mac, 4),
            GET1B(mac, 5));
    return buffer;
}

static char ip_buffer[256];
static char *ip_str(void *ip) {
    sprintf(ip_buffer, "%u.%u.%u.%u", GET1B(ip, 0), GET1B(ip, 1), GET1B(ip, 2),
            GET1B(ip, 3));
    return ip_buffer;
}
static char mask_buffer[256];
static char *mask_str(void *mask) {
    sprintf(mask_buffer, "%u.%u.%u.%u", GET1B(mask, 0), GET1B(mask, 1),
            GET1B(mask, 2), GET1B(mask, 3));
    return mask_buffer;
}

static void linkstate_dump(struct LinkState *ls) {
    uint64_t cur_time = gettime_ms();
    INDENT_DEBUG("curtime: %lu\n", cur_time);
    INDENT_DEBUG("gid: %d, size: %d, capacity: %d\n", ls->router_gid, ls->size,
                 ls->capacity);
    INDENT_INC();
    if (LINKSTATE_DEBUG_RECORD) {
        INDENT_DEBUG("Records: %d in total\n", HASH_CNT(hh_id, ls->recs_id));
        INDENT_INC();
        struct linkstate_record *rec, *tmp;
        struct linkstate_record *record_buffer[255];
        memset(record_buffer, 0, sizeof(record_buffer));

        HASH_ITER(hh_id, ls->recs_id, rec, tmp) {
            record_buffer[rec->id] = rec;
        }

        for (int i = 0; i != 255; ++i) {
            if (!record_buffer[i]) continue;
            rec = record_buffer[i];

            INDENT_DEBUG("Record lid=%u, gid=%u, timestamp: %ld\n", rec->id,
                         rec->gid, rec->timestamp);
            INDENT_INC();

            INDENT_DEBUG("%d Attached IP\n", rec->ip_count);
            INDENT_INC();
            for (int i = 0; i != rec->ip_count; ++i) {
                INDENT_DEBUG(
                    "%u.%u.%u.%u, mask: %u.%u.%u.%u\n",
                    GET1B(&rec->ip_list[i], 0), GET1B(&rec->ip_list[i], 1),
                    GET1B(&rec->ip_list[i], 2), GET1B(&rec->ip_list[i], 3),
                    GET1B(&rec->ip_mask_list[i], 0),
                    GET1B(&rec->ip_mask_list[i], 1),
                    GET1B(&rec->ip_mask_list[i], 2),
                    GET1B(&rec->ip_mask_list[i], 3));
            }
            INDENT_DEC();

            INDENT_DEBUG("%d Link State\n", rec->link_count);
            INDENT_INC();
            for (int i = 0; i != rec->link_count; ++i) {
                INDENT_DEBUG("---> %u, dis: %u\n", rec->link_list[i],
                             rec->dis_list[i]);
            }
            INDENT_DEC();

            INDENT_DEC();
        }
        INDENT_DEC();
    }

    if (LINKSTATE_DEBUG_IP_HOST) {
        struct ip_host_record *rec, *tmp;
        int ip_count = HASH_COUNT(ls->ip_rec);
        INDENT_DEBUG("Total IP: %d\n", ip_count);
        INDENT_INC();
        HASH_ITER(hh, ls->ip_rec, rec, tmp) {
            INDENT_DEBUG("%u.%u.%u.%u, GID: %u\n", GET1B(&rec->ip, 0),
                         GET1B(&rec->ip, 1), GET1B(&rec->ip, 2),
                         GET1B(&rec->ip, 3), rec->gid);
        }
        INDENT_DEC();
    }

    if (LINKSTATE_DEBUG_NEIGH) {
        int total_neigh = 0;
        for (int i = 0; i != ls->neigh_net_cnt; ++i)
            total_neigh += ls->neigh_size[i];
        INDENT_DEBUG("%d neigh networks, %d neighbor in total\n",
                     ls->neigh_net_cnt, total_neigh);
        INDENT_INC();
        for (int i = 0; i != ls->neigh_net_cnt; ++i) {
            INDENT_DEBUG("Port %d, neigh_cnt=%d\n", i, ls->neigh_size[i]);
            INDENT_INC();
            for (int j = 0; j != ls->neigh_size[i]; ++j) {
                INDENT_DEBUG("IP: %u.%u.%u.%u, gid: %u, mac: "
                             "%02x:%02x:%02x:%02x:%02x:%02x\n",
                             GET1B(&ls->neigh[i][j].ip, 0),
                             GET1B(&ls->neigh[i][j].ip, 1),
                             GET1B(&ls->neigh[i][j].ip, 2),
                             GET1B(&ls->neigh[i][j].ip, 3), ls->neigh[i][j].gid,
                             GET1B(&ls->neigh[i][j].mac.data, 0),
                             GET1B(&ls->neigh[i][j].mac.data, 1),
                             GET1B(&ls->neigh[i][j].mac.data, 2),
                             GET1B(&ls->neigh[i][j].mac.data, 3),
                             GET1B(&ls->neigh[i][j].mac.data, 4),
                             GET1B(&ls->neigh[i][j].mac.data, 5));
            }
            INDENT_DEC();
        }
        INDENT_DEC();
    }
    INDENT_DEC();
}

static void arp_dump(struct arp_table *arp_t) {
    uint64_t cur_time = gettime_ms();
    char buf[255];

    INDENT_DEBUG("ARP table for device %d, mac: %s, ip: %s\n", arp_t->device,
                 mac_str(arp_t->local_mac_addr.data),
                 ip_str(&arp_t->local_ip_addr));
    INDENT_INC();

    struct arp_record *rec, *tmp;
    HASH_ITER(hh, arp_t->table, rec, tmp) {
        sprintf(buf, "ip: %s, mac: %s", ip_str(&rec->ip_addr),
                mac_str(rec->mac_addr.data));
        if (cur_time - rec->timestamp > ARP_EXPIRE) {
            INDENT_DEBUG("%s [EXPIRED]\n", buf);
        } else {
            INDENT_DEBUG("%s\n", buf);
        }
    }

    INDENT_DEC();
}

static void rt_dump(struct RT *rt) {
    uint64_t cur_time = gettime_ms();

    printf("Routing Table, size: %d/%d\n", rt->cnt, rt->capacity);
    INDENT_INC();
    for (int i = 0; i != rt->cnt; ++i) {
        char buf[255];
        sprintf(buf, "port: %d, dst: %s, mask: %s, mac: %s",
                rt->table[i].device, ip_str(&rt->table[i].dest.s_addr),
                mask_str(&rt->table[i].mask.s_addr),
                mac_str(rt->table[i].nexthop_mac));
        if (cur_time - rt->table[i].timestamp > RT_EXPIRE)
            INDENT_DEBUG("%s [EXPIRED] \n", buf);
        else
            INDENT_DEBUG("%s\n", buf);
    }
    INDENT_DEC();
}

#endif