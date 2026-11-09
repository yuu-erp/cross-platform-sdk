# Module dependency graph

Edges mean "may include `<to>/api/`". Must stay acyclic. Update this file in the same PR that adds an edge.
Registration order in `src/core/composition_root.cpp` must be a topological order of this graph.

```text
network      (no deps)
wallet       (no deps yet)
smc          (no deps yet)
```
