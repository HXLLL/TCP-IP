#ifndef LINK_STATE_H
#define LINK_STATE_H

#include "uthash/uthash.h"
#include <stdlib.h>
#include <string.h>

#define LINKSTATE_EXPIRE 4000

struct linkstate_record {
// user set
    uint32_t gid;

    int ip_count;
    uint32_t *ip_list;
    uint32_t *ip_mask_list;

    int link_count;
    uint32_t *link_list;
    uint32_t *dis_list;

// automatically maintained
    uint32_t id;
    uint64_t timestamp;
    UT_hash_handle hh_gid;
    UT_hash_handle hh_id;
};

struct LinkState {
// private
    int size, capacity;
    int *dis, **c;

// public
    int *next_hop;
    struct linkstate_record *recs;
};

int linkstate_init(struct LinkState *ls, int size);
int linkstate_add_rec(struct LinkState *ls, int gid, struct linkstate_record *rec);
int linkstate_update(struct LinkState *ls);
int linkstate_next_hop(struct LinkState *ls, int t);
int linkstate_SPFA(struct LinkState *ls);

#endif
