# filestore

Owner: M3 (@Byte-sized-Ali).

The module contains the M3 local store and peer-facing metadata service. Public APIs are in `include/filestore/`; implementation files cover manifest generation, verified chunk storage, placement, bounded message codecs and `FileStore` coordination.

`FileStore` implements `common::IChunkStore` and depends on `ITransport`, `IPeerDirectory` and `IChunkFetcher` from `common`. M2 dispatches M3 messages to `FileStore::on_message`, calls `on_peer_joined` / `on_peer_left`, and supplies the current peer-session snapshot. M4 implements `IChunkFetcher`; successful fetches are verified and persisted by M3 before a HAVE page is sent.

The active M3 scope is issue #51: fixed 16 KiB chunks, full-file and per-chunk SHA-256, immutable manifests, local reconstruction and deterministic two-holder placement. The service also exchanges manifests and confirmed holder bitmaps, replans after joins/leaves, fetches missing assigned chunks through M4, and repairs replicas when a surviving confirmed copy exists.

The first share keeps the input file untouched and stores verified chunks under `<data-dir>/chunk_store/`. During initial distribution, the source retains extra managed chunks until all replacement holders confirm storage. With fewer than three peers, the no-complete-managed-copy rule cannot be satisfied; degraded placement is reported.

The registered `filestore_behavior` CTest covers empty/exact/partial chunk boundaries, persistent verified storage, rejection of unverified chunks, bounded wire pages, out-of-order multi-page HAVE assembly, placement invariants, catalog exchange, explicit degraded/unavailable events, queue-full retry and repair of a corrupt local replica. Its M3-only fake-network scenario distributes a 2 MiB file across four stores, joins a fifth store, verifies that old copies remain while replacement fetches are blocked, then checks the final holder map after retry and publisher departure. It validates M3 coordination without implementing M1, M2 or M4. Real UDP integration remains required work.

See [storage format](../../docs/storage-format.md), [protocol payloads](../../docs/protocol.md#61-m3-payload-layouts), [storage report](../../docs/reports/storage.md), and [team workflow](../../CONTRIBUTING.md).
