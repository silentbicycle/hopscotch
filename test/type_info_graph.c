#include "type_info_graph.h"

#include <assert.h>
#include <string.h>

static enum theft_alloc_res
graph_alloc(struct theft *t, void *env, void **output) {

    struct test_env *test_env = theft_hook_get_env(t);
    uint8_t limit_bits = 0;
    while ((1LLU << limit_bits) < test_env->limit) { limit_bits++; }
    assert((1LLU << limit_bits) == test_env->limit); /* power of 2 */
    const size_t count_ceil = theft_random_bits(t, limit_bits);

    const size_t alloc_size = sizeof(struct graph) + count_ceil * sizeof(struct edge);
    struct graph *res = malloc(alloc_size);
    if (res == NULL) { return THEFT_ALLOC_ERROR; }
    memset(res, 0x00, alloc_size);

    struct edge *edges = res->edges;

    size_t count = 0;
    for (size_t e_i = 0; e_i < count_ceil; e_i++) {
        struct edge *e = &edges[count];
        uint32_t from = theft_random_bits(t, limit_bits);
        uint32_t to_set[MAX_TO] = { 0 };

        assert(MAX_TO == 4);    /* 1<<2 == 4 */
        const uint8_t to_ceil = theft_random_bits(t, 2);

        uint8_t to_count = 0;
        for (size_t t_i = 0; t_i < to_ceil; t_i++) {
            uint32_t to = theft_random_bits(t, limit_bits);
            if (to_count == 0 || theft_random_bits(t, 1)) {
                to_set[to_count++] = to;
            }
        }

        /* make shrinking away easy */
        if (theft_random_bits(t, 3) > 0) {
            *e = (struct edge) {
                .from = from,
                .to_count = to_count,
            };
            memcpy(e->to, to_set, sizeof(to_set));
            count++;
        }
    }

    res->edge_count = count;
    *output = res;
    return THEFT_ALLOC_OK;
}

static void
graph_print(FILE *f, const void *instance, void *env) {
    const struct graph *g = instance;
    assert(g);
    for (size_t e_i = 0; e_i < g->edge_count; e_i++) {
        const struct edge *e = &g->edges[e_i];
        fprintf(f, "%u: ", e->from);
        for (size_t t_i = 0; t_i < e->to_count; t_i++) {
            fprintf(f, "%u ", e->to[t_i]);
        }
        fprintf(f, "\n");
    }
}

const struct theft_type_info graph_type_info = {
    .alloc = graph_alloc,
    .free = theft_generic_free_cb,
    .print = graph_print,

    .autoshrink_config = {
        .enable = true,
    },
};
