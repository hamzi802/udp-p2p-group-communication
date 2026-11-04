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

If your installed CTest supports it, add `--no-tests=error` so an empty test suite fails. CI enables this. The initial C++ checks validate only the executable scaffold; when Python 3 is available CTest also checks planning sync behavior. Each member must add feature tests.

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

1. Read [architecture](docs/architecture.md) and your [role sheet](docs/roles.md).
2. Choose your first assigned [issue](https://github.com/hamzi802/udp-p2p-group-communication/issues) from the [project plan](docs/project-plan.md).
3. Agree shared interfaces before implementing features; use fakes to work independently.
4. Follow [contribution and review steps](CONTRIBUTING.md).

See [protocol draft](docs/protocol.md), [storage draft](docs/storage-format.md), [test/demo plan](docs/testing.md) and [setup status](docs/setup-status.md).

The five November 2026 milestone dates are proposed internal targets, not confirmed instructor deadlines.
