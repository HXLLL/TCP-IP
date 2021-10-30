#include "link_state.h"
#include "utils.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

int linkstate_init(struct LinkState *ls) {
    ls->size = 0;
    ls->capacity = 0;

    ls->recs_id = NULL;
    ls->recs_gid = NULL;

    pthread_mutex_init(&ls->ls_mutex, NULL);

    return 0;
}

int linkstate_SPFA(struct LinkState *ls) {
    int *q = malloc(sizeof(int) * ls->size);
    int *in_queue = malloc(sizeof(int) * ls->size);
    int *d = ls->dis, *nh = ls->next_hop;
    int head = 0, tail = 0, n = ls->size, cnt = 1;
    q[head] = 0;
    memset(d, 0x7f, sizeof(int) * n);
    memset(in_queue, 0, sizeof(int)*n);
    memset(ls->next_hop, -1, sizeof(int) * n);
    in_queue[0] = 1;
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
struct linkstate_record *new_linkstate_record(uint32_t ip_count, uint32_t link_count) {
    struct linkstate_record *rec = malloc(sizeof(struct linkstate_record));

    rec->ip_count = ip_count;
    rec->ip_list = malloc(ip_count * sizeof(uint32_t));
    rec->ip_mask_list = malloc(ip_count * sizeof(uint32_t));

    rec->link_count = link_count;
    rec->link_list = malloc(link_count * sizeof(uint32_t));
    rec->dis_list = malloc(link_count * sizeof(uint32_t));

    return rec;
}

int linkstate_add_rec(struct LinkState *ls, struct linkstate_record *rec) {
    pthread_mutex_lock(&ls->ls_mutex);

    struct linkstate_record *r;
    HASH_FIND(hh_gid, ls->recs_gid, &rec->gid, sizeof(uint32_t), r);

    if (!r) {
        rec->id = ls->size++;
        fprintf(stderr, "[LinkState] new host detected, gid: %d\n", rec->gid);
    } else {
        rec->id = r->id;
        HASH_DELETE(hh_gid, ls->recs_gid, r);
        HASH_DELETE(hh_id, ls->recs_id, r);
        linkstate_free_rec(r);
    }

    HASH_ADD(hh_gid, ls->recs_gid, gid, sizeof(uint32_t), rec);
    HASH_ADD(hh_id, ls->recs_id, id, sizeof(uint32_t), rec);

    pthread_mutex_unlock(&ls->ls_mutex);

    return 0;
}

int linkstate_update(struct LinkState *ls) {
    pthread_mutex_lock(&ls->ls_mutex);

    uint64_t cur_time = gettime_ms();

    // free old arrays
    if (ls->capacity) {
        free(ls->dis);
        free(ls->next_hop);
        for (int i = 0; i != ls->capacity; ++i)
            free(ls->c[i]);
        free(ls->c);
    }

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
    HASH_ITER(hh_gid, ls->recs_gid, rec, tmp) {
        // if (cur_time - rec->timestamp > LINKSTATE_EXPIRE) continue; // TODO: delete expired record
        int s = rec->id;
        for (int i=0;i!=rec->link_count;++i) {
            int t_gid = rec->link_list[i];
            struct linkstate_record *rec2;
            HASH_FIND(hh_gid, ls->recs_gid, &t_gid, sizeof(uint32_t), rec2);
            if (!rec2) continue;
            int t = rec2->id;
            ls->c[s][t] = ls->c[t][s] = rec->dis_list[i];
        }
    }

    // update dis and **next_hop**
    linkstate_SPFA(ls);

    pthread_mutex_unlock(&ls->ls_mutex);

    return 0;
}

int linkstate_next_hop(struct LinkState *ls, uint32_t gid) { 
    struct linkstate_record *rec;
    HASH_FIND(hh_gid, ls->recs_gid, &gid, sizeof(uint32_t), rec);
    if (rec) {
        uint32_t nxt_id = ls->next_hop[rec->id];
        if (nxt_id == -1) return -1;
        HASH_FIND(hh_id, ls->recs_id, &nxt_id, sizeof(uint32_t), rec);
        assert(rec != NULL);
        return rec->gid;
    } else

        return -1;
}
