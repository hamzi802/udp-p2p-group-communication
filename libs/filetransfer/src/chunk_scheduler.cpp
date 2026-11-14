#include "filetransfer/chunk_scheduler.hpp"

namespace cn::filetransfer {

ChunkScheduler::ChunkScheduler(std::vector<std::vector<std::string>> holders) {
    chunks_.resize(holders.size());
    for (std::size_t i = 0; i < holders.size(); ++i) {
        chunks_[i].holders = std::move(holders[i]);
    }
}

std::size_t ChunkScheduler::inFlight(const std::string& source) const {
    std::size_t n = 0;
    for (const auto& c : chunks_) {
        if (c.state == State::InFlight && c.source == source) ++n;
    }
    return n;
}

std::optional<Assignment> ChunkScheduler::next() {
    for (std::size_t i = 0; i < chunks_.size(); ++i) {
        Chunk& c = chunks_[i];
        if (c.state != State::Pending) continue;

        const std::string* best = nullptr;
        std::size_t bestLoad = 0;
        for (const auto& s : c.holders) {
            if (down_.count(s) || c.tried.count(s)) continue;
            std::size_t load = inFlight(s);
            if (best == nullptr || load < bestLoad) {
                best = &s;
                bestLoad = load;
            }
        }
        if (best == nullptr) {  // every holder is down or already tried
            c.state = State::Failed;
            continue;
        }
        c.state = State::InFlight;
        c.source = *best;
        return Assignment{static_cast<ChunkId>(i), *best};
    }
    return std::nullopt;
}

void ChunkScheduler::markDone(ChunkId chunk) {
    if (chunk < chunks_.size()) chunks_[chunk].state = State::Done;
}

void ChunkScheduler::retryElsewhere(ChunkId chunk) {
    if (chunk >= chunks_.size()) return;
    Chunk& c = chunks_[chunk];
    if (c.state != State::InFlight) return;
    c.tried.insert(c.source);
    c.state = State::Pending;
}

void ChunkScheduler::markSourceDown(const std::string& source) {
    down_.insert(source);
    for (auto& c : chunks_) {
        if (c.state == State::InFlight && c.source == source) {
            c.tried.insert(source);
            c.state = State::Pending;
        }
    }
}

bool ChunkScheduler::finished() const {
    for (const auto& c : chunks_) {
        if (c.state != State::Done) return false;
    }
    return true;
}

bool ChunkScheduler::failed() const {
    for (const auto& c : chunks_) {
        if (c.state == State::Failed) return true;
    }
    return false;
}

}  // namespace cn::filetransfer
