#include "filetransfer/block_assembler.hpp"

#include <iostream>
#include <vector>

using namespace cn::filetransfer;

static int failures = 0;
#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) {                                                \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; \
            ++failures;                                               \
        }                                                             \
    } while (0)

using Bytes = std::vector<std::uint8_t>;

static Bytes block(std::size_t size, std::uint8_t fill) {
    return Bytes(size, fill);
}

static void outOfOrderBlocksRebuildInOrder() {
    BlockAssembler a;
    CHECK(a.addBlock(2, 3, block(1024, 'c')) == BlockResult::Accepted);
    CHECK(a.addBlock(0, 3, block(1024, 'a')) == BlockResult::Accepted);
    CHECK(!a.complete());
    CHECK(a.addBlock(1, 3, block(1024, 'b')) == BlockResult::Accepted);
    CHECK(a.complete());
    Bytes d = a.data();
    CHECK(d.size() == 3072);
    CHECK(d[0] == 'a' && d[1024] == 'b' && d[2048] == 'c');
}

static void duplicateBlockIsIgnored() {
    BlockAssembler a;
    CHECK(a.addBlock(0, 2, block(1024, 'a')) == BlockResult::Accepted);
    CHECK(a.addBlock(0, 2, block(1024, 'x')) == BlockResult::Duplicate);
    CHECK(a.received() == 1);
    CHECK(a.addBlock(1, 2, block(10, 'b')) == BlockResult::Accepted);
    Bytes d = a.data();
    CHECK(d.size() == 1034);
    CHECK(d[0] == 'a');  // the retry did not overwrite or append
}

static void badIndexRejected() {
    BlockAssembler a;
    CHECK(a.addBlock(3, 3, block(1024, 'a')) == BlockResult::BadIndex);
    CHECK(a.received() == 0);
}

static void badSizesRejected() {
    BlockAssembler a;
    CHECK(a.addBlock(0, 2, block(1025, 'a')) == BlockResult::BadSize);  // too big
    CHECK(a.addBlock(0, 2, block(0, 'a')) == BlockResult::BadSize);     // empty
    CHECK(a.addBlock(0, 2, block(500, 'a')) == BlockResult::BadSize);   // short, not last
    CHECK(a.received() == 0);
}

static void badBlockCountRejected() {
    BlockAssembler a;
    CHECK(a.addBlock(0, 0, block(1024, 'a')) == BlockResult::BadBlockCount);
    CHECK(a.addBlock(0, 17, block(1024, 'a')) == BlockResult::BadBlockCount);
    CHECK(a.addBlock(0, 4, block(1024, 'a')) == BlockResult::Accepted);
    CHECK(a.addBlock(1, 5, block(1024, 'b')) == BlockResult::BadBlockCount);  // changed
}

static void shortLastBlockIsAllowed() {
    BlockAssembler a;
    CHECK(a.addBlock(0, 2, block(1024, 'a')) == BlockResult::Accepted);
    CHECK(a.addBlock(1, 2, block(100, 'b')) == BlockResult::Accepted);
    CHECK(a.complete());
    CHECK(a.data().size() == 1124);
}

static void fullChunkIsSixteenKiB() {
    BlockAssembler a;
    for (std::uint16_t i = 0; i < 16; ++i) {
        CHECK(a.addBlock(i, 16, block(1024, static_cast<std::uint8_t>(i))) ==
              BlockResult::Accepted);
    }
    CHECK(a.complete());
    CHECK(a.data().size() == 16384);
}

int main() {
    outOfOrderBlocksRebuildInOrder();
    duplicateBlockIsIgnored();
    badIndexRejected();
    badSizesRejected();
    badBlockCountRejected();
    shortLastBlockIsAllowed();
    fullChunkIsSixteenKiB();
    if (failures == 0) std::cout << "block assembler tests passed\n";
    return failures == 0 ? 0 : 1;
}
