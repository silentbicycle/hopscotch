#include "test_hopscotch.h"
#include "type_info_graph.h"

#include <inttypes.h>

static enum theft_trial_res
prop_evaluate_and_check(struct theft *t, void *arg);

static void
solve_cb(uint32_t group_id,
    size_t group_count, const uint32_t *group, void *udata);

static bool
postcondition_checks(struct test_env *env);

static void
add_cycles(struct test_env *env, const struct graph *g);

static bool
both_in_group(const uint32_t nodes[2],
    size_t group_count, const uint32_t *group);

TEST generate_and_check(size_t limit) {
    struct test_env env = { .limit = limit };

    theft_seed seed = theft_seed_of_time();

    env.limit64 = (env.limit / 64) + ((env.limit % 64) ? 1 : 0);
    env.seen = calloc(env.limit64, sizeof(*env.seen));
    if (env.seen == NULL) { assert(false); }
    env.used = calloc(env.limit64, sizeof(*env.used));
    if (env.used == NULL) { assert(false); }

    struct theft_run_config cfg = {
        .name = __func__,
        .prop1 = prop_evaluate_and_check,
        .type_info = { &graph_type_info },
        .hooks = {
            .env = &env,
        },
        .trials = 1000000,
        .seed = seed,
    };

    enum theft_run_res res;
    res = theft_run(&cfg);
    free(env.seen);
    free(env.used);
    ASSERT_EQ_FMT(THEFT_RUN_PASS, res, "%d");

    PASS();
}

static void set_bit(uint64_t *vector, size_t pos) {
    const size_t offset = pos / 64;
    const uint64_t bit = 1LLU << (pos % 64);
    vector[offset] |= bit;
    if (0) {
        fprintf(stdout, "(%s: %p: pos %zu => offset %zu, bit %"PRIx64 " => %"PRIx64")\n",
            __func__, (void *)vector, pos, offset, bit, vector[offset]);
    }
}

static bool get_bit(uint64_t *vector, size_t pos) {
    const size_t offset = pos / 64;
    const uint64_t bit = 1LLU << (pos & 63);
    if (0) {
        fprintf(stdout, "(%s: %p: pos %zu => offset %zu, bit %"PRIx64 " => %"PRIx64")\n",
            __func__, (void *)vector, pos, offset, bit, vector[offset]);
    }
    return (vector[offset] & bit) != 0;
}

static void reset_env(struct test_env *env) {
    env->error = NULL;
    memset(env->seen, 0x00, env->limit64 * sizeof(uint64_t));
    memset(env->used, 0x00, env->limit64 * sizeof(uint64_t));
    env->cycle_count = 0;
    memset(env->cycles, 0x00, sizeof(env->cycles));
}

static enum theft_trial_res
prop_evaluate_and_check(struct theft *t, void *arg) {
    enum theft_trial_res res = THEFT_TRIAL_FAIL;

    struct test_env *env = theft_hook_get_env(t);
    const struct graph *g = arg;
    assert(g);

    reset_env(env);

    if (g->edge_count == 0) {
        return THEFT_TRIAL_SKIP; /* none */
    }

    struct hopscotch *hopscotch = hopscotch_new();
    if (hopscotch == NULL) { return THEFT_TRIAL_ERROR; }

    for (size_t e_i = 0; e_i < g->edge_count; e_i++) {
        const struct edge *e = &g->edges[e_i];
        if (0) {
            fprintf(stdout, "%u: ", e->from);
            for (size_t i = 0; i < e->to_count; i++) {
                fprintf(stdout, "%u ", e->to[i]);
            }
            fprintf(stdout, "\n");
        }

        set_bit(env->used, e->from);
        if (!hopscotch_add(hopscotch, e->from, e->to_count, e->to)) {
            goto cleanup;
        }

        for (size_t i = 0; i < e->to_count; i++) {
            /* note: there may be duplicates */
            set_bit(env->used, e->to[i]);
        }
    }

    /* Note if there are any cycles, in whatever slow way; if any are
     * found, then save them all as pairs, and then check that, for any
     * pair found, they both appear in the same group. */
    add_cycles(env, g);

    if (!hopscotch_seal(hopscotch)) { goto cleanup; }

    if (!hopscotch_solve(hopscotch, 0, solve_cb, env)) { goto cleanup; }

    if (!postcondition_checks(env)) { goto cleanup; }

    res = THEFT_TRIAL_PASS;

    /* fall through */
cleanup:
    hopscotch_free(hopscotch);
    return res;
}

static size_t
get_env_cycle_count(const struct test_env *env) {
    /* In case of rollover to exactly 0, the TO node_id
     * of the pair will be nonzero. */
    size_t cycle_count = (env->cycle_count > 0
        ? env->cycle_count
        : env->cycles[0].nodes[1] != 0 ? 256 : 0);
    return cycle_count;
}

static void
solve_cb(uint32_t group_id,
    size_t group_count, const uint32_t *group, void *udata) {
    struct test_env *env = udata;
    if (group_count == 0) {
        env->error = "group count == 0";
    }
    if (env->error) { return; }

    (void)group_id;

    /* fprintf(stdout, "## group %u: ", group_id); */

    /* mark seen */
    for (size_t i = 0; i < group_count; i++) {
        /* set error if any seen more than once */
        /* fprintf(stdout, "%u ", group[i]); */
        if (get_bit(env->seen, group[i])) {
            fprintf(stdout, "-- FAIL: seen in multiple groups: %u\n", group[i]);
            env->error = "seen in multiple groups";
            /* fprintf(stdout, "\n"); */
            return;
        }
        set_bit(env->seen, group[i]);
    }
    /* fprintf(stdout, "\n"); */

    if (group_count < 2) { return; }

    /* note cycles */
    const size_t cycle_count = get_env_cycle_count(env);
    for (size_t c_i = 0; c_i < cycle_count; c_i++) {
        if (both_in_group(env->cycles[c_i].nodes, group_count, group)) {
            env->cycles[c_i].found = true;
        }
    }
}

static bool
both_in_group(const uint32_t nodes[2],
    size_t group_count, const uint32_t *group) {
    uint8_t found = 0;
    for (size_t i = 0; i < group_count; i++) {
        if (group[i] == nodes[0]) {
            found |= 0x01;
            if (found == 0x03) { break; }
            continue;
        }
        if (group[i] == nodes[1]) {
            found |= 0x02;
            if (found == 0x03) { break; }
            continue;
        }
    }

    return found == 0x03;
}

static void
add_cycles(struct test_env *env, const struct graph *g) {
    /* Finding cycles, the naive/expensive way */
    for (size_t f_i = 0; f_i < g->edge_count; f_i++) {
        const struct edge *from_e = &g->edges[f_i];
        for (size_t fe_i = 0; fe_i < from_e->to_count; fe_i++) {
            const uint32_t to = from_e->to[fe_i];

            for (size_t t_i = 0; t_i < g->edge_count; t_i++) {
                const struct edge *to_e = &g->edges[t_i];
                if (to_e->from != to) { continue; }

                /* only check when x<y for {x,y} */
                if (from_e->from >= to_e->from) { continue; }

                /* check for a backward edge that completes a cycle */
                for (size_t te_i = 0; te_i < to_e->to_count; te_i++) {
                    if (from_e->from == to_e->to[te_i]) {
                        /* fprintf(stdout, "found cycle? %u -> %u and %u -> %u\n", */
                        /*     from_e->from, from_e->to[fe_i], */
                        /*     to_e->from, to_e->to[te_i]); */
                        env->cycles[env->cycle_count++] = (struct cycle){
                            .nodes = { from_e->from, to_e->from },
                        };
                        assert(from_e->to[fe_i] == to_e->from);
                        assert(to_e->to[te_i] == from_e->from);
                        break;
                    }
                }
            }
        }
    }
}

static bool
postcondition_checks(struct test_env *env) {
    /* no duplicates */
    if (env->error != NULL) {
        fprintf(stdout, "-- FAIL: %s\n", env->error);
        return false;
    }

    const size_t cycle_count = get_env_cycle_count(env);
    if (cycle_count > 0) {
        for (size_t i = 0; i < cycle_count; i++) {
            if (!env->cycles[i].found) {
                fprintf(stdout, "-- FAIL cycle %zu/%zu [%u, %u] not found\n",
                    i, cycle_count, env->cycles[i].nodes[0], env->cycles[i].nodes[1]);
                env->error = "cycle not found";
                return false;
            }
        }
    }

/* checked here
 * - property: each node appears in exactly one group
 * - property: there are at most as many groups as nodes
 * - property: any cycles lead to a CB with > 1 value, inc those nodes
 * - property: any cycles detected the slow/naive way are grouped together
 *
 * TODO:
 * - property: the graph of grouped SCC nodes should be acyclic
 *
 * TODO shuffling
 * - property: successor order shouldn't matter within a node
 * - property: adding pairs vs. as a set shouldn't matter
 *
 * TODO property for timing
 * - property: worst case timing?
 * */

    for (size_t i = 0; i < env->limit64; i++) {
        if (env->seen[i] != env->used[i]) {
            fprintf(stdout,
                "-- FAIL lost some at %zu: seen %"PRIx64 " <-> used %"PRIx64"\n",
                i, env->seen[i], env->used[i]);
            env->error = "lost some used";
            return false;
        }
    }

    return true;
}

SUITE(prop) {
    RUN_TESTp(generate_and_check, 32);
}
