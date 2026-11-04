# filestore

Owner: M3 (@Byte-sized-Ali).

This is a buildable scaffold, not a completed implementation. Public headers go in `include/filestore/`, implementation in `src/`, and behavioral tests in `tests/`.

Tasks:
- I12: Fixed-size chunking and SHA-256 hashing
- I13: Local chunk store and manifest generation
- I21: Two-holder placement and storage invariant checks
- I24: Initial chunk distribution and storage confirmation
- I25: Replicated catalog, manifests and holder records
- I26: Joining-peer metadata and storage handoff
- I35: Replica placement and publisher-departure tests
- I44: Optional: restore lost chunk replicas

Use interfaces from common to avoid circular library dependencies. See [architecture](../../docs/architecture.md) and [team workflow](../../CONTRIBUTING.md).
