#pragma once // ELI5: prevent this header from being included twice.

#include "common/file_types.hpp" // ELI5: reuse the shared byte and hash types.

#include <cstddef> // ELI5: use standard sizes for byte counts.
#include <memory> // ELI5: keep the hidden implementation alive safely.
#include <string> // ELI5: accept and return ordinary text strings.
#include <string_view> // ELI5: read text without copying it first.

namespace cn::hashing { // ELI5: group the project's hashing tools together.

class Sha256 final { // ELI5: calculate a hash gradually as data arrives.
public:
    Sha256(); // ELI5: prepare a fresh hash calculation.
    ~Sha256(); // ELI5: release the hidden hash-calculation data.

    Sha256(Sha256&&) noexcept; // ELI5: let another object take over this calculation.
    Sha256& operator=(Sha256&&) noexcept; // ELI5: replace this calculation with one moved from elsewhere.
    Sha256(const Sha256&) = delete; // ELI5: forbid copying the hidden calculation state.
    Sha256& operator=(const Sha256&) = delete; // ELI5: forbid copying over another hash calculation.

    void update(const void* data, std::size_t size); // ELI5: add these raw bytes to the calculation.
    void update(std::string_view data); // ELI5: add these text characters as bytes.
    void update(const Bytes& data); // ELI5: add all bytes from this list.
    Digest256 finish(); // ELI5: finish the calculation and return its 32-byte answer.

private:
    struct Impl; // ELI5: promise there is a private implementation defined in the source file.
    std::unique_ptr<Impl> impl_; // ELI5: own that hidden implementation without exposing its details here.
};

Digest256 sha256(std::string_view data); // ELI5: hash text in one call.
Digest256 sha256(const Bytes& data); // ELI5: hash a byte list in one call.
Digest256 sha256_file(const std::string& path, std::uint64_t* size = nullptr); // ELI5: hash a file from disk and optionally report its byte size.
std::string to_hex(const Digest256& digest); // ELI5: write the 32 hash bytes as easy-to-read hexadecimal text.
Digest256 from_hex(std::string_view text); // ELI5: turn 64 hexadecimal characters back into 32 hash bytes.

} // namespace cn::hashing: the public hashing tools are declared.
