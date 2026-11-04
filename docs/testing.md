# Verification and demonstration plan

Initial CTest checks validate the executable scaffold only. Real acceptance evidence remains assigned work.

| Scenario | Required evidence |
|---|---|
| Four-peer membership | All peer lists agree; duplicate joins are safe |
| Bootstrap exits | Surviving peers keep chatting; newcomer contacts a survivor |
| UDP loss/duplication/reordering/lost ACK | Per-sender single ordered delivery or explicit exhausted-retry failure |
| File contribution | Four computers, two files each >= 2 MiB, eight files total |
| Initial distribution | Two distinct confirmed holders for every chunk, no complete managed copy |
| Publisher departure | Catalog/manifests remain available; surviving replicas support retrieval |
| Fifth peer | Dynamic join, metadata sync and verified storage acquisition |
| Multi-source download | Overlapping requests to two different remote peers |
| Source killed during download | Verified progress retained; alternate replica completes remaining work |
| Integrity | Per-chunk and complete-output SHA-256 match |
| Responsiveness | Chat, input and heartbeats continue during transfer |
| Failure cases | Bounded queues, invalid packets/paths, stale sessions, missing copies, disk failure |
| Clean clone | Documented configure/build/test steps succeed without local data |

First test isolated processes on one machine with separate ports and data directories; then repeat over a reachable LAN. Target CLI flags (--name, --port, --data-dir, --bootstrap) are to be implemented under M2's issues, not available in the skeleton.

For every scenario record: actual command, deterministic injection seed, expected result, actual logs/hash output, commit SHA and owner. Each member writes their own module tests and report section; M4 coordinates system evidence.

Optional I44 restores replication after a crash. Mandatory failover must work first and may operate with a degraded replica count.
