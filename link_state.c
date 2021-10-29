#include "link_state.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

int linkstate_init(struct LinkState *ls, int size) {
    ls->size = 0;
    ls->capacity = 0;

    ls->recs = NULL;

    return 0;
}

int linkstate_SPFA(struct LinkState *ls) {
    int *q = malloc(sizeof(int) * ls->size);
    int *in_queue = malloc(sizeof(int) * ls->size);
    int *d = ls->dis, *nh = ls->next_hop;
    int head = 0, tail = 0, n = ls->size, cnt = 1;
    q[head] = 0;
    in_queue[0] = 1;
    memset(d, 0x7f, sizeof(int) * n);
    memset(ls->next_hop, -1, sizeof(int) * n);
    nh[0] = 0;
    d[0] = 0;
    while (cnt != 0) {
        int u = q[tail];
        --cnt;
        ADD_MOD(tail, n);
        for (int i = 0; i != n; ++i) {
            if (ls->c[u][i] != -1 && d[i] > d[u] + ls->c[u][i]) {
                if (u == 0)
                    nh[i] = i;
                else
                    nh[i] = nh[u];
                d[i] = d[u] + ls->c[u][i];
                if (!in_queue[i]) {
                    ADD_MOD(head, n);
                    q[head] = i;
                    in_queue[i] = 1;
                    ++cnt;
                }
            }
        }
    }
    free(q);
    free(in_queue);
    return 0;
}

void linkstate_free_rec(struct linkstate_record *rec) {
    free(rec->ip_list);
    free(rec->ip_mask_list);
    free(rec);
}

int linkstate_add_rec(struct LinkState *ls, int gid,
                      struct linkstate_record *rec) {
    struct linkstate_record *r;
    HASH_FIND(hh_gid, ls->recs, &gid, sizeof(uint32_t), r);

    if (!r) {
        rec->id = ls->size++;
    } else {
        rec->id = r->id;
        linkstate_free_rec(r);
    }

    HASH_ADD(hh_gid, ls->recs, gid, sizeof(uint32_t), rec);
    HASH_ADD(hh_id, ls->recs, id, sizeof(uint32_t), rec);

    return 0;
}

int linkstate_update(struct LinkState *ls) {
    uint64_t cur_time = gettime_ms();

    // free old arrays
    free(ls->dis);
    free(ls->next_hop);
    for (int i = 0; i != ls->capacity; ++i)
        free(ls->c[i]);
    free(ls->c);

    int n = ls->size;
    ls->capacity = n;

    // realloc auxiliary arrays
    ls->dis = malloc(n * sizeof(int));
    ls->next_hop = malloc(n * sizeof(int));
    ls->c = malloc(n * sizeof(int *));
    for (int i = 0; i != n; ++i) {
        ls->c[i] = malloc(n * sizeof(int));
        for (int  j=0;j!=n;++j)
            ls->c[i][j] = -1;
    }

    // update c
    struct linkstate_record *rec, *tmp;
    HASH_ITER(hh_id, ls->recs, rec, tmp) {
        if (cur_time - rec->timestamp > LINKSTATE_EXPIRE) continue; // TODO: delete expired record
        int s = rec->id;
        for (int i=0;i!=rec->link_count;++i) {
            int t = rec->link_list[i];
            ls->c[s][t] = ls->c[t][s] = rec->dis_list[i];
        }
    }

    // update dis and **next_hop**
    linkstate_SPFA(ls);

    return 0;
}

int linkstate_next_hop(struct LinkState *ls, int t) { return ls->next_hop[t]; }
