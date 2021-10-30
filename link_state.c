#include "link_state.h"
#include "arp.h"
#include "utils.h"
#include "debug_utils.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

const int LINKSTATE_DEBUG = 1;
const int LINKSTATE_DUMP = 1;

int linkstate_SPFA(struct LinkState *ls, int *dis, int **c) {
    int *q = malloc(sizeof(int) * ls->size);
    int *in_queue = malloc(sizeof(int) * ls->size);
    int *d = dis, *nh = ls->next_hop;
    int head = 0, tail = 0, n = ls->size, cnt = 1;
    q[head] = 0;
    memset(d, 0x7f, sizeof(int) * n);
    memset(in_queue, 0, sizeof(int) * n);
    memset(ls->next_hop, -1, sizeof(int) * n);
    in_queue[0] = 1;
    nh[0] = 0;
    d[0] = 0;
    while (cnt != 0) {
        int u = q[tail];
        --cnt;
        ADD_MOD(tail, n);
        for (int i = 0; i != n; ++i) {
            if (c[u][i] != -1 && d[i] > d[u] + c[u][i]) {
                if (u == 0)
                    nh[i] = i;
                else
                    nh[i] = nh[u];
                d[i] = d[u] + c[u][i];
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

uint32_t query_gid_by_ip(struct LinkState *ls, uint32_t ip) {
    struct ip_host_record *rec;
    HASH_FIND_INT(ls->ip_rec, &ip, rec);
    if (rec)
        return rec->gid;
    else
        return -1;
}

struct linkstate_record *new_linkstate_record(uint32_t ip_count,
                                              uint32_t link_count) {
    struct linkstate_record *rec = malloc(sizeof(struct linkstate_record));

    rec->ip_count = ip_count;
    rec->ip_list = malloc(ip_count * sizeof(uint32_t));
    rec->ip_mask_list = malloc(ip_count * sizeof(uint32_t));

    rec->link_count = link_count;
    rec->link_list = malloc(link_count * sizeof(uint32_t));
    rec->dis_list = malloc(link_count * sizeof(uint32_t));

    return rec;
}

void linkstate_free_rec(struct linkstate_record *rec) {
    free(rec->ip_list);
    free(rec->ip_mask_list);
    free(rec);
}

int linkstate_add_rec(struct LinkState *ls, struct linkstate_record *rec) {
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

    return 0;
}

/*****
 * @brief update neigh info from arp table, then
 * update native linkstate record based on neigh info
 */
void update_neigh_info(struct LinkState *ls) {
    uint64_t cur_time = gettime_ms();
    uint32_t neigh_count = 0;

    for (int i = 0; i != ls->neigh_net_cnt; ++i) {
        struct arp_record *rec, *tmp;
        int j = 0;
        HASH_ITER(hh, arp_t[i]->table, rec, tmp) {

            // if (cur_time - rec->timestamp > ARP_EXPIRE) continue;

            uint32_t t_gid = query_gid_by_ip(ls, rec->ip_addr);
            if (t_gid == -1) continue;

            ls->neigh[i][j].ip = rec->ip_addr;
            ls->neigh[i][j].port = i;
            ls->neigh[i][j].gid = t_gid;
            ls->neigh[i][j].dis = 1;
            ls->neigh[i][j].mac = rec->mac_addr;
            ++j;
        }
        ls->neigh_size[i] = j;
        neigh_count += j;
    }

    struct linkstate_record *rec =
        new_linkstate_record(ls->neigh_net_cnt, neigh_count);
    rec->gid = ls->router_gid;
    rec->timestamp = -1;

    for (int i = 0; i != ls->neigh_net_cnt; ++i) {
        rec->ip_list[i] = dev_IP[i];
        rec->ip_mask_list[i] = dev_IP_mask[i];
    }

    for (int i = 0, k = 0; i != ls->neigh_net_cnt; ++i) {
        for (int j = 0; j != ls->neigh_size[i]; ++j) {
            rec->link_list[k] = ls->neigh[i][j].gid;
            rec->dis_list[k++] = ls->neigh[i][j].dis;
        }
    }

    linkstate_add_rec(ls, rec);
}

int linkstate_init(struct LinkState *ls, int neigh_net_cnt,
                   struct arp_table **arp_t) {
    pthread_mutex_init(&ls->ls_mutex, NULL);

    ls->size = 0;
    ls->capacity = 0;

    // TODO: detect collision
    ls->router_gid = random_ex();

    ls->neigh_net_cnt = neigh_net_cnt;
    ls->arp_t = arp_t;

    ls->ip_rec = NULL;

    ls->recs_id = NULL;
    ls->recs_gid = NULL;

    if (LINKSTATE_DEBUG) {
        fprintf(stderr, "LinkState Initialized, with GID=%u\n", ls->router_gid);
    }

    return 0;
}


int linkstate_update(struct LinkState *ls, int neigh_net_cnt,
                     struct arp_table **arp_t) {
    pthread_mutex_lock(&ls->ls_mutex);

    uint64_t cur_time = gettime_ms();
    struct linkstate_record *rec, *tmp;
    
    // delete expired record, and their effect
    HASH_ITER(hh_gid, ls->recs_gid, rec, tmp) {
        if (rec->timestamp == -1) continue;
        if (cur_time - rec->timestamp > LINKSTATE_EXPIRE) {
            HASH_DELETE(hh_gid, ls->recs_gid, rec);
            HASH_DELETE(hh_id, ls->recs_id, rec);
            for (int i=0;i!=rec->ip_count;++i) {
                struct ip_host_record *ip_rec;
                HASH_FIND_INT(ls->ip_rec, &rec->ip_list[i], ip_rec);
                HASH_DEL(ls->ip_rec, ip_rec);
                free(ip_rec);
            }
            linkstate_free_rec(rec);
        }
    }
    
    ls->neigh_net_cnt = neigh_net_cnt;
    ls->arp_t = arp_t;

    // update neighbor info, delete expired host
    update_neigh_info(ls);

    if (ls->capacity) {
        free(ls->next_hop);
    }

    int n = ls->size;
    ls->capacity = n;

    // realloc auxiliary arrays
    ls->next_hop = malloc(n * sizeof(int));

    int *dis = malloc(n * sizeof(int));
    int **c = malloc(n * sizeof(int *));
    for (int i = 0; i != n; ++i) {
        c[i] = malloc(n * sizeof(int));
        for (int j = 0; j != n; ++j)
            c[i][j] = -1;
    }

    // update c
    HASH_ITER(hh_gid, ls->recs_gid, rec, tmp) {
        int s = rec->id;
        for (int i = 0; i != rec->link_count; ++i) {
            int t_gid = rec->link_list[i];
            struct linkstate_record *rec2;
            HASH_FIND(hh_gid, ls->recs_gid, &t_gid, sizeof(uint32_t), rec2);
            if (!rec2) continue;
            int t = rec2->id;
            c[s][t] = c[t][s] = rec->dis_list[i];
        }
    }

    // update dis and **next_hop**
    linkstate_SPFA(ls, dis, c);

    linkstate_dump(ls);

    pthread_mutex_unlock(&ls->ls_mutex);

    return 0;
}

int linkstate_process_advertise(struct LinkState *ls, void *data) {
    uint64_t cur_time = gettime_ms();

    pthread_mutex_lock(&ls->ls_mutex);

    uint32_t s_gid = GET4B(data, 8);
    int j = 16;

    int neigh_count = GET2B(data, 4);
    int ip_count = GET2B(data, 6);

    struct linkstate_record *rec = new_linkstate_record(ip_count, neigh_count);
    rec->gid = s_gid;
    rec->timestamp = cur_time;

    // acquire ip info
    for (int i = 0; i != ip_count; ++i, j += 8) {
        uint32_t s_ip = GET4B(data, j);
        if (query_gid_by_ip(ls, s_ip) == -1) {
            struct ip_host_record *ip_rec =
                malloc(sizeof(struct ip_host_record));
            ip_rec->gid = s_gid;
            ip_rec->ip = s_ip;
            HASH_ADD_INT(ls->ip_rec, ip, ip_rec);
        }
        rec->ip_list[i] = s_ip;
        rec->ip_mask_list[i] = GET4B(data, j + 4);
    }

    // acquire link info
    for (int i = 0; i != neigh_count; ++i, j += 8) {
        uint32_t t_gid = GET4B(data, j);
        uint32_t dis = GET4B(data, j + 4);
        rec->link_list[i] = t_gid;
        rec->dis_list[i] = dis;
    }

    linkstate_add_rec(ls, rec);

    pthread_mutex_unlock(&ls->ls_mutex);

    return 0;
}
/****
 *   -0------------16----------32----------48----------64
 *   -0------------2-----------4-----------6-----------8
 *   0| PROTO_LINKSTATE        |neigh_count| ip_count  |
 *   8| router_gid             | reserved              |
 *  16| ip[1]                  | ip_mask[1]            |
 *    | .....                                          |
 *    | ip[ip_count]           | ip_mask[ip_count]     |
 *    | neigh[1].addr          | neigh[cnt].dis        |
 *    | .....                                          |
 *    | neigh[neigh_count].addr| ~~~.dis               |
 *    |
 * */
int linkstate_make_advertisement(struct LinkState *ls, void **res_buf) {
    update_neigh_info(ls); // TODO: is this really necessary?

    int ip_count = ls->neigh_net_cnt;
    int neigh_count = 0;
    for (int i = 0; i != ls->neigh_net_cnt; ++i) {
        neigh_count += ls->neigh_size[i];
    }

    int len = (neigh_count + ip_count) * sizeof(int) * 2 + 16;

    void *buf = malloc(len);

    PUT4B(buf, 0, PROTO_LINKSTATE);
    PUT2B(buf, 4, neigh_count);
    PUT2B(buf, 6, ip_count);

    PUT4B(buf, 8, ls->router_gid);
    PUT4B(buf, 12, 0);

    int j = 16;

    for (int i = 0; i != ip_count; ++i) {
        PUT4B(buf, j, ls->arp_t[i]->local_ip_addr);
        PUT4B(buf, j + 4, ls->arp_t[i]->local_ip_mask);
        j += 8;
    }

    for (int i = 0; i != ls->neigh_net_cnt; ++i) {
        for (int k = 0; k != ls->neigh_size[i]; ++k) {
            uint32_t t_gid = ls->neigh[i][k].gid;
            PUT4B(buf, j, t_gid);
            PUT4B(buf, j + 4, 1);
            j += 8;
        }
    }

    *res_buf = buf;

    return j;
}

struct neigh_record *get_neigh_by_gid(struct LinkState *ls, int gid) {
    for (int i = 0; i != ls->neigh_net_cnt; ++i)
        for (int j = 0; j != ls->neigh_size[i]; ++j) {
            if (ls->neigh[i][j].gid == gid) return &ls->neigh[i][j];
        }
    return NULL;
}

// TODO: maybe too complicated, maybe can store id directly in neighbor info
struct neigh_record *linkstate_next_hop(struct LinkState *ls, uint32_t dst_ip) {
    // use dst_ip to get dst gid
    struct ip_host_record *ip_rec;
    HASH_FIND_INT(ls->ip_rec, &dst_ip, ip_rec);
    if (!ip_rec) return NULL;

    // use dst gid to get dst id
    struct linkstate_record *rec;
    HASH_FIND(hh_gid, ls->recs_gid, &ip_rec->gid, sizeof(uint32_t), rec);
    if (!rec) return NULL;

    // use dst id to get next_hop id
    uint32_t nxt_id = ls->next_hop[rec->id];
    if (nxt_id == -1) return NULL;

    // use next_hop id to get next_hop gid
    HASH_FIND(hh_id, ls->recs_id, &nxt_id, sizeof(uint32_t), rec);
    assert(rec != NULL);

    // use next_hop gid to get next_hop neigh record
    struct neigh_record *result = get_neigh_by_gid(ls, rec->gid);

    return result;
}