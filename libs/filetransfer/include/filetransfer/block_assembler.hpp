#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cn::filetransfer {

enum class BlockResult {
    Accepted,       // new block stored
    Duplicate,      // already had it, ignored
    BadIndex,       // index >= block count
    BadSize,        // block too large, or wrong size for its position
    BadBlockCount,  // block count is zero, too large or changed mid-chunk
};

// Collects the numbered blocks of one chunk and rebuilds the chunk bytes.
class BlockAssembler {
public:
    static constexpr std::size_t kMaxBlockSize = 1024;
    static constexpr std::size_t kMaxBlocks = 16;  // 16 x 1024 = 16 KiB

    BlockResult addBlock(std::uint16_t index, std::uint16_t blockCount,
                         const std::vector<std::uint8_t>& bytes);

    bool complete() const;
    std::size_t received() const { return received_; }
    std::size_t expected() const { return blockCount_; }

    // The rebuilt chunk. Only meaningful when complete() is true.
    std::vector<std::uint8_t> data() const;

private:
    std::size_t blockCount_ = 0;  // 0 means "not known yet"
    std::size_t received_ = 0;
    std::vector<bool> have_;
    std::vector<std::vector<std::uint8_t>> blocks_;
};

}  // namespace cn::filetransfer
