# Verification and demonstration plan

Hashing and filestore have registered behavioral tests. A project-wide WSL Debug/Ninja run built every configured target and passed all five CTest cases (hashing behavior, filestore behavior and three peer CLI scaffold checks). The latest isolated M3 run rebuilt only `cn_hashing_tests`, `cn_filestore_tests` and `chunk_tool`; both M3 CTest cases passed. `filestore_behavior` includes bounded payload checks, out-of-order atomic assembly of an 8,193-chunk HAVE snapshot, explicit insufficient-peer/unavailable-replica events, a fake-network 2 MiB distribution across four stores, a fifth peer joining, queue-full retention until replacement confirmation, and rebalancing after the publisher leaves. These are M3 logic checks through fakes, not real UDP acceptance.

The standalone `chunk_tool` workflow also passed on a 2 MiB zero-filled input: it reported 128 locally verified chunks, rebuilt the output, and `cmp` confirmed byte-for-byte equality. After one managed chunk was truncated to simulate corruption, `rebuild` exited with an error and did not create the output path. Real peer acceptance evidence still requires M1, M2 and M4 to implement and connect their modules.

## Reproduce the isolated M3 checks

From the repository root in WSL, build only the M3 targets and run only their tests:

```bash
cmake --build build --target cn_hashing_tests cn_filestore_tests chunk_tool --parallel
ctest --test-dir build --output-on-failure -R behavior
```

The recorded zero-filled CLI fixture produced file ID `5647f05ec18958947d32874eeb788fa396a05d0bab7c1b71f112ceb7e9b31eee` (2,097,152 bytes; 128 chunks). Recreate and verify that local workflow with:

```bash
mkdir -p build/m3_acceptance
dd if=/dev/zero of=build/m3_acceptance/source.bin bs=1048576 count=2 status=none
./build/apps/chunk_tool/chunk_tool share build/m3_acceptance/source.bin --data-dir build/m3_acceptance/store --peers A,B,C,D
./build/apps/chunk_tool/chunk_tool catalog --data-dir build/m3_acceptance/store
./build/apps/chunk_tool/chunk_tool rebuild 5647f05ec18958947d32874eeb788fa396a05d0bab7c1b71f112ceb7e9b31eee --data-dir build/m3_acceptance/store --output build/m3_acceptance/rebuilt.bin
cmp build/m3_acceptance/source.bin build/m3_acceptance/rebuilt.bin
```

The recorded `cmp` succeeded. Corrupt a managed test chunk and confirm the output is not published:

```bash
truncate -s 1 build/m3_acceptance/store/chunk_store/chunks/5647f05ec18958947d32874eeb788fa396a05d0bab7c1b71f112ceb7e9b31eee/0.chunk
./build/apps/chunk_tool/chunk_tool rebuild 5647f05ec18958947d32874eeb788fa396a05d0bab7c1b71f112ceb7e9b31eee --data-dir build/m3_acceptance/store --output build/m3_acceptance/corrupt-output.bin
test ! -e build/m3_acceptance/corrupt-output.bin
```

The corrupted rebuild exited with status 1 and the output path remained absent. The supplied workspace has no `.git` metadata, so this local evidence cannot be associated with a commit SHA until it is applied in the Git checkout.

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

## M3 local acceptance walkthrough

Use a binary input of at least 2 MiB and a fresh ignored data directory. The following commands are the local M3 demo; they do not claim remote placement confirmations:

```bash
./build/apps/chunk_tool/chunk_tool share sample.bin --data-dir run/m3 --peers A,B,C,D
./build/apps/chunk_tool/chunk_tool catalog --data-dir run/m3
./build/apps/chunk_tool/chunk_tool rebuild <file-id> --data-dir run/m3 --output downloads/sample-rebuilt.bin
```

Record the printed file ID and per-chunk hashes. Compare the source and rebuilt files byte for byte and compare their SHA-256. Repeat with a size that is not divisible by 16 KiB to check the exact final-chunk length. To check corruption rejection, alter a stored `.chunk` file in a disposable data directory and confirm `rebuild` fails before publishing an output. The command protects an existing output unless `--force` is passed.

For peer integration, M2 must dispatch M3 messages and notify M3 of membership changes; M4 must implement `IChunkFetcher` and use M3's `IChunkStore`. Then record each holder's HAVE generation and show old chunks are retained until replacement holders confirm. Do not mark these network scenarios complete from the local demo alone.

First test isolated processes on one machine with separate ports and data directories; then repeat over a reachable LAN. Target CLI flags (--name, --port, --data-dir, --bootstrap) are to be implemented under M2's issues, not available in the skeleton.

For every scenario record: actual command, deterministic injection seed, expected result, actual logs/hash output, commit SHA and owner. Each member writes their own module tests and report section; M4 coordinates system evidence.

Optional I44 restores replication after a crash. Mandatory failover must work first and may operate with a degraded replica count.
