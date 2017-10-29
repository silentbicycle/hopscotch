#include "test_hopscotch.h"
#include <sys/time.h>
#include <inttypes.h>

static uint64_t x128p_next(uint64_t s[2]);

#if 0
#include <stdio.h>
#define LOG(...) fprintf(stdout, __VA_ARGS__)
#else
#define LOG(...)
#endif

struct hopscotch *
random_graph(uint64_t seed, uint32_t max_id, size_t max_count, size_t limit) {
    /* (uint64_t)sha1sum("meaningless") */
    static const uint64_t meaningless = 0xd09685772865bbb2LLU;
    uint64_t state[2] = { seed ^ meaningless, ~(seed ^ meaningless), };

    struct hopscotch *t = hopscotch_new();
    if (t == NULL) { return NULL; }

    for (size_t i = 0; i < limit; i++) {
        const uint32_t id = ((uint32_t)x128p_next(state)) % max_id;
        const size_t count = x128p_next(state) % max_count;
        {
            uint32_t succ[count];
            LOG("%u: ", id);
            for (size_t si = 0; si < count; si++) {
                succ[si] = ((uint32_t)x128p_next(state)) % max_id;
                LOG("%u ", succ[si]);
            }
            LOG("\n");
            if (!hopscotch_add(t, id, count, succ)) {
                hopscotch_free(t);
                return NULL;
            }
        }
    }

    if (!hopscotch_seal(t)) {
        hopscotch_free(t);
        return NULL;
    }
    return t;
}

static size_t
msec_of_delta(struct timeval *pre, struct timeval *post) {
    return 1000 * (post->tv_sec - pre->tv_sec)
      + (post->tv_usec - pre->tv_usec)/1000;
}

/* xoroshiro128+ PRNG, by David Blackman and Sebastiano Vigna.
 * For details, see http://xoroshiro.di.unimi.it/xoroshiro128plus.c . */
static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}
static uint64_t x128p_next(uint64_t s[2]) {
    const uint64_t s0 = s[0];
    uint64_t s1 = s[1];
    const uint64_t result = s0 + s1;

    s1 ^= s0;
    s[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14); // a, b
    s[1] = rotl(s1, 36); // c

    return result;
}

TEST gen(void) {
    const size_t max_seed = 10;
    uint64_t seed_base;
    struct timeval now;
    ASSERT_EQ(0, gettimeofday(&now, NULL));
    seed_base = now.tv_sec ^ now.tv_usec;

    for (size_t seed = 0; seed < max_seed; seed++) {

        const uint32_t max_id = 256;
        const size_t max_count = 64;
        const size_t limit = 1000000;

        struct hopscotch *t = random_graph(seed + seed_base, max_id, max_count, limit);
        ASSERT(t);

        struct timeval pre, post;
        ASSERT(0 == gettimeofday(&pre, NULL));
        ASSERT(hopscotch_solve(t, 0, NULL, NULL));
        ASSERT(0 == gettimeofday(&post, NULL));

        const uint64_t msec = msec_of_delta(&pre, &post);
        printf("seed %"PRIx64" max_id %u, max_count %zu, limit %zu -- msec %"PRIu64"\n",
            seed + seed_base, max_id, max_count, limit, msec);

        hopscotch_free(t);
    }
    PASS();
}

TEST gen_chain_with_cycle(size_t count) {
    struct hopscotch *t = hopscotch_new();
    if (count == 0) { SKIPm("bad count"); }
    ASSERT(t);

    for (size_t i = 0; i < count - 1; i++) {
        uint32_t succ[1] = { i + 1, };
        ASSERT(hopscotch_add(t, i, 1, succ));
    }

    {
        /* And the last node links back to the first */
        uint32_t succ[1] = { 0, };
        ASSERT(hopscotch_add(t, count - 1, 1, succ));
    }

    ASSERT(hopscotch_seal(t));

    struct timeval pre, post;
    ASSERT(0 == gettimeofday(&pre, NULL));
    ASSERT(hopscotch_solve(t, 0 /* use default */, NULL, NULL));
    ASSERT(0 == gettimeofday(&post, NULL));

    const uint64_t msec = msec_of_delta(&pre, &post);
    printf("chain, count %zu -- msec %"PRIu64"\n", count, msec);

    hopscotch_free(t);
    PASS();
}


SUITE(bench) {
    RUN_TEST(gen);

    /* Note: a sufficiently deep chain of references can overflow
     * the stack; on this particular computer, ten million nodes
     * `a -> b -> c -> ... -> a` will do it.
     *
     * This could be avoided by restructuring the code to use an
     * explicit stack, rather than the C call stack. The crash
     * is currently prevented with a max stack depth parameter. */
    for (size_t i = 1; i < 1000000; i *= 10) {
        RUN_TESTp(gen_chain_with_cycle, i);
    }
}
