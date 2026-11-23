# hashing

Owner: M3 (@Byte-sized-Ali).

The module provides streaming SHA-256, file hashing, and lowercase hexadecimal conversion. Its implementation uses OpenSSL's EVP API so hashing large files does not require loading them into memory. The target links `OpenSSL::Crypto`.

Public API: `hashing/sha256.hpp` (`Sha256`, `sha256`, `sha256_file`, `to_hex`, `from_hex`). `Digest256` is a 32-byte shared type from `common/file_types.hpp`.

`filestore::build_manifest` uses the streaming API to calculate the full-file ID and each 16 KiB chunk hash. Chunk reads and rebuilt outputs are checked against these hashes before use. See [storage format](../../docs/storage-format.md), [storage report](../../docs/reports/storage.md) and [team workflow](../../CONTRIBUTING.md).

The registered `hashing_behavior` CTest covers SHA-256 known vectors, incremental updates, binary file hashing, byte counts and hexadecimal validation.
