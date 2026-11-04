# Storage draft — M3 owns I04

Proposed storage chunk size: 16 KiB. File ID: full-file SHA-256 (32 bytes). Each immutable manifest includes filename/display metadata, exact total length, chunk size/count, final-chunk length and per-chunk hashes.

Define the exact serialized representation and bounds with M1 under I04. Keep intended placement separate from confirmed holder/session records. Catalog, manifest and HAVE information must be paged to fit the datagram budget.

Managed paths are keyed by validated file ID and chunk index, never unsanitized user-provided paths. Originals, managed chunks and assembled outputs belong in separate locations. Never delete original user files.

Confirm a replica only after its entire chunk is verified and written. For joins, retain old verified copies until replacement holders confirm. Handle insufficient live peers and unavailable chunks explicitly. See architecture.md and issues I12/I13/I21/I24–I26/I35/I44.
