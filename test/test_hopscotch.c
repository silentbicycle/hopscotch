#include "test_hopscotch.h"

#define MAX_MEMBERS_BUF 16

struct input_group {
    uint32_t from;
    char to[MAX_MEMBERS_BUF];
};

struct expected_group {
    uint32_t group_id;
    char members[MAX_MEMBERS_BUF];
};

struct example_env {
    char tag;
    bool error;

    size_t in_count;
    struct input_group *in;
    size_t exp_count;
    struct expected_group *exp;
    bool match;

    // example
    bool abe;
    bool cdh;
    bool fg;
};

#define INIT_ENV(ENV, IN, EXP)                                         \
    do {                                                               \
        memset(&ENV, 0x00, sizeof(ENV));                               \
        ENV.tag = 'E';                                                 \
        ENV.in_count = sizeof(IN)/sizeof(IN[0]);                       \
        ENV.exp_count = sizeof(EXP)/sizeof(EXP[0]);                    \
        ENV.in = IN;                                                   \
        ENV.exp = EXP;                                                 \
    } while(0);

#define ADD_INPUT(ENV, T)                                              \
    do {                                                               \
        printf("\n==== in\n");                                         \
        for (size_t row_i = 0; row_i < ENV.in_count; row_i++) {        \
            size_t offset = 0;                                         \
            const struct input_group *row = &ENV.in[row_i];            \
            uint32_t succs[8];                                         \
            size_t count = strlen(row->to);                            \
            printf("%c: ", row->from);                                 \
            for (size_t i = 0; i < count; i++) {                       \
                if (row->to[offset] == ' ') { offset++; }              \
                if (row->to[offset] == '\0') {                         \
                    count = i;                                         \
                    break;                                             \
                }                                                      \
                succs[i] = row->to[offset++] - 'a';                    \
                printf("%c", succs[i] + 'a');                          \
            }                                                          \
            printf("\n");                                              \
            ASSERT(hopscotch_add(t, row->from - 'a', count, succs));   \
        }                                                              \
        ASSERT(hopscotch_seal(t));                                     \
        printf("\n==== out\n");                                        \
    } while(0)

struct api_use_env {
    bool error;
};

static void
api_use_cb(uint32_t group_id,
    size_t group_count, const uint32_t *group, void *udata) {
    struct api_use_env *env = udata;

    struct exp_row {
        uint8_t count;
        uint32_t ids[2];
    } rows[] = {
        { 1, { 2 } },
        { 2, { 1, 4 } },
        { 1, { 3 } },
        { 1, { 10 } },
        { 1, { 11 } },
        { 2, { 5, 9 } },
    };

    if (group_count != rows[group_id].count) {
        fprintf(stderr, "COUNT: group %u, exp %u, got %zu\n",
            group_id, rows[group_id].count, group_count);
        env->error = true;
        return;
    }

    for (size_t i = 0; i < group_count; i++) {
        if (rows[group_id].ids[i] != group[i]) {
            fprintf(stderr, "ELEMENT: group %u, exp %u, got %u\n",
                group_id, group[i], rows[group_id].ids[i]);
            env->error = true;
            return;
        }
    }
}

TEST bare_api_use(void) {
    struct hopscotch *t = hopscotch_new();
    ASSERT(t);

    const uint32_t succ_5[] = { 1, 3, 9 };
    ASSERT(hopscotch_add(t, 5, 3, succ_5));

    /* Calling `hopscotch_add` with the same node
     * multiple times is fine. */
    const uint32_t succ_1a[] = { 2, };
    const uint32_t succ_1b[] = { 4, };
    ASSERT(hopscotch_add(t, 1, 1, succ_1a));
    ASSERT(hopscotch_add(t, 1, 1, succ_1b));

    const uint32_t succ_4[] = { 1, };
    ASSERT(hopscotch_add(t, 4, 1, succ_4));

    /* If the node has 0 successors, *successors can be NULL. */
    ASSERT(hopscotch_add(t, 11, 0, NULL));

    /* Also, if a node has 0 successors, it doesn't need to have
     * `hopscotch_add` called at all -- just appearing as a successor for
     * another node is enough. */

    const uint32_t succ_9[] = { 10, 11, 5 };
    ASSERT(hopscotch_add(t, 9, 3, succ_9));

    ASSERT(hopscotch_seal(t));

    struct api_use_env env = { .error = false };
    ASSERT(hopscotch_solve(t, 0, api_use_cb, &env));

    ASSERT(!env.error);

    hopscotch_free(t);
    PASS();
}

static void
example_hopscotch_shape_cb(uint32_t group_id,
    size_t group_count, const uint32_t *group, void *udata) {
    struct api_use_env *env = udata;

    struct exp_row {
        uint8_t count;
        uint32_t ids[2];
    } rows[] = {
        { 1, { 8 } },
        { 2, { 6, 7 } },
        { 1, { 5 } },
        { 2, { 3, 4 } },
        { 1, { 2 } },
        { 1, { 1 } },
    };

    if (group_count != rows[group_id].count) {
        fprintf(stderr, "COUNT: group %u, exp %u, got %zu\n",
            group_id, rows[group_id].count, group_count);
        env->error = true;
        return;
    }

    for (size_t i = 0; i < group_count; i++) {
        if (rows[group_id].ids[i] != group[i]) {
            fprintf(stderr, "ELEMENT: group %u, exp %u, got %u\n",
                group_id, group[i], rows[group_id].ids[i]);
            env->error = true;
            return;
        }
    }
}

TEST example_hopscotch_shape(void) {
    struct hopscotch *t = hopscotch_new();
    ASSERT(t);

#define MAX_COUNT 3
    struct {
        uint32_t id;
        size_t count;
        uint32_t s[MAX_COUNT];
    } rows[] = {
        { .id = 1, .count = 1, .s = { 2 }, },
        { .id = 2, .count = 2, .s = { 3, 4 }, },
        { .id = 3, .count = 2, .s = { 4, 5 }, },
        { .id = 4, .count = 2, .s = { 3, 5 }, },
        { .id = 5, .count = 2, .s = { 6, 7 }, },
        { .id = 6, .count = 2, .s = { 7, 8 }, },
        { .id = 7, .count = 2, .s = { 6, 8 }, },
    };
    for (size_t i = 0; i < sizeof(rows)/sizeof(rows[0]); i++) {
        ASSERT(hopscotch_add(t, rows[i].id, rows[i].count, rows[i].s));
    }

    ASSERT(hopscotch_seal(t));

    struct api_use_env env = { .error = false };
    ASSERT(hopscotch_solve(t, 0, example_hopscotch_shape_cb, &env));

    ASSERT(!env.error);

    hopscotch_free(t);
    PASS();
}

static void
match_cb(uint32_t group_id, size_t count, const uint32_t *group,
    void *udata) {
    struct example_env *env = (struct example_env *)udata;
    if (env->tag != 'E') { return; }

    char buf[8] = { 0 };
    uint8_t used = 0;

    printf("%u: ", group_id);

    for (size_t i = 0; i < count; i++) {
        buf[used++] = group[i] + 'a';
        buf[used++] = ' ';
    }
    if (used > 0) { used--; }   /* truncate trailing ' ' */
    buf[used] = 0;
    printf("%s\n", buf);

    if (group_id >= env->exp_count) {
        fprintf(stderr, "Error at: group_id %u\n", group_id);
        env->error = true;
        return;
    } else {
        struct expected_group *exp = &env->exp[group_id];
        assert(exp->group_id == group_id);
        if (0 == strcmp(buf, exp->members)) {
            if (group_id == env->exp_count - 1) {
                env->match = true;
            }
        } else {
            fprintf(stderr, "Error at group_id %u: exp '%s' got '%s'\n",
                group_id, exp->members, buf);
            env->error = true;
            return;
        }
    }
}

#define HOPSCOTCH_TEST()                                               \
    struct example_env env;                                            \
    INIT_ENV(env, in, exp);                                            \
    ADD_INPUT(env, t);                                                 \
                                                                       \
    ASSERT(hopscotch_solve(t, 0, match_cb, &env));                     \
                                                                       \
    hopscotch_free(t);                                                 \
    ASSERT(!env.error);                                                \
    ASSERT(env.match)


static void
set_flag_cb(uint32_t group_id, size_t count, const uint32_t *group,
    void *udata) {
    (void)group_id;
    (void)count;
    (void)group;
    bool *flag = (bool *)udata;
    *flag = true;
}

TEST empty(void) {
    struct hopscotch *t = hopscotch_new();
    ASSERT(hopscotch_seal(t));
    bool group_found = false;
    ASSERT(hopscotch_solve(t, 0, set_flag_cb, (void *)&group_found));
    hopscotch_free(t);
    ASSERT(!group_found);
    PASS();
}

TEST one(void) {
    struct hopscotch *t = hopscotch_new();

    struct input_group in[] = {
        { 'a', "" },
    };

    struct expected_group exp[] = {
        { 0, "a", },
    };

    HOPSCOTCH_TEST();
    PASS();
}

TEST one_nonzero(void) {
    struct hopscotch *t = hopscotch_new();

    struct input_group in[] = {
        { 'b', "" },
    };

    struct expected_group exp[] = {
        { 0, "b", },
    };

    HOPSCOTCH_TEST();
    PASS();
}

TEST one_arc(void) {
    struct hopscotch *t = hopscotch_new();

    struct input_group in[] = {
        { 'a', "b" },
    };

    struct expected_group exp[] = {
        { 0, "b", },
        { 1, "a", },
    };

    HOPSCOTCH_TEST();
    PASS();
}

TEST one_cycle(void) {
    struct hopscotch *t = hopscotch_new();

    struct input_group in[] = {
        { 'a', "b" },
        { 'b', "a c" },
    };

    struct expected_group exp[] = {
        { 0, "c", },
        { 1, "a b", },
    };

    HOPSCOTCH_TEST();
    PASS();
}

TEST self_cycle(void) {
    struct hopscotch *t = hopscotch_new();

    struct input_group in[] = {
        { 'a', "a" },
    };

    struct expected_group exp[] = {
        { 0, "a", },
    };

    HOPSCOTCH_TEST();
    PASS();
}

TEST example(void) {
    struct hopscotch *t = hopscotch_new();

    struct input_group in[] = {
        { 'a', "b" },
        { 'b', "c e f" },
        { 'c', "d g" },
        { 'd', "c h" },
        { 'e', "a f" },
        { 'f', "g" },
        { 'g', "f" },
        { 'h', "d g" },
    };

    struct expected_group exp[] = {
        { 0, "f g", },
        { 1, "c d h", },
        { 2, "a b e", },
    };

    HOPSCOTCH_TEST();

    PASS();
}

TEST example_disconnected(void) {
    struct hopscotch *t = hopscotch_new();

    struct input_group in[] = {
        { 'a', "b" },
        { 'b', "c e f" },
        { 'c', "d g" },
        { 'd', "c h" },
        { 'e', "a f" },
        { 'f', "g" },
        { 'g', "f" },
        { 'h', "d g" },
        { 'i', "" },            /* disconnected node */
    };

    /* Check that completely unconnected nodes are emitted first.
     * While this is possibly outside the scope of the algorithm
     * (since it takes a graph, not a set of nodes and arcs that
     * form multiple distinct graphs), the algorithm can support
     * it with only a minor change, and it's useful. */
    struct expected_group exp[] = {
        { 0, "i", },
        { 1, "f g", },
        { 2, "c d h", },
        { 3, "a b e", },
    };

    HOPSCOTCH_TEST();

    PASS();
}

TEST example_disconnected_cycle(void) {
    struct hopscotch *t = hopscotch_new();

    struct input_group in[] = {
        { 'a', "b" },
        { 'b', "c e f" },
        { 'c', "d g" },
        { 'd', "c h" },
        { 'e', "a f" },
        { 'f', "g" },
        { 'g', "f" },
        { 'h', "d g" },
        { 'i', "i" },           /* disconnected node with self cycle */
    };

    struct expected_group exp[] = {
        { 0, "i", },
        { 1, "f g", },
        { 2, "c d h", },
        { 3, "a b e", },
    };

    HOPSCOTCH_TEST();

    PASS();
}

TEST max_depth_limit(void) {
    // First pass: within limit, allowed
    {
        struct hopscotch *t = hopscotch_new();
        for (size_t i = 0; i < 9; i++) {
            uint32_t succ[1] = { i + 1 };
            ASSERT(hopscotch_add(t, i, 1, succ));
        }

        {
            /* And the last node links back to the first */
            uint32_t succ[1] = { 0, };
            ASSERT(hopscotch_add(t, 9, 1, succ));
        }

        ASSERT(hopscotch_seal(t));
        ASSERTm("with a sufficient recursion depth",
            hopscotch_solve(t, 10, NULL, NULL));
        ASSERT_EQ_FMT(HOPSCOTCH_ERROR_NONE, hopscotch_error(t), "%d");
        hopscotch_free(t);
    }

    // Second pass: limit exceeded, rejected
    {
        struct hopscotch *t = hopscotch_new();
        for (size_t i = 0; i < 9; i++) {
            uint32_t succ[1] = { i + 1 };
            ASSERT(hopscotch_add(t, i, 1, succ));
        }

        {
            /* And the last node links back to the first */
            uint32_t succ[1] = { 0, };
            ASSERT(hopscotch_add(t, 9, 1, succ));
        }

        ASSERT(hopscotch_seal(t));
        ASSERTm("limiting recursion depth makes it fail",
            !hopscotch_solve(t, 9, NULL, NULL));
        ASSERT_EQ_FMT(HOPSCOTCH_ERROR_RECURSION_DEPTH, hopscotch_error(t), "%d");
        hopscotch_free(t);
    }

    PASS();
}

SUITE(basic) {
    RUN_TEST(bare_api_use);
    RUN_TEST(example_hopscotch_shape);

    RUN_TEST(empty);
    RUN_TEST(one);
    RUN_TEST(one_nonzero);
    RUN_TEST(one_arc);
    RUN_TEST(one_cycle);
    RUN_TEST(self_cycle);
    RUN_TEST(example);
    RUN_TEST(example_disconnected);
    RUN_TEST(example_disconnected_cycle);
    RUN_TEST(max_depth_limit);
}

/* Add all the definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();      /* command-line arguments, initialization. */
    RUN_SUITE(basic);
    RUN_SUITE(prop);
    RUN_SUITE(bench);
    GREATEST_MAIN_END();        /* display results */
}
