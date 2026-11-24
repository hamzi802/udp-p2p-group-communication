#include "filetransfer/block_assembler.hpp"

namespace cn::filetransfer {

BlockResult BlockAssembler::addBlock(std::uint16_t index, std::uint16_t blockCount,
                                     const std::vector<std::uint8_t>& bytes) {
    const auto count = static_cast<std::size_t>(blockCount);
    const auto idx = static_cast<std::size_t>(index);

    // Validate everything before changing any state.
    if (count == 0 || count > kMaxBlocks) return BlockResult::BadBlockCount;
    if (blockCount_ != 0 && count != blockCount_) return BlockResult::BadBlockCount;
    if (idx >= count) return BlockResult::BadIndex;
    if (bytes.empty() || bytes.size() > kMaxBlockSize) return BlockResult::BadSize;
    // Only the last block of a chunk may be shorter than 1024 bytes.
    if (idx + 1 < count && bytes.size() != kMaxBlockSize) return BlockResult::BadSize;

    if (blockCount_ == 0) {  // first valid block fixes the chunk layout
        blockCount_ = count;
        have_.assign(count, false);
        blocks_.assign(count, std::vector<std::uint8_t>());
    }

    if (have_[idx]) return BlockResult::Duplicate;  // a retry: ignore it

    blocks_[idx] = bytes;
    have_[idx] = true;
    ++received_;
    return BlockResult::Accepted;
}

bool BlockAssembler::complete() const {
    return blockCount_ != 0 && received_ == blockCount_;
}

std::vector<std::uint8_t> BlockAssembler::data() const {
    std::vector<std::uint8_t> out;
    if (!complete()) return out;
    for (const auto& b : blocks_) {
        out.insert(out.end(), b.begin(), b.end());  // index order, not arrival order
    }
    return out;
}

}  // namespace cn::filetransfer
