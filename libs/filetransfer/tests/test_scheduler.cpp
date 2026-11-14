#include "filetransfer/chunk_scheduler.hpp"

#include <iostream>

using namespace cn::filetransfer;

static int failures = 0;
#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) {                                                \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; \
            ++failures;                                               \
        }                                                             \
    } while (0)

static void twoSourcesGetDifferentChunks() {
    ChunkScheduler s({{"A", "B"}, {"A", "B"}, {"A", "B"}, {"A", "B"}});
    auto first = s.next();
    auto second = s.next();
    CHECK(first && second);
    CHECK(first->chunk != second->chunk);
    CHECK(first->source != second->source);
}

static void failoverUsesReplica() {
    ChunkScheduler s({{"A", "B"}, {"A", "B"}});
    auto a1 = s.next();  // chunk 0 -> A
    auto a2 = s.next();  // chunk 1 -> B
    CHECK(a1 && a2 && a2->source == "B");
    s.markSourceDown("B");
    auto retry = s.next();  // chunk 1 must move to A
    CHECK(retry && retry->chunk == 1 && retry->source == "A");
    CHECK(!s.failed());
}

static void allHoldersGoneFails() {
    ChunkScheduler s({{"A"}});
    CHECK(s.next().has_value());
    s.markSourceDown("A");
    CHECK(!s.next().has_value());
    CHECK(s.failed());
}

static void finishesWhenAllDone() {
    ChunkScheduler s({{"A", "B"}, {"A", "B"}});
    auto a = s.next();
    auto b = s.next();
    CHECK(a && b);
    s.markDone(a->chunk);
    CHECK(!s.finished());
    s.markDone(b->chunk);
    CHECK(s.finished());
}

int main() {
    twoSourcesGetDifferentChunks();
    failoverUsesReplica();
    allHoldersGoneFails();
    finishesWhenAllDone();
    if (failures == 0) std::cout << "scheduler tests passed\n";
    return failures == 0 ? 0 : 1;
}
