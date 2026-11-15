#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "common/build_info.hpp"
#include "filetransfer/chunk_scheduler.hpp"

using cn::filetransfer::Assignment;
using cn::filetransfer::ChunkScheduler;

namespace {

constexpr std::size_t kChunkCount = 8;
constexpr std::size_t kChunkSize = 16;  // tiny chunks for the demo

// A fake peer holding some chunks in memory (no network).
struct FakeSource {
    std::string name;
    std::map<std::uint32_t, std::string> chunks;
    bool alive = true;
};

// Simple checksum so the demo can verify the rebuilt file.
std::uint32_t checksum(const std::string& data) {
    std::uint32_t h = 2166136261u;
    for (unsigned char c : data) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

void printUsage() {
    std::cout << "download_demo: two-source download demo (fake sources, no UDP)\n"
              << "Usage: download_demo [--help | --version | --demo]\n"
              << "  --demo       fetch chunks from sources A and B; B dies halfway\n"
              << "  --demo-fail  every holder of a chunk disappears: explicit failure\n";
}

int runDemo() {
    // 1. Build the original file in memory.
    std::string original;
    for (std::size_t i = 0; i < kChunkCount; ++i) {
        std::string piece(kChunkSize, static_cast<char>('a' + i));
        original += piece;
    }

    // 2. Every chunk has two holders: A and B (and C holds a spare of some).
    std::map<std::string, FakeSource> sources;
    sources["A"].name = "A";
    sources["B"].name = "B";
    std::vector<std::vector<std::string>> holders(kChunkCount, {"A", "B"});
    for (std::size_t i = 0; i < kChunkCount; ++i) {
        std::string piece = original.substr(i * kChunkSize, kChunkSize);
        sources["A"].chunks[static_cast<std::uint32_t>(i)] = piece;
        sources["B"].chunks[static_cast<std::uint32_t>(i)] = piece;
    }

    ChunkScheduler scheduler(holders);
    std::map<std::uint32_t, std::string> received;
    std::set<std::string> usedSources;
    std::size_t completed = 0;

    std::cout << "Downloading " << kChunkCount << " chunks from A and B\n";

    // 3. Issue requests (bounded by the scheduler), then complete them.
    struct Request {
        std::uint64_t id;
        Assignment job;
    };
    std::uint64_t nextRequestId = 1;
    std::vector<Request> inFlight;

    while (!scheduler.finished() && !scheduler.failed()) {
        // Issue as many requests as the scheduler allows.
        while (auto job = scheduler.next()) {
            Request r{nextRequestId++, *job};
            inFlight.push_back(r);
            std::cout << "  [req " << r.id << "] ask " << r.job.source
                      << " for chunk " << r.job.chunk << "\n";
        }
        if (inFlight.empty()) break;  // nothing running and nothing to issue

        // Source B crashes after 3 chunks have been received.
        if (completed == 3 && sources["B"].alive) {
            sources["B"].alive = false;
            scheduler.markSourceDown("B");
            std::cout << "  !! source B went down, switching unfinished work to A\n";
        }

        // Complete the oldest in-flight request.
        Request r = inFlight.front();
        inFlight.erase(inFlight.begin());
        FakeSource& src = sources[r.job.source];
        if (!src.alive || !src.chunks.count(r.job.chunk)) {
            std::cout << "  [req " << r.id << "] FAILED: source " << r.job.source
                      << " unavailable, chunk " << r.job.chunk << " retried elsewhere\n";
            scheduler.retryElsewhere(r.job.chunk);
            continue;
        }
        received[r.job.chunk] = src.chunks[r.job.chunk];
        usedSources.insert(r.job.source);
        scheduler.markDone(r.job.chunk);
        ++completed;
        std::cout << "  [req " << r.id << "] chunk " << r.job.chunk
                  << " <- source " << r.job.source << " OK\n";
    }

    if (scheduler.failed() || !scheduler.finished()) {
        std::cout << "FAILED: no source left for some chunk\n";
        return 1;
    }

    // 4. Rebuild in order and verify.
    std::string rebuilt;
    for (std::size_t i = 0; i < kChunkCount; ++i) {
        rebuilt += received[static_cast<std::uint32_t>(i)];
    }

    std::cout << "sources used: " << usedSources.size() << "\n";
    std::cout << "original checksum: " << checksum(original) << "\n";
    std::cout << "rebuilt  checksum: " << checksum(rebuilt) << "\n";
    if (rebuilt == original) {
        std::cout << "SUCCESS: file rebuilt correctly after source B stopped\n";
        return 0;
    }
    std::cout << "FAILED: rebuilt file differs\n";
    return 1;
}

int runFailureDemo() {
    // Chunk 0 is held only by A and B; both vanish before any reply.
    ChunkScheduler scheduler({{"A", "B"}});
    auto job = scheduler.next();
    std::cout << "  [req 1] ask " << job->source << " for chunk 0\n";
    scheduler.markSourceDown(job->source);
    std::cout << "  !! source " << job->source << " went down\n";
    auto again = scheduler.next();
    std::cout << "  [req 2] ask " << again->source << " for chunk 0\n";
    scheduler.markSourceDown(again->source);
    std::cout << "  !! source " << again->source << " went down\n";
    scheduler.next();
    if (scheduler.failed()) {
        std::cout << "EXPECTED FAILURE: chunk 0 has no reachable copy\n";
        return 0;
    }
    std::cout << "UNEXPECTED: scheduler did not report failure\n";
    return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << cn::project_version << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--demo") {
        return runDemo();
    }
    if (argc == 2 && std::string_view(argv[1]) == "--demo-fail") {
        return runFailureDemo();
    }
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        printUsage();
        return 0;
    }
    std::cerr << "Unsupported option. Run download_demo --help.\n";
    return 2;
}
