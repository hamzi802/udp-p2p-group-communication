# filetransfer

Owner: M4 (@hamzayousuf717-ops).

This is a buildable scaffold, not a completed implementation. Public headers go in `include/filetransfer/`, implementation in `src/`, and behavioral tests in `tests/`.

Tasks:
- I15: Chunk scheduler demonstration using fakes
- I22: Chunk request/data/error and block reassembly
- I24: Initial chunk distribution and storage confirmation
- I27: Parallel download using two remote sources
- I28: Source failover retaining verified progress
- I29: Per-chunk and complete-file hash verification
- I44: Optional: restore lost chunk replicas

Use interfaces from common to avoid circular library dependencies. See [architecture](../../docs/architecture.md) and [team workflow](../../CONTRIBUTING.md).
