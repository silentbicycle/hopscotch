# Hopscotch

An implementation of Tarjan's Strongly Connected Components algorithm.

This analyzes a directed graph and yields the graph's nodes in
reverse-topologically sorted order, grouped into strongly connected
components. In other words, it groups nodes together that refer to
each other with cycle(s), and orders the groups so that every group
only refers to nodes appearing in later groups.

For a nice overview of the algorithm, see Vaibhav Sagar's
["An All-in-One DAG Toolkit"][1] post.

This builds both a C library and a standalone command-line program. The
latter previously appeared in a slightly different form, as
[my winning 2018 IOCCC entry][2].

[1]: http://vaibhavsagar.com/blog/2017/06/10/dag-toolkit/
[2]: https://www.ioccc.org/years.html#2018_vokes

Note: The TSCC algorithm assumes that its input is a single connected
graph. If there are disconnected subgraphs in the input, their relative
order will be based on where they appear in the input.


## Building

To build, just run `make` (GNU make).

To run the tests, run `make test`. Note that the property tests depend
on [theft](https://github.com/silentbicycle/theft) to build.


## Example

Given the following input:

    $ build/hopscotch << HERE
    1: 2
    2: 3 4
    3: 4 5
    4: 3 5
    5: 6 7
    6: 7 8
    7: 6 8
    HERE

it will print:

    0: 8
    1: 6 7
    2: 5
    3: 3 4
    4: 2
    5: 1

because 8 doesn't depend on anything, then 6 and 7 reference each other
but otherwise only refer to 8, 5 only refers to 6 and 7, etc.

The equivalent C API usage looks like:

    struct hopscotch *t = hopscotch_new();

    struct {
        uint32_t id;
        size_t count;
        uint32_t successors[MAX_COUNT];
    } rows[] = {
        { 1, 1, { 2 }, },
        { 2, 2, { 3, 4 }, },
        { 3, 2, { 4, 5 }, },
        { 4, 2, { 3, 5 }, },
        { 5, 2, { 6, 7 }, },
        { 6, 2, { 7, 8 }, },
        { 7, 2, { 6, 8 }, },
    };
    for (size_t i = 0; i < sizeof(rows)/sizeof(rows[0]); i++) {
        if (!hopscotch_add(t, rows[i].id, rows[i].count, rows[i].successors)) {
            /* handle error */
        }
    }

    if (!hopscotch_seal(t)) { /* handle error */ }
    hopscotch_solve(t, 0, callback, NULL);
    hopscotch_free(t);

and the callback will be called with each group ID, count, and
array of members. The last argument to `hopscotch_solve` is opaque
to hopscotch, but passed to the callback, so the callback can be
used as a closure.


## Diagrams

The command-line program can also generate [Graphviz dot][3], by
using the `-d` flag:

    $ build/hopscotch -d < examples/abc

[3]: https://www.graphviz.org/

This output can be used to generate PNGs, SVGs, etc.

![](/examples/abc.png)


Several environment variables can be used to add attributes to the generated DOT:

    env HOPSCOTCH_DOT_GRAPH_ATTR='bgcolor=black' \
        HOPSCOTCH_DOT_NODE_ATTR='color=mintcream;fontcolor="#ffffff";fontname="terminus"' \
        HOPSCOTCH_DOT_EDGE_ATTR='color=mediumspringgreen' \
        HOPSCOTCH_DOT_CLUSTER_ATTR='color=mediumturquoise' \
        build/hopscotch -d < examples/abc

[...]


![](/examples/abc-custom_color.png)
