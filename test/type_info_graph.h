#ifndef TYPE_INFO_GRAPH_H
#define TYPE_INFO_GRAPH_H

#include "theft.h"

struct cycle {
    uint32_t nodes[2];
    bool found;
};

struct test_env {
    size_t limit;
    size_t limit64;

    uint64_t *seen;
    uint64_t *used;
    const char *error;
    bool has_cycle;

    /* note: this will wrap, that's ok */
    uint8_t cycle_count;
    struct cycle cycles[256];
};

#define MAX_TO 4

struct edge {
    uint32_t from;
    uint8_t to_count;
    uint32_t to[MAX_TO];
};

struct graph {
    size_t edge_count;
    struct edge edges[];
};

extern const struct theft_type_info graph_type_info;

#endif
