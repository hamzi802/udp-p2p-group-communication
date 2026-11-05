# UDP P2P Group Communication

Decentralized group chat and distributed file sharing in **C++17 on Linux**, using UDP with reliability implemented by the team. Every peer runs the same application; no central application server or storage is required.

**Current status:** buildable project skeleton only. Networking, membership, chat, storage and transfers are assigned work, not implemented features. The demo programs currently show help/version information.

## Team

| Role | Member | Owns |
|---|---|---|
| M1 | @Musab-Farooq | common interfaces, reliable UDP, protocol |
| M2 | @hamzi802 | membership, chat, main application and coordination |
| M3 | @Byte-sized-Ali | hashing, file chunks, metadata and replica placement |
| M4 | @hamzayousuf717-ops | transfer scheduler, failover, build/CI and integration evidence |

## Build and run

Requires Linux, a C++17 compiler, CMake 3.16+ and a build tool such as Make or Ninja. GitHub CI uses the CMake/CTest supplied by its runner.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/apps/peer/peer --help
```

If your installed CTest supports it, add `--no-tests=error` so an empty test suite fails. CI enables this. The initial C++ checks validate only the executable scaffold. Each member must add tests for their own feature.

## Where to work

| Path | Purpose |
|---|---|
| apps/peer/ | CLI, event loop and connecting the services (M2) |
| apps/rudp_demo/ | Independent transport demonstration (M1) |
| apps/chunk_tool/ | Local chunk/hash demonstration (M3) |
| apps/download_demo/ | Fake-based scheduler then real download demonstration (M4) |
| libs/common/ | Header-only shared types and interfaces (M1 + agreement) |
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

The three active milestone dates are proposed internal targets, not confirmed instructor deadlines. The earlier 44 detailed planning issues are closed and kept only as technical reference.

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
