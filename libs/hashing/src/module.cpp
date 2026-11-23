#include "hashing/sha256.hpp" // ELI5: expose the SHA-256 class and helper functions.

#include <openssl/evp.h> // ELI5: use OpenSSL's tested SHA-256 implementation.

#include <array> // ELI5: hold a fixed-size file reading buffer.
#include <fstream> // ELI5: read file contents from disk.
#include <limits> // ELI5: detect if a file size counter would overflow.
#include <stdexcept> // ELI5: report invalid input and hashing failures.
#include <utility> // ELI5: move ownership of the hidden implementation.

namespace cn::hashing { // ELI5: put all hash code in its namespace.

struct Sha256::Impl { // ELI5: hold the OpenSSL details hidden from the public header.
    EVP_MD_CTX* context{EVP_MD_CTX_new()}; // ELI5: ask OpenSSL for a place to keep the running hash.
    bool finalized{}; // ELI5: remember whether the final answer was already produced.

    Impl() {
        if (context == nullptr || EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) { // ELI5: create the context and choose SHA-256 as its algorithm.
            if (context != nullptr) EVP_MD_CTX_free(context); // ELI5: release the context if setup only partly succeeded.
            context = nullptr; // ELI5: make clear that no usable context remains.
            throw std::runtime_error("could not initialize SHA-256"); // ELI5: tell the caller that hashing could not start.
        }
    }

    ~Impl() {
        if (context != nullptr) EVP_MD_CTX_free(context); // ELI5: release OpenSSL's memory when this object is destroyed.
    }
};

Sha256::Sha256() : impl_(std::make_unique<Impl>()) {} // ELI5: create and own a fresh SHA-256 context.
Sha256::~Sha256() = default; // ELI5: let the hidden implementation free itself.
Sha256::Sha256(Sha256&&) noexcept = default; // ELI5: allow the context ownership to move to another object.
Sha256& Sha256::operator=(Sha256&&) noexcept = default; // ELI5: replace this context with one moved from elsewhere.

void Sha256::update(const void* data, std::size_t size) {
    if (!impl_ || impl_->finalized) throw std::logic_error("SHA-256 context is already finalized"); // ELI5: new bytes cannot be added after finishing or moving away the context.
    if (size != 0 && data == nullptr) throw std::invalid_argument("SHA-256 input is null"); // ELI5: non-empty input must point to real bytes.
    if (size != 0 && EVP_DigestUpdate(impl_->context, data, size) != 1) {
        throw std::runtime_error("could not update SHA-256 digest"); // ELI5: report if OpenSSL could not absorb the new bytes.
    }
}

void Sha256::update(std::string_view data) {
    update(data.data(), data.size()); // ELI5: pass the text's address and length to the raw-byte version.
}

void Sha256::update(const Bytes& data) {
    update(data.data(), data.size()); // ELI5: pass the byte list's address and length to the raw-byte version.
}

Digest256 Sha256::finish() {
    if (!impl_ || impl_->finalized) throw std::logic_error("SHA-256 context is already finalized"); // ELI5: only a live, unfinished context can produce a result.
    Digest256 digest{}; // ELI5: reserve 32 bytes for the final answer.
    unsigned int written = 0; // ELI5: let OpenSSL tell us how many answer bytes it wrote.
    if (EVP_DigestFinal_ex(impl_->context, digest.data(), &written) != 1 || written != digest.size()) {
        throw std::runtime_error("could not finish SHA-256 digest"); // ELI5: report if OpenSSL could not produce exactly 32 bytes.
    }
    impl_->finalized = true; // ELI5: prevent changing or finishing this hash a second time.
    return digest; // ELI5: return the completed checksum.
}

Digest256 sha256(std::string_view data) {
    Sha256 hash; // ELI5: start an empty hash calculation.
    hash.update(data); // ELI5: add all the input text.
    return hash.finish(); // ELI5: return its checksum.
}

Digest256 sha256(const Bytes& data) {
    Sha256 hash; // ELI5: start an empty hash calculation.
    hash.update(data); // ELI5: add all the input bytes.
    return hash.finish(); // ELI5: return their checksum.
}

Digest256 sha256_file(const std::string& path, std::uint64_t* size) {
    std::ifstream input(path, std::ios::binary); // ELI5: open the file without changing its bytes.
    if (!input) throw std::runtime_error("could not open file for hashing: " + path); // ELI5: stop if the file cannot be opened.

    Sha256 hash; // ELI5: begin calculating the file's checksum.
    std::array<char, 64 * 1024> buffer{}; // ELI5: use a reusable 64 KiB reading area.
    std::uint64_t total = 0; // ELI5: count how many bytes are read.
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size())); // ELI5: read the next block of file bytes.
        const auto count = input.gcount(); // ELI5: find how many bytes arrived this time.
        if (count > 0) {
            const auto amount = static_cast<std::uint64_t>(count); // ELI5: convert that count to the saved size type.
            if (amount > std::numeric_limits<std::uint64_t>::max() - total) {
                throw std::overflow_error("file size exceeds uint64 range"); // ELI5: stop if the total would exceed its number range.
            }
            total += amount; // ELI5: add these bytes to the total file size.
            hash.update(buffer.data(), static_cast<std::size_t>(count)); // ELI5: add this block to the running checksum.
        }
    }
    if (!input.eof()) throw std::runtime_error("error while hashing file: " + path); // ELI5: report disk errors that are not normal end-of-file.
    if (size != nullptr) *size = total; // ELI5: write the total size if the caller asked for it.
    return hash.finish(); // ELI5: finish and return the file checksum.
}

std::string to_hex(const Digest256& digest) {
    static constexpr char digits[] = "0123456789abcdef"; // ELI5: use these symbols for base-16 numbers.
    std::string text; // ELI5: prepare readable output text.
    text.resize(digest.size() * 2); // ELI5: each hash byte needs two hex characters.
    for (std::size_t i = 0; i < digest.size(); ++i) {
        text[i * 2] = digits[digest[i] >> 4]; // ELI5: turn the byte's top half into the first character.
        text[i * 2 + 1] = digits[digest[i] & 0x0f]; // ELI5: turn the byte's bottom half into the next character.
    }
    return text; // ELI5: return the 64-character checksum.
}

Digest256 from_hex(std::string_view text) {
    if (text.size() != 64) throw std::invalid_argument("SHA-256 text must contain 64 hex digits"); // ELI5: a 32-byte hash needs exactly 64 hex characters.
    const auto nibble = [](char c) -> int { // ELI5: convert one hex character into a number from 0 to 15.
        if (c >= '0' && c <= '9') return c - '0'; // ELI5: handle the characters zero through nine.
        if (c >= 'a' && c <= 'f') return c - 'a' + 10; // ELI5: handle lowercase a through f.
        if (c >= 'A' && c <= 'F') return c - 'A' + 10; // ELI5: handle uppercase A through F.
        return -1; // ELI5: mark every other character as invalid.
    };

    Digest256 digest{}; // ELI5: reserve the 32 bytes that will hold the decoded hash.
    for (std::size_t i = 0; i < digest.size(); ++i) {
        const int high = nibble(text[i * 2]); // ELI5: decode the first character for this byte.
        const int low = nibble(text[i * 2 + 1]); // ELI5: decode the second character for this byte.
        if (high < 0 || low < 0) throw std::invalid_argument("SHA-256 text contains a non-hex digit"); // ELI5: refuse any pair that contains an invalid symbol.
        digest[i] = static_cast<std::uint8_t>((high << 4) | low); // ELI5: join the two four-bit halves into one byte.
    }
    return digest; // ELI5: return the reconstructed 32-byte hash.
}

} // namespace cn::hashing: SHA-256 helpers are implemented here.
