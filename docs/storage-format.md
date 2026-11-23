# M3 storage format and behavior

This document records the M3 implementation choices for issue #51 and later storage integration. The peer application and M1/M2/M4 services still need to wire the declared interfaces into the live event loop.

## File identity and chunking

- A file ID is the 32-byte SHA-256 digest of the complete source file.
- M3 uses fixed 16 KiB chunks. Each chunk has its own 32-byte SHA-256 digest in the manifest.
- The final chunk records its exact length. An empty file has zero chunks and the SHA-256 of the empty byte string as its file ID.
- The implementation accepts up to 1,000,000 chunks (about 16.384 GB decimal). Larger files are rejected before a manifest is stored.
- A display name is metadata only, limited to 512 bytes, and is never used to form a storage path.

`FileManifest` records the file ID, display name, total byte length, chunk size, chunk count, final chunk length and ordered chunk hashes. It is immutable for a given file ID. Publishing the same bytes under a conflicting display name is rejected; aliases are not represented in v1.

## Local managed paths

Given `--data-dir DIR`, the chunk tool and `FileStore` use:

```text
DIR/chunk_store/
  chunks/<64 lowercase hex file ID>/<chunk index>.chunk
  manifests/<64 lowercase hex file ID>.manifest
  holders/<64 lowercase hex file ID>.holders
```

Only validated file IDs and numeric chunk indexes form paths. Original inputs and assembled output paths are supplied separately and are never deleted by M3. Chunk and metadata writes use a temporary sibling followed by rename. Initial whole-file ingestion uses a staging directory, verifies every chunk and the complete file hash, then publishes the directory.

Incomplete network state is bounded: at most 64 manifest assemblies and 64 MiB of retained manifest hashes, plus at most 1,024 HAVE assemblies and 16 MiB of retained HAVE bitmaps. Assemblies with no activity for 30 seconds expire. PeerApp calls `FileStore::on_timer` from its monotonic timer hook.

Every chunk read checks both the manifest's expected length and SHA-256. If a local read finds a missing or corrupt chunk, M3 withdraws its HAVE bit and recalculates placement. `put_verified_chunk` rejects an unknown file, invalid index, wrong length or wrong hash. A local holder bit is updated only after the verified bytes have been written. Holder records persist the peer ID, session, monotonically increasing HAVE generation and packed chunk bitmap; records from inactive or old sessions are not considered usable sources.

## Placement and replica safety

For a membership snapshot, peers are sorted by numeric peer ID. Interpret the first eight file-ID bytes as a big-endian integer and take `offset = value mod peer_count`. For chunk `c`, the intended holders are positions `(offset + c) mod N` and `(offset + c + 1) mod N`.

- With fewer than two active peers, two distinct replicas are impossible; the store reports degraded placement.
- With exactly two peers, two replicas are possible only by storing every chunk on both peers. M3 reports that the no-complete-managed-copy rule cannot also hold.
- With at least three peers and at least three chunks, the rotating pair has two distinct holders per chunk and no peer receives every managed chunk.
- A desired assignment is not a confirmed holder. Only complete HAVE snapshots from a peer session count as confirmed.
- A non-target local copy is removed only after every replacement target has advertised the chunk as verified and stored.

This implementation interprets “no peer has every chunk” as a rule for the managed chunk store. The original source remains user-owned outside that store, and reconstructed downloads are separate output files. The instructor did not define whether those user files count toward the rule, so reports and demonstrations must state this interpretation.

## Catalog, joining and recovery

`CATALOG_REQUEST` has an empty payload. A peer replies with one `FILE_ANNOUNCE`, the paged manifest hashes and its own paged HAVE bitmap for each catalog entry. Each peer retains completed manifests and holder snapshots locally, so a publisher can leave while other replicas still have catalog data.

When a peer link is established, `FileStore::synchronize_with_peer` both requests the remote catalog and offers the local catalog. M2 calls this on link establishment and provides the current peer-session snapshot. M3 calculates the updated placement. If this peer is newly assigned a chunk, it asks M4's `IChunkFetcher` to retrieve it from an active confirmed holder. M3 verifies and stores it before advertising its own new HAVE generation.

When membership changes, the service recalculates targets. It keeps old verified copies until new target holders confirm storage. If a source fails, the queued fetch completes with an error and M3 tries another confirmed active holder. A failed source gets a five-second retry cooldown, after which the periodic timer can try it again if no source succeeded. Already verified chunks stay stored. If no live confirmed source remains, M3 emits `replica_unavailable`; it does not claim success or fabricate a copy. Peer departure can trigger placement repair when at least one surviving source and enough active peers remain; this implements the optional I44 recovery path.

## C++ interfaces

`common/file_types.hpp` defines `FileId`, `ChunkKey`, `PeerIdentity` and `FileManifest`. `common/storage_interfaces.hpp` defines:

- `ITransport`: enqueue one reliable application message and return an explicit send result.
- `IPeerDirectory`: provide active remote peer IDs and sessions; `FileStore` adds itself to the placement snapshot.
- `IChunkStore`: query, read, find a manifest and store a verified chunk.
- `IChunkFetcher`: asynchronously fetch a specific chunk from a selected holder. A full M4 queue returns `false` without invoking the completion callback; a queued request completes once on the PeerApp event-loop thread. PeerApp can call `retry_pending` periodically after queue-full results.

M2 dispatches message types `0x30`-`0x33` to `FileStore::on_message`, and calls `on_peer_joined` / `on_peer_left` after membership state changes. M4 receives `FileStore` as its `IChunkStore` and implements `IChunkFetcher`. Concrete libraries do not construct each other.

## Local demonstration

```bash
./build/apps/chunk_tool/chunk_tool share sample.bin --data-dir run/m3 --peers A,B,C,D
./build/apps/chunk_tool/chunk_tool catalog --data-dir run/m3
./build/apps/chunk_tool/chunk_tool rebuild <file-id> --data-dir run/m3 --output downloads/sample.bin
```

The `share` command prints every chunk hash and the intended placement. Its labels are pretend peers: the local command does not perform remote transfers or claim that those peers confirmed storage. The rebuilt output is written to a temporary file, checked against the full-file SHA-256, then renamed into place. Existing output is protected unless `--force` is supplied.

See [protocol v1, M3 payload layouts](protocol.md#61-m3-payload-layouts) and the [storage report](reports/storage.md).
