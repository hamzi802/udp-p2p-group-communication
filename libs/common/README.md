# common

Owner: M1 (@Musab-Farooq).

`common/` is a header-only target for stable types and service contracts shared by module owners. M3 added `file_types.hpp` and `storage_interfaces.hpp` so the storage service can compile independently of concrete membership, transport and transfer implementations.

The interfaces are: `ITransport` for reliable application messages, `IPeerDirectory` for active peer sessions, `IChunkStore` for verified local chunks, and `IChunkFetcher` for asynchronous M4 transfers. Review changes with M1, M2 and M4 before wiring them into the peer application. Libraries should depend on these declarations rather than concrete peer modules.

Use interfaces from common to avoid circular library dependencies. See [architecture](../../docs/architecture.md) and [team workflow](../../CONTRIBUTING.md).
