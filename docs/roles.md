# Role sheets

Confirmed accounts: M1 @Musab-Farooq; M2 @hamzi802; M3 @Byte-sized-Ali; M4 @hamzayousuf717-ops.

The following task descriptions are transcribed from the project proposal. Names and owned paths above take precedence over the proposal's unfilled role choices.

Role M1 Reliable UDP and protocol
You build the packet-delivery service and coordinate shared protocol decisions. M2 shares
human review and M4 owns build and test automation. Choose this role if you enjoy
packet logic, timers and careful debugging.

Files and classes you own
libs/common/include/common/ contains shared types and interfaces.
libs/rudp/include/rudp/ and libs/rudp/src/ contain ReliableTransport, UdpSocket,
PacketCodec and ReliableChannel. apps/rudp_demo/ is your independent demonstration.
Add your own module tests and CMake target.

Work to implement
1. Write the envelope encoder and decoder using fixed-width integer types. Reject short,
oversized, malformed and unsupported-version packets. Never transmit the raw memory
of a C++ object.
2. Build a nonblocking UDP socket wrapper with automatic cleanup. One transport object
owns the socket. It exposes readiness and timer hooks for the main loop.
3. Track reliable state for each remote session and stream: next sequence,
unacknowledged packets, send timestamps, retry counts, next expected sequence and
buffered later packets.
4. Implement ACK/SACK handling, timeouts, backoff, duplicate suppression and ordered
delivery. Check the advertised receive window and stop accepting work when bounded
queues fill.
5. Provide both reliable sends and the limited unreliable discovery/liveness path. Separate
control/chat and bulk streams and avoid starving either.
6. Report success, queue-full and terminal failure explicitly. Agree with M2 on
handshake/session resets and with M4 on file-block delivery.

Your first coding task
Create a packet object, encode it into bytes, then decode it and compare every field. Test
an invalid length and wrong version. After that, send the same bytes between two
processes using UDP. This gives the team a small foundation before the reliability
machinery is added.

How to prove your module works
Send numbered messages between two real processes. Inject recoverable loss,
duplicated packets, reordered packets and lost ACKs. Delivered messages must appear
once and in order. Also demonstrate a permanent-failure result, bounded buffers, a peer
restart and progress on both streams.

What you hand to the team
A documented ITransport implementation, packet-format examples, a working
rudp_demo, tests and the protocol/reliability report section. M2 and M4 must know how to
service the socket and timers. Shared interface changes need review from affected
implementers.

---

Role M2 Membership chat and application
Your job is to make the program feel like a group application. You decide who is currently
reachable, handle chat commands and connect the services. This is the coordinator's
current role choice, and you share human code review with M1.

Files and classes you own
libs/membership/ contains Membership and Peer records. libs/chat/ contains ChatService.
apps/peer/ contains main.cpp, command parsing and module wiring. Your public view of
the peer list implements IPeerDirectory.

Work to implement
1. Store each peer's ID, session, display name, endpoint, last-seen timestamp and state in
a dynamic container. Keep display names separate from protocol identities.
2. Implement bootstrap JOIN/WELCOME/HELLO and peer-list resynchronization. Repeated
join packets must be safe, and joining must not depend on one permanent coordinator.
3. Send heartbeats and handle LEAVE. Raise membership-change events when peers join
or become unreachable. Treat timeouts as observations, not proof of a crash.
4. Implement group chat as one send to each active remote peer. Preserve
sender/session identity and chat order. Show local messages once and show delivery
failures clearly.
5. Implement /peers, /msg, /share, /files, /get and /quit. The last three file commands call
M3/M4 interfaces rather than implement file logic themselves.
6. Construct all services in PeerApp and dispatch received messages to the right handler.
Keep input, timers, chat and file activity moving in one nonblocking event loop.

Your first coding task
Build a Peer record and a peer table. Add four sample entries, prevent duplicates, remove
one and print the result through /peers. Do this without a network first. Then connect the
same behavior to a fake ITransport and finally to M1's real implementation.

How to prove your module works
Start four peers, join through A and compare their peer lists. Exit A and show that B, C
and D still chat. Join E through B. Kill another peer and show timeout detection. Send
multiple messages while a large file is being downloaded and check that neither chat nor
heartbeats stop.

What you hand to the team
Membership and chat libraries, the main executable, documented membership events,
CLI help, tests and your report section. M3/M4 must receive a stable active-peer snapshot
and change events. M1 must know when a new peer session is accepted.
Choose this role if you like user commands, state machines and seeing the whole system
work together. Useful preparation: vectors/maps, callbacks, simple command parsing and
event loops. Start with the peer table; integration support comes from M1 and M4.

---

Role M3 Distributed storage and metadata
Your job is to know what files exist, which chunks belong to each file, and where verified
copies live. You decide what must be stored. M4 performs the network movement
requested through IChunkFetcher.

Files and classes you own
libs/hashing/ wraps the selected SHA-256 library. libs/filestore/ contains FileStore,
FileManifest, ChunkKey and placement logic. apps/chunk_tool/ is your local
demonstration. FileStore implements IChunkStore.

Work to implement
1. Read files in binary mode and split them into 16 KiB chunks. Preserve the exact length
of the last chunk. Use the complete SHA-256 as a proposed 32-byte file ID and keep
chunk hashes in the manifest.
2. Store chunks by file ID and chunk index, separate from source files and final
downloads. Only advertise chunks after complete verification and successful storage.
3. Maintain a file catalog, immutable manifests and confirmed holder records. Page large
metadata so every message fits the datagram limit.
4. Choose two distinct target holders for each chunk and verify that nobody holds every
chunk after distribution. Distinguish intended holders from confirmed holders.
5. On a new member, share metadata and calculate the new storage assignment. Ask M4
to fetch missing replicas, then update holder information.
6. Keep old good copies until replacement holders confirm verified storage. Never delete
original user files. Report insufficient peers or missing replicas explicitly.

A simple placement rule for the first version
Sort the active peer IDs. For chunk c, select positions (offset + c) modulo N and (offset +
c + 1) modulo N, where N is the peer count and offset is a fixed value derived from the
file ID. Everyone must use the same ordered membership snapshot and calculation.
For N at least 3 and a file of at least 3 chunks, this rotating pair gives distinct holders and
prevents one peer from holding every chunk. Our required 2 MiB files contain 128 chunks.
Validate the rule in tests. This simple scheme may move many chunks when membership
changes; optimizing movement with consistent hashing can come later.

Your first task and independent demonstration
Split a local 2 MiB file, record the hashes, reconstruct it and verify identical bytes. Then
print a placement table for A, B, C and D. Test a short final chunk and corrupt one stored
chunk to show rejection.

What you hand to the team
IChunkStore, chunk_tool, metadata formats, placement tests and the storage report
section. M4 receives manifests and confirmed holder lookups. Choose this role if you
prefer files, data structures and deterministic algorithms.

---

Role M4 Transfers failover and verification
You move chunks, assemble downloads, recover from failed sources and coordinate
build/test automation. Choose this role if you enjoy scheduling and failure analysis. Every
member still tests their own module.

Files and classes you own
libs/filetransfer/ contains FileTransfer and DownloadJob. FileTransfer implements
IChunkFetcher. apps/download_demo/, tests/support/, tests/integration/, scripts/ and the
CI workflow are also yours. Each library owner supplies their own module CMake file.

Work to implement
1. Handle CHUNK_REQUEST, CHUNK_DATA and CHUNK_ERROR, including serving a local
chunk through IChunkStore. Replies must identify the request, file, chunk and block.
2. Reassemble blocks with a received-block bitmap and enforce size limits. A retry or
duplicate block must not append the same bytes twice. Verify completed chunks before
recording progress.
3. Expose one asynchronous fetch operation for both storage replication and user
downloads. Report completion/failure, allow cancellation and return queue-full rather than
blocking.
4. Keep requests active to at least two different remote holders during a multi-source
download. Use bounded work queues and give each source a fair turn.
5. On timeout, missing data, a bad hash or a departed source, reassign unfinished chunks
to another confirmed holder. Keep verified chunks and fail explicitly if no reachable copy
remains.
6. Write chunks at their correct output offsets and verify the complete file hash. Validate
output names and handle disk failures. Coordinate clean builds, test fixtures and CI.

Your first coding task
Make a fake store containing a few known chunks on three simulated peers. Write a
scheduler that requests different chunks from two peers. Remove one source before a
response and show that the scheduler chooses its replica. Add real networking after the
scheduling logic works.

How to prove your module works
Record request ID, source, chunk index, completion and retry reason in logs. Demonstrate
overlapping requests to two sources, kill one source and finish from replicas. Compare the
final hash. Repeat with a corrupt block, a missing chunk, a full queue and an output-write
failure.

What you hand to the team
IChunkFetcher, a reusable chunk-transfer engine, download_demo, test fakes, CI and
reproducible demonstration scripts. M3 determines placement; your engine executes
those transfers. The final report is assembled from all four members' sections.