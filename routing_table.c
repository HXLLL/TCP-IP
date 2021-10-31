#include "routing_table.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

int rt_init(struct RT *rt) {
    rt->cnt = 0;
    rt->capacity = 1;
    rt->table = malloc(sizeof(struct Record));
    memset(rt->table, 0, sizeof(struct Record));

    return 0;
}

int rt_expand(struct RT *rt) {
    rt->capacity *= 2;                  // depends on specific expand algo

    struct Record *new_table = malloc(rt->capacity * sizeof(struct Record));
    memcpy(new_table, rt->table, rt->cnt * sizeof(struct Record));
    free(rt->table);
    rt->table = new_table;

    return 0;
}

int rt_match(struct Record *rec, struct in_addr addr) {
    return (rec->dest.s_addr & rec->mask.s_addr) == (addr.s_addr & rec->mask.s_addr);
}

/****
 * @brief match against routing table
 * @return 1 when found, 0 when not found
 **/
int rt_query(struct RT *rt, struct in_addr addr, struct Record *res) {
    uint64_t cur_time = gettime_ms();
    int p = -1;
    uint32_t res_mask = 0;
    for (int i = 0; i != rt->cnt; ++i) {
        if (rt_match(&rt->table[i], addr) && (
            cur_time - rt->table[i].timestamp <= RT_EXPIRE || rt->table[i].timestamp == -1)) {
            if ((rt->table[i].mask.s_addr & res_mask) == res_mask) {
                res_mask = rt->table[i].mask.s_addr;
                p = i;
            }
        }
    }
    if (p != -1) {
        if (res) memcpy(res, &rt->table[p], sizeof(struct Record));
        return 1;
    }
    return 0;
}

/****
 * @brief exact match with addr and mask
 * @return specified record when found, NULL otherwise
 **/
struct Record *rt_find(struct RT *rt, struct in_addr addr,
                       struct in_addr mask) {
    for (int i = 0; i != rt->cnt; ++i) {
        if (rt->table[i].dest.s_addr == addr.s_addr &&
            rt->table[i].mask.s_addr == mask.s_addr) {
            return &rt->table[i];
        }
    }
    return NULL;
}

/****
 * @return inserted record
 **/
struct Record *rt_insert(struct RT *rt, struct Record *rec) {
    if (rt->cnt + 1 > rt->capacity) {
        rt_expand(rt);
    }
    memcpy(&rt->table[rt->cnt], rec, sizeof(struct Record));
    return &rt->table[rt->cnt++];
}

/****
 * @return 1 when insert, 0 when update
 **/
int rt_update(struct RT *rt, struct Record *rec) {
    struct Record *t;

    if ((t = rt_find(rt, rec->dest, rec->mask))) {
        *t = *rec;
        return 0;
    } else {
        rt_insert(rt, rec);
        return 1;
    }
}