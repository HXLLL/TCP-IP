#include "link_state.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

int linkstate_init(struct LinkState *ls, int size) {
    ls->size = size;

    ls->dis = (int *)malloc(sizeof(int) * size);
    memset(ls->dis, -1, sizeof(int) * size);
    ls->dis[0] = 0;

    ls->next_hop = (int *)malloc(sizeof(int) * size);
    memset(ls->next_hop, -1, sizeof(ls->next_hop));
    ls->next_hop[0] = 0;

    ls->c = (int **)malloc(sizeof(int *) * size);
    for (int i = 0; i != size; ++i) {
        ls->c[i] = (int *)malloc(sizeof(int) * size);
        memset(ls->c[i], -1, sizeof(int) * size);
    }
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
}

int linkstate_update(struct LinkState *ls, int u, int v, int c) {
    ls->c[u][v] = c;
    ls->c[v][u] = c;
    linkstate_SPFA(ls);
}

int linkstate_next_hop(struct LinkState *ls, int t) { return ls->next_hop[t]; }
