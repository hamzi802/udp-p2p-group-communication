# UDP P2P Group Communication

Decentralized group chat and distributed file sharing in **C++17 on Linux**, using UDP with reliability implemented by the team. Every peer runs the same application; no central application server or storage is required.

**Current status:** M3's hashing and storage modules now have an implementation and a local `chunk_tool` workflow. M1 transport, M2 membership/chat/peer application and M4 transfer service remain scaffolds, so the end-to-end peer application is not integrated yet. The M3 transport and transfer contracts are declared in `libs/common/` for the owners to review.

## Team

| Role | Member | Owns |
|---|---|---|
| M1 | @Musab-Farooq | common interfaces, reliable UDP, protocol |
| M2 | @hamzi802 | membership, chat, main application and coordination |
| M3 | @Byte-sized-Ali | hashing, file chunks, metadata and replica placement |
| M4 | @hamzayousuf717-ops | transfer scheduler, failover, build/CI and integration evidence |

## Build and run

Requires Linux, a C++17 compiler, CMake 3.16+, OpenSSL Crypto development files and a build tool such as Make or Ninja. GitHub CI uses the CMake/CTest supplied by its runner.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/apps/peer/peer --help
```

If your installed CTest supports it, add `--no-tests=error` so an empty test suite fails. CI enables this. Hashing and filestore have registered behavioral tests; the full peer demo and network acceptance still depend on M1, M2 and M4 implementations.

## M3 local chunk workflow

`chunk_tool` works without the other peers. It stores chunks below the selected data directory and leaves the source file untouched.

```bash
./build/apps/chunk_tool/chunk_tool share sample.bin --data-dir run/m3 --peers A,B,C,D
./build/apps/chunk_tool/chunk_tool catalog --data-dir run/m3
./build/apps/chunk_tool/chunk_tool rebuild <file-id> --data-dir run/m3 --output downloads/sample.bin
```

`share` prints the full file ID, chunk hashes and intended two-holder placement. Its peer labels are a local demonstration only: no remote storage is confirmed by this command. The full storage-service API and the defined M3 message payloads are documented in [storage format](docs/storage-format.md), [protocol](docs/protocol.md) and [storage report](docs/reports/storage.md).

## Networking: what to use

For this Linux C++17 project, use the operating system's **POSIX UDP socket API**. There is no extra networking package to install. M1 wraps the socket calls in `libs/rudp/`; the other modules use that wrapper when the team integrates them. UDP can lose, duplicate or reorder messages, so M1's reliability code handles ACKs, retries and ordering. See the Linux [UDP](https://man7.org/linux/man-pages/man7/udp.7.html) and [socket](https://man7.org/linux/man-pages/man2/socket.2.html) manual pages.

| Header | What it is for | When to use it |
|---|---|---|
| `<sys/socket.h>` | `socket(AF_INET, SOCK_DGRAM, 0)`, `bind`, `sendto`, `recvfrom` | M1's UDP sender/receiver |
| `<netinet/in.h>` | `sockaddr_in` and port conversion with `htons`/`ntohs` | M1's IPv4 address and port |
| `<arpa/inet.h>` | `inet_pton`/`inet_ntop` for text IP addresses | M1 when converting IP text |
| `<unistd.h>` | `close` to release a socket | M1 after using a socket |
| `<poll.h>` | `poll` with a timeout to check when a socket has data | Later, when the event loop is connected |

These are system headers, not GitHub libraries. On Linux with a working C++ compiler, CMake does not need an extra networking `find_package` or library link for this socket API. Start M1's standalone demo on one computer with `127.0.0.1` and two different ports such as `5001` and `5002`; move to LAN IP addresses during integration.

**M2 (Hamza):** Keep working on [peer list and chat #48](https://github.com/hamzi802/udp-p2p-group-communication/issues/48) with `std::vector` and a fake message sender. Your `ip_addr` and `port_no` are data for now; your code does not open a socket yet. At [integration #52](https://github.com/hamzi802/udp-p2p-group-communication/issues/52), connect your chat send/receive functions to M1's transport. M3 uses local C++ file I/O for chunks; M4 tests downloads with fake sources before connecting to the same transport.

## Where to work

| Path | Purpose |
|---|---|
| apps/peer/ | CLI, event loop and connecting the services (M2) |
| apps/rudp_demo/ | Independent transport demonstration (M1) |
| apps/chunk_tool/ | Local chunk/hash demonstration (M3) |
| apps/download_demo/ | Fake-based scheduler then real download demonstration (M4) |
| libs/common/ | Header-only shared types and interfaces (M1 + agreement; includes M3 contracts) |
| libs/rudp/ | Socket, codec and reliable transport (M1) |
| libs/membership/, libs/chat/ | Group membership and chat (M2) |
| libs/hashing/, libs/filestore/ | Integrity, storage, manifests and placement (M3) |
| libs/filetransfer/ | Chunk requests, downloads and failover (M4) |
| tests/support/, tests/integration/ | Shared fakes and system scenarios (M4 coordinates) |
| docs/, scripts/ | Design, module reports, planning and reproducible checks |

Every compiled library has public `include/<module>/`, `src/`, `tests/` and a CMake target. Keep generated `build/` and all local runtime data out of Git.

## Start here

1. Open the [six-task project plan](docs/project-plan.md) and find your one module issue.
2. Build your own module and standalone demo. Use fake peers or fake messages; you do not need another member's code yet.
3. After all four demos work, connect them in the integration issue.
4. Follow [contribution and review steps](CONTRIBUTING.md).

See [protocol draft](docs/protocol.md), [storage draft](docs/storage-format.md), [test/demo plan](docs/testing.md) and [setup status](docs/setup-status.md).

The three active milestone dates are proposed internal targets, not confirmed instructor deadlines. The earlier 44 detailed planning issues are closed and kept only as technical reference. The current M3 scope is active issue #51 plus its later storage contributions to integration and final demonstration; the archived Ixx rows are design breakdowns, not separate active tickets.

## Working on a GitHub issue

Issues live on GitHub; `git clone` and `git fetch` download code and branches, not the issue descriptions. Open the [six active issues](https://github.com/hamzi802/udp-p2p-group-communication/issues), choose the one assigned to you, and read its first step. Use its real issue number in your branch and pull request.

**First time on your computer:** clone the repository and enter its folder.

```bash
git clone https://github.com/hamzi802/udp-p2p-group-communication.git
cd udp-p2p-group-communication
```

**Before each new issue:** get the latest code and make a separate branch. For example, M2 starts issue #48:

```bash
git switch main
git pull --ff-only origin main
git switch -c feature/48-peer-chat
```

Work on the files named in the issue. Build and test using the commands above, then commit and publish your branch:

```bash
git status
git add path/to/changed-file
git commit -m "Add peer list and chat demo (#48)"
git push -u origin feature/48-peer-chat
```

On GitHub, open a pull request from your branch into `main`. Describe what you changed, include your demo/test results and write `Closes #48` (or your own issue number) in the PR description. Request the reviewers listed in [CONTRIBUTING.md](CONTRIBUTING.md), respond to feedback, and merge after human approval and passing checks. Comment on your issue if you are blocked.

**Already cloned?** From the repository folder, run `git fetch origin` to download new remote branches and commits without changing your working files. Run `git pull --ff-only origin main` while on `main` to update that branch before starting the next issue. Avoid committing directly to `main`.
