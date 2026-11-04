# Project plan and assigned issues

The dates below are **proposed internal targets in November 2026, end of day Asia/Karachi**, not confirmed instructor deadlines. Proposal IDs (I01–I44) and GitHub issue numbers differ; use the actual links.

## MS1 — Project foundation — November 4

Agree shared interfaces, protocol and metadata; establish C++17/CMake layout, CI and review workflow.

| Proposal | GitHub task | Owner | Dependencies |
|---|---|---|---|
| I01 | [#1: Repository folders and root CMake configuration](https://github.com/hamzi802/udp-p2p-group-communication/issues/1) | M4: @hamzayousuf717-ops | — |
| I02 | [#2: Shared C++ interfaces and common types](https://github.com/hamzi802/udp-p2p-group-communication/issues/2) | M1: @Musab-Farooq | [#1](https://github.com/hamzi802/udp-p2p-group-communication/issues/1) |
| I03 | [#3: Protocol envelope and operation sequences](https://github.com/hamzi802/udp-p2p-group-communication/issues/3) | M1: @Musab-Farooq | [#2](https://github.com/hamzi802/udp-p2p-group-communication/issues/2) |
| I04 | [#4: Manifest and replica metadata specification](https://github.com/hamzi802/udp-p2p-group-communication/issues/4) | M3: @Byte-sized-Ali | [#2](https://github.com/hamzi802/udp-p2p-group-communication/issues/2) |
| I05 | [#5: Project board, labels, milestones and issue/PR templates](https://github.com/hamzi802/udp-p2p-group-communication/issues/5) | M2: @hamzi802 | — |
| I06 | [#6: M1/M2 code owners, Copilot and main branch rules](https://github.com/hamzi802/udp-p2p-group-communication/issues/6) | M2: @hamzi802 | [#5](https://github.com/hamzi802/udp-p2p-group-communication/issues/5) |
| I07 | [#7: Initial CMake build and CTest CI workflow](https://github.com/hamzi802/udp-p2p-group-communication/issues/7) | M4: @hamzayousuf717-ops | [#1](https://github.com/hamzi802/udp-p2p-group-communication/issues/1) |

## MS2 — Separate module demonstrations — November 8

Each role demonstrates its module independently with real transport or agreed fakes.

| Proposal | GitHub task | Owner | Dependencies |
|---|---|---|---|
| I08 | [#8: Packet encoding, decoding and validation](https://github.com/hamzi802/udp-p2p-group-communication/issues/8) | M1: @Musab-Farooq | [#2](https://github.com/hamzi802/udp-p2p-group-communication/issues/2), [#3](https://github.com/hamzi802/udp-p2p-group-communication/issues/3) |
| I09 | [#9: UDP socket wrapper and two-process demo](https://github.com/hamzi802/udp-p2p-group-communication/issues/9) | M1: @Musab-Farooq | [#8](https://github.com/hamzi802/udp-p2p-group-communication/issues/8) |
| I10 | [#10: Peer table and /peers command](https://github.com/hamzi802/udp-p2p-group-communication/issues/10) | M2: @hamzi802 | [#2](https://github.com/hamzi802/udp-p2p-group-communication/issues/2) |
| I11 | [#11: Bootstrap JOIN, WELCOME and HELLO](https://github.com/hamzi802/udp-p2p-group-communication/issues/11) | M2: @hamzi802 | [#3](https://github.com/hamzi802/udp-p2p-group-communication/issues/3), [#10](https://github.com/hamzi802/udp-p2p-group-communication/issues/10), [#14](https://github.com/hamzi802/udp-p2p-group-communication/issues/14) |
| I12 | [#12: Fixed-size chunking and SHA-256 hashing](https://github.com/hamzi802/udp-p2p-group-communication/issues/12) | M3: @Byte-sized-Ali | [#4](https://github.com/hamzi802/udp-p2p-group-communication/issues/4) |
| I13 | [#13: Local chunk store and manifest generation](https://github.com/hamzi802/udp-p2p-group-communication/issues/13) | M3: @Byte-sized-Ali | [#12](https://github.com/hamzi802/udp-p2p-group-communication/issues/12), [#4](https://github.com/hamzi802/udp-p2p-group-communication/issues/4) |
| I14 | [#14: Fake transport, peer directory, store and clock](https://github.com/hamzi802/udp-p2p-group-communication/issues/14) | M4: @hamzayousuf717-ops | [#2](https://github.com/hamzi802/udp-p2p-group-communication/issues/2), [#4](https://github.com/hamzi802/udp-p2p-group-communication/issues/4) |
| I15 | [#15: Chunk scheduler demonstration using fakes](https://github.com/hamzi802/udp-p2p-group-communication/issues/15) | M4: @hamzayousuf717-ops | [#14](https://github.com/hamzi802/udp-p2p-group-communication/issues/14), [#4](https://github.com/hamzi802/udp-p2p-group-communication/issues/4) |

## MS3 — Reliable communication — November 15

Four-peer membership/chat, reliable UDP and basic chunk transfers work under loss and reordering.

| Proposal | GitHub task | Owner | Dependencies |
|---|---|---|---|
| I16 | [#17: ACK tracking, retries, backoff and failure reporting](https://github.com/hamzi802/udp-p2p-group-communication/issues/17) | M1: @Musab-Farooq | [#8](https://github.com/hamzi802/udp-p2p-group-communication/issues/8), [#9](https://github.com/hamzi802/udp-p2p-group-communication/issues/9) |
| I17 | [#18: Sequence ordering and duplicate suppression](https://github.com/hamzi802/udp-p2p-group-communication/issues/18) | M1: @Musab-Farooq | [#17](https://github.com/hamzi802/udp-p2p-group-communication/issues/17) |
| I18 | [#19: Separate streams, bounded queues and receive window](https://github.com/hamzi802/udp-p2p-group-communication/issues/19) | M1: @Musab-Farooq | [#17](https://github.com/hamzi802/udp-p2p-group-communication/issues/17), [#18](https://github.com/hamzi802/udp-p2p-group-communication/issues/18) |
| I19 | [#20: Heartbeat, LEAVE and crash detection](https://github.com/hamzi802/udp-p2p-group-communication/issues/20) | M2: @hamzi802 | [#11](https://github.com/hamzi802/udp-p2p-group-communication/issues/11), [#14](https://github.com/hamzi802/udp-p2p-group-communication/issues/14) |
| I20 | [#21: Ordered four-peer group chat](https://github.com/hamzi802/udp-p2p-group-communication/issues/21) | M2: @hamzi802 | [#11](https://github.com/hamzi802/udp-p2p-group-communication/issues/11), [#18](https://github.com/hamzi802/udp-p2p-group-communication/issues/18), [#20](https://github.com/hamzi802/udp-p2p-group-communication/issues/20) |
| I21 | [#22: Two-holder placement and storage invariant checks](https://github.com/hamzi802/udp-p2p-group-communication/issues/22) | M3: @Byte-sized-Ali | [#13](https://github.com/hamzi802/udp-p2p-group-communication/issues/13), [#10](https://github.com/hamzi802/udp-p2p-group-communication/issues/10) |
| I22 | [#23: Chunk request/data/error and block reassembly](https://github.com/hamzi802/udp-p2p-group-communication/issues/23) | M4: @hamzayousuf717-ops | [#15](https://github.com/hamzi802/udp-p2p-group-communication/issues/15), [#13](https://github.com/hamzi802/udp-p2p-group-communication/issues/13), [#17](https://github.com/hamzi802/udp-p2p-group-communication/issues/17) |
| I23 | [#24: Real UDP loss, duplicate and reorder tests](https://github.com/hamzi802/udp-p2p-group-communication/issues/24) | M4: @hamzayousuf717-ops | [#18](https://github.com/hamzi802/udp-p2p-group-communication/issues/18), [#19](https://github.com/hamzi802/udp-p2p-group-communication/issues/19), [#21](https://github.com/hamzi802/udp-p2p-group-communication/issues/21), [#23](https://github.com/hamzi802/udp-p2p-group-communication/issues/23) |

## MS4 — Complete distributed file sharing — November 22

Verified distributed storage, replicated metadata, joining storage and multi-source download failover work.

| Proposal | GitHub task | Owner | Dependencies |
|---|---|---|---|
| I24 | [#25: Initial chunk distribution and storage confirmation](https://github.com/hamzi802/udp-p2p-group-communication/issues/25) | M3: @Byte-sized-Ali | [#22](https://github.com/hamzi802/udp-p2p-group-communication/issues/22), [#23](https://github.com/hamzi802/udp-p2p-group-communication/issues/23) |
| I25 | [#26: Replicated catalog, manifests and holder records](https://github.com/hamzi802/udp-p2p-group-communication/issues/26) | M3: @Byte-sized-Ali | [#25](https://github.com/hamzi802/udp-p2p-group-communication/issues/25), [#4](https://github.com/hamzi802/udp-p2p-group-communication/issues/4) |
| I26 | [#27: Joining-peer metadata and storage handoff](https://github.com/hamzi802/udp-p2p-group-communication/issues/27) | M3: @Byte-sized-Ali | [#26](https://github.com/hamzi802/udp-p2p-group-communication/issues/26), [#11](https://github.com/hamzi802/udp-p2p-group-communication/issues/11) |
| I27 | [#28: Parallel download using two remote sources](https://github.com/hamzi802/udp-p2p-group-communication/issues/28) | M4: @hamzayousuf717-ops | [#23](https://github.com/hamzi802/udp-p2p-group-communication/issues/23), [#26](https://github.com/hamzi802/udp-p2p-group-communication/issues/26) |
| I28 | [#29: Source failover retaining verified progress](https://github.com/hamzi802/udp-p2p-group-communication/issues/29) | M4: @hamzayousuf717-ops | [#28](https://github.com/hamzi802/udp-p2p-group-communication/issues/28), [#20](https://github.com/hamzi802/udp-p2p-group-communication/issues/20) |
| I29 | [#30: Per-chunk and complete-file hash verification](https://github.com/hamzi802/udp-p2p-group-communication/issues/30) | M4: @hamzayousuf717-ops | [#28](https://github.com/hamzi802/udp-p2p-group-communication/issues/28), [#12](https://github.com/hamzi802/udp-p2p-group-communication/issues/12) |
| I30 | [#31: File commands and responsive chat during transfers](https://github.com/hamzi802/udp-p2p-group-communication/issues/31) | M2: @hamzi802 | [#21](https://github.com/hamzi802/udp-p2p-group-communication/issues/21), [#26](https://github.com/hamzi802/udp-p2p-group-communication/issues/26), [#28](https://github.com/hamzi802/udp-p2p-group-communication/issues/28), [#29](https://github.com/hamzi802/udp-p2p-group-communication/issues/29), [#30](https://github.com/hamzi802/udp-p2p-group-communication/issues/30) |
| I31 | [#32: Restart, stale-packet and queue-failure handling](https://github.com/hamzi802/udp-p2p-group-communication/issues/32) | M1: @Musab-Farooq | [#19](https://github.com/hamzi802/udp-p2p-group-communication/issues/19), [#11](https://github.com/hamzi802/udp-p2p-group-communication/issues/11), [#23](https://github.com/hamzi802/udp-p2p-group-communication/issues/23) |

## MS5 — Final verification and demonstration — November 29

Four-computer/eight-file and fifth-peer demos, failure tests, report and final release review. I44 is optional.

| Proposal | GitHub task | Owner | Dependencies |
|---|---|---|---|
| I32 | [#33: Four-computer eight-file demonstration](https://github.com/hamzi802/udp-p2p-group-communication/issues/33) | M4: @hamzayousuf717-ops | [#25](https://github.com/hamzi802/udp-p2p-group-communication/issues/25), [#31](https://github.com/hamzi802/udp-p2p-group-communication/issues/31) |
| I33 | [#34: Bootstrap departure continuity test](https://github.com/hamzi802/udp-p2p-group-communication/issues/34) | M2: @hamzi802 | [#21](https://github.com/hamzi802/udp-p2p-group-communication/issues/21), [#20](https://github.com/hamzi802/udp-p2p-group-communication/issues/20) |
| I34 | [#35: Fifth-peer joining and storage acquisition test](https://github.com/hamzi802/udp-p2p-group-communication/issues/35) | M2: @hamzi802 | [#27](https://github.com/hamzi802/udp-p2p-group-communication/issues/27), [#34](https://github.com/hamzi802/udp-p2p-group-communication/issues/34) |
| I35 | [#36: Replica placement and publisher-departure tests](https://github.com/hamzi802/udp-p2p-group-communication/issues/36) | M3: @Byte-sized-Ali | [#26](https://github.com/hamzi802/udp-p2p-group-communication/issues/26), [#27](https://github.com/hamzi802/udp-p2p-group-communication/issues/27) |
| I36 | [#37: Two-source download with a crashed source](https://github.com/hamzi802/udp-p2p-group-communication/issues/37) | M4: @hamzayousuf717-ops | [#29](https://github.com/hamzi802/udp-p2p-group-communication/issues/29), [#30](https://github.com/hamzi802/udp-p2p-group-communication/issues/30) |
| I37 | [#38: Full loss, corruption and failure test suite](https://github.com/hamzi802/udp-p2p-group-communication/issues/38) | M4: @hamzayousuf717-ops | [#24](https://github.com/hamzi802/udp-p2p-group-communication/issues/24), [#32](https://github.com/hamzi802/udp-p2p-group-communication/issues/32), [#36](https://github.com/hamzi802/udp-p2p-group-communication/issues/36), [#37](https://github.com/hamzi802/udp-p2p-group-communication/issues/37) |
| I38 | [#39: Clean-clone build and registered-test verification](https://github.com/hamzi802/udp-p2p-group-communication/issues/39) | M4: @hamzayousuf717-ops | [#7](https://github.com/hamzi802/udp-p2p-group-communication/issues/7), [#38](https://github.com/hamzi802/udp-p2p-group-communication/issues/38) |
| I39 | [#40: Transport and protocol report section](https://github.com/hamzi802/udp-p2p-group-communication/issues/40) | M1: @Musab-Farooq | [#32](https://github.com/hamzi802/udp-p2p-group-communication/issues/32) |
| I40 | [#41: Membership and chat report section](https://github.com/hamzi802/udp-p2p-group-communication/issues/41) | M2: @hamzi802 | [#31](https://github.com/hamzi802/udp-p2p-group-communication/issues/31), [#34](https://github.com/hamzi802/udp-p2p-group-communication/issues/34), [#35](https://github.com/hamzi802/udp-p2p-group-communication/issues/35) |
| I41 | [#42: Storage and placement report section](https://github.com/hamzi802/udp-p2p-group-communication/issues/42) | M3: @Byte-sized-Ali | [#36](https://github.com/hamzi802/udp-p2p-group-communication/issues/36) |
| I42 | [#43: Transfer report, final assembly and demo guide](https://github.com/hamzi802/udp-p2p-group-communication/issues/43) | M4: @hamzayousuf717-ops | [#37](https://github.com/hamzi802/udp-p2p-group-communication/issues/37), [#39](https://github.com/hamzi802/udp-p2p-group-communication/issues/39), [#40](https://github.com/hamzi802/udp-p2p-group-communication/issues/40), [#41](https://github.com/hamzi802/udp-p2p-group-communication/issues/41), [#42](https://github.com/hamzi802/udp-p2p-group-communication/issues/42) |
| I43 | [#44: Final integration review and release tag](https://github.com/hamzi802/udp-p2p-group-communication/issues/44) | M1: @Musab-Farooq | [#33](https://github.com/hamzi802/udp-p2p-group-communication/issues/33), [#35](https://github.com/hamzi802/udp-p2p-group-communication/issues/35), [#36](https://github.com/hamzi802/udp-p2p-group-communication/issues/36), [#37](https://github.com/hamzi802/udp-p2p-group-communication/issues/37), [#39](https://github.com/hamzi802/udp-p2p-group-communication/issues/39), [#43](https://github.com/hamzi802/udp-p2p-group-communication/issues/43) |
| I44 (optional) | [#45: Optional: restore lost chunk replicas](https://github.com/hamzi802/udp-p2p-group-communication/issues/45) | M3: @Byte-sized-Ali | [#29](https://github.com/hamzi802/udp-p2p-group-communication/issues/29), [#36](https://github.com/hamzi802/udp-p2p-group-communication/issues/36) |

Each issue has concrete acceptance checks. Do not close feature issues merely because a folder or empty class exists. I44 is optional. M4 owns 14 tasks because integration/CI/report coordination crosses modules; everyone remains responsible for their own tests and report section.
