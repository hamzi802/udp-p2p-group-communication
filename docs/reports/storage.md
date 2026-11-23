# M3: storage and placement

## Responsibility

M3 owns content identity, fixed-size chunks, immutable manifests, local verified storage, replica placement and metadata handoff. M3 does not own UDP delivery or the transfer scheduler. `FileStore` consumes `ITransport`, `IPeerDirectory` and `IChunkFetcher`; it implements `IChunkStore` for M4.

## Data flow

`build_manifest` streams a source file once, calculates the full-file SHA-256 ID, and records one SHA-256 digest for each 16 KiB chunk. `ChunkStore::store_file` rereads the source, checks every chunk and the complete file ID, writes into a staging directory and publishes only a complete verified set. Original files remain untouched. Reads and downloaded chunks are checked against the immutable manifest before data is returned or advertised.

The local catalog stores each manifest and a holder table keyed by `(peer ID, session)`. Holder tables contain a generation and a complete chunk bitmap. A receiver applies a new bitmap only after every page for that session and generation arrives. A reliable transport ACK indicates message receipt; the sender's HAVE page is the storage confirmation, sent only after verified disk storage.

When a peer joins, catalog exchange provides manifests and current local HAVE maps. Each peer sorts the active numeric peer IDs and uses the first eight file-ID bytes as a deterministic rotation offset. For chunk `c`, positions `(offset + c) mod N` and `(offset + c + 1) mod N` are the intended holders. A peer requests missing assigned chunks through M4, verifies each returned chunk, persists it and then advertises a new HAVE generation. Old chunks remain available until replacements confirm. A failed source is tried again after a five-second cooldown if no other source succeeds. On departure, M3 recomputes placement and attempts repair from any surviving confirmed copy; unavailable chunks are reported explicitly.

## Storage layout and limits

Managed data lives under `<data-dir>/chunk_store/`: chunk paths use a lowercase full file ID and numeric index; manifests and holder snapshots are separate binary files. Display names never form paths. The implementation caps manifests at 1,000,000 chunks and names at 512 bytes. Incomplete state is capped at 64 manifest assemblies/64 MiB of hashes and 1,024 HAVE assemblies/16 MiB of bitmaps, with a 30-second inactivity expiry. PeerApp calls `FileStore::on_timer` for expiry and `retry_pending` periodically after queue-full results. Temporary metadata/chunk writes are renamed into place after validation.

The no-complete-copy invariant applies to managed chunks. The source and rebuilt output are user-owned files in separate locations and are not deleted by M3. With one peer, two holders are impossible; with two peers, both must hold every chunk to maintain two replicas. The service reports these degraded states instead of claiming both rules are met.

## Public entry points

- `hashing/sha256.hpp`: streaming SHA-256, file hashing, and hexadecimal conversion, using OpenSSL EVP.
- `filestore/manifest.hpp`: manifest construction, validation and expected chunk lengths.
- `filestore/chunk_store.hpp`: durable manifests/chunks and verified reads/writes.
- `filestore/placement.hpp`: deterministic target calculation and degraded-state flags.
- `filestore/wire.hpp`: bounded, big-endian M3 payload codecs and page builders.
- `filestore/file_store.hpp`: catalog exchange, holder tracking, membership rebalancing, fetch scheduling and storage events.

The exact M3 packet payloads are in [protocol section 6.1](../protocol.md#61-m3-payload-layouts). The standalone [chunk tool guide](../../apps/chunk_tool/README.md) shows the local chunk, placement and reconstruction workflow.

## Verification status

Verification recorded in WSL: CMake 4.2.3, GCC 15.2.0 and OpenSSL 3.5.5 configured and built every target with Ninja; the project-wide run passed all five registered CTest cases. A later M3-only build rebuilt `cn_hashing_tests`, `cn_filestore_tests` and `chunk_tool`, and both M3 behavior tests passed. Filestore coverage includes bounded metadata payloads and an 8,193-chunk manifest/HAVE exchange, with the final HAVE page sent first to confirm incomplete snapshots are not applied; it also checks that M3 reports insufficient peers and unavailable replicas. The expanded fake-network scenario distributes a deterministic 2 MiB file over four peer stores, joins a fifth store with a full fetch queue, verifies old copies remain until replacement confirmation, then checks exact placement after retry and publisher departure. It checks each chunk has exactly the planned two physical and confirmed holders and that no active peer has the complete managed file.

The `chunk_tool share`, `catalog` and `rebuild` workflow passed for a 2 MiB zero-filled input (128 chunks); the rebuilt file matched byte-for-byte. Truncating one stored chunk made `rebuild` fail, and no output file was published. These checks validate M3 behavior through local storage and fakes; they do not demonstrate real UDP or peer-app integration. Those scenarios remain open until M1, M2 and M4 are implemented and connected.
