/*
 * Copyright (c) 2017-18 Scott Vokes <vokes.s@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <string.h>

#include <getopt.h>

#include "hopscotch.h"
#include "symtab.h"

char buf[64 * 1024LLU];

#define DEF_LINE_IDS_CEIL 3

struct main_env {
    struct hopscotch *t;
    struct symtab *s;
    bool dot;

    FILE *in;

    /* Buffer for IDs read from the current line, grown on demand */
    uint8_t line_ids_ceil;
    uint32_t *line_ids;
};

/* static bool
 * intern(struct main_env *env, size_t len, const char *name,
 *     struct symtab_symbol **s, uint32_t *id); */

static void usage(const char *msg) {
    if (msg) { fprintf(stderr, "%s\n\n", msg); }
    fprintf(stderr, "hopscotch v. %d.%d.%d by %s\n",
        HOPSCOTCH_VERSION_MAJOR, HOPSCOTCH_VERSION_MINOR,
        HOPSCOTCH_VERSION_PATCH, HOPSCOTCH_AUTHOR);
    fprintf(stderr,
        "Usage: hopscotch [-d] [input_file]\n"
        );
    exit(1);
}

static void handle_args(struct main_env *env, int argc, char **argv) {
    int fl;
    while ((fl = getopt(argc, argv, "dh")) != -1) {
        switch (fl) {
        case 'd':               /* dot */
            env->dot = true;
            break;
        case 'h':               /* help */
            usage(NULL);
            break;
        case '?':
        default:
            usage(NULL);
        }
    }

    argc -= (optind - 1);
    argv += (optind - 1);

    if (argc > 1) {
        env->in = fopen(argv[1], "r");
        if (env->in == NULL) {
            err(1, "fopen: %s", argv[1]);
        }
    }
}

static void
print_cb(uint32_t group_id, size_t count, const uint32_t *group, void *udata);

#define MAX_LENGTH 256

struct symbol {
    uint8_t len;
    char name[MAX_LENGTH];
};

static const char *
getenv_attr(const char *key) {
    const char *res = getenv(key);
    if (res == NULL) { return ""; }
    return res;
}

static const char *indent_regular = "    ";
static const char *indent_cluster = "        ";

static void print_cb(uint32_t group_id,
        size_t group_count, const uint32_t *group, void *udata) {
    (void)udata;
    struct main_env *env = (struct main_env *)udata;
    assert(env);

    if (env->dot) {
        bool cluster = group_count > 1;
        const char *indent = indent_regular;
        if (cluster) {
            printf("%ssubgraph cluster_%d {\n", indent, group_id);
            indent = indent_cluster;
            printf("%sgraph [%s];\n", indent, getenv_attr("HOPSCOTCH_DOT_CLUSTER_ATTR"));
        }

        for (size_t g_i = 0; g_i < group_count; g_i++) {
            const uint32_t root_id = group[g_i];
            const struct symtab_symbol *sym = symtab_get(env->s, root_id);
            assert(sym);
            printf("%sn%u [label=\"%s\"];\n", indent, root_id, sym->str);
        }

        if (cluster) {
            printf("    }\n");
            indent = indent_regular;
        }

        for (size_t g_i = 0; g_i < group_count; g_i++) {
            const uint32_t root_id = group[g_i];

            const uint32_t *successors = NULL;
            size_t succ_count = 0;
            if (!hopscotch_get_successors(env->t, root_id, &succ_count, &successors)) {
                assert(false);
            }

            for (size_t s_i = 0; s_i < succ_count; s_i++) {
                const uint32_t edge_id = successors[s_i];
                printf("%sn%u -> n%u\n", indent, edge_id, root_id);
            }
        }

        printf("\n");

    } else {
        printf("%u: ", group_id);
        for (size_t i = 0; i < group_count; i++) {
            const struct symtab_symbol *sym = symtab_get(env->s, group[i]);
            assert(sym);
            printf("%s ", sym->str);
        }
        printf("\n");
    }
}

int main(int argc, char **argv) {
    int res = EXIT_SUCCESS;
    struct main_env env = {
        .in = stdin,
    };
    handle_args(&env, argc, argv);

    env.t = hopscotch_new();
    assert(env.t);
    env.s = symtab_new(NULL);
    assert(env.s);

    env.line_ids_ceil = DEF_LINE_IDS_CEIL;
    env.line_ids = malloc((1LLU << DEF_LINE_IDS_CEIL) * sizeof(*env.line_ids));

    for (;;) {
        char *line = fgets(buf, sizeof(buf) - 1, env.in);
        if (line == NULL) { break; }
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }

        /* Allow comment lines */
        if (line[0] == '#') { continue; }

        const char *head = strtok(line, ": \t");
        if (head == NULL) { continue; }

        /* get symbol and id for head */
        struct symtab_symbol *sym_head = NULL;
        enum symtab_intern_res ires =
          symtab_intern(env.s, strlen(head), (uint8_t *)head,
              &sym_head);
        assert(ires == SYMTAB_INTERN_CREATED || ires == SYMTAB_INTERN_EXISTING);

        size_t used = 0;
        for (;;) {
            const char *succ = strtok(NULL, " \t");
            if (succ == NULL) { break; }
            if (succ[0] == '\0') { continue; }

            /* get symbol for succ and append id */
            struct symtab_symbol *sym_succ = NULL;
            ires = symtab_intern(env.s, strlen(succ), (uint8_t *)succ,
                &sym_succ);
            assert(ires == SYMTAB_INTERN_CREATED || ires == SYMTAB_INTERN_EXISTING);

            if (used == (1LLU << env.line_ids_ceil)) {
                const uint8_t nceil = env.line_ids_ceil + 1;
                uint32_t *nline_ids = realloc(env.line_ids,
                    (1LLU << nceil) * sizeof(*nline_ids));
                if (nline_ids == NULL) {
                    res = EXIT_FAILURE;
                    goto cleanup;
                }
                env.line_ids_ceil = nceil;
                env.line_ids = nline_ids;
            }
            env.line_ids[used] = sym_succ->id;
            used++;
        }

        if (!hopscotch_add(env.t, sym_head->id, used, env.line_ids)) {
            res = EXIT_FAILURE;
            goto cleanup;
        }
    }

    if (!hopscotch_seal(env.t)) {
        res = EXIT_FAILURE;
        goto cleanup;
    }

    if (env.dot) {
        const char *indent = "    ";
        printf("digraph {\n");
        printf("%sgraph [%s];\n", indent, getenv_attr("HOPSCOTCH_DOT_GRAPH_ATTR"));
        printf("%snode [%s];\n", indent, getenv_attr("HOPSCOTCH_DOT_NODE_ATTR"));
        printf("%sedge [%s];\n", indent, getenv_attr("HOPSCOTCH_DOT_EDGE_ATTR"));
    }

    if (!hopscotch_solve(env.t, 0, print_cb, &env)) {
        res = EXIT_FAILURE;
        goto cleanup;
    }

    if (env.dot) { printf("}\n"); }

cleanup:
    symtab_free(env.s);
    hopscotch_free(env.t);
    return res;
}
