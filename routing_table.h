#ifndef ROUTING_TABLE_H
#define ROUTING_TABLE_H

#include <netinet/in.h>

struct Record {
    struct in_addr dest;
    struct in_addr mask;
    uint8_t nexthop_mac[6];
    uint8_t device;
};

struct RT {
    int cnt;
    int capacity;
    struct Record *table;
};

/****
 * @brief initialize routing table
 * @return 0 when success, -1 when error
 **/
int rt_init(struct RT *rt);

/****
 * @brief match against routing table
 * @return 1 when found, 0 when not found
 **/
int rt_query(struct RT *rt, struct in_addr addr, struct Record *res);

/****
 * @return 1 when insert, 0 when update
 **/
int rt_update(struct RT *rt, struct Record *rec);

#endif