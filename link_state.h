#ifndef LINK_STATE_H
#define LINK_STATE_H

#include <stdlib.h>
#include <string.h>

struct LinkState {
    int size;
    int *dis, **c;
    int *next_hop;
};

int linkstate_init(struct LinkState *ls, int size);
int linkstate_update(struct LinkState *ls, int u,int v,int c);
int linkstate_next_hop(struct LinkState *ls, int t);

#endif
