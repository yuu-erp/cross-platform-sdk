# Module dependency graph

Edges mean "may include `<to>/api/`". Must stay acyclic. Update this file in the same PR that adds an edge.
Registration order in `src/core/composition_root.cpp` must be a topological order of this graph.

```text
crypto       (no deps)        hash / random / BLS / secp256k1
network      (no deps)        publishes network.connected / network.disconnected
wallet -> crypto            offline create/import/public metadata/sign
smc          (no deps yet)
```
