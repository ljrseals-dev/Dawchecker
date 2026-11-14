#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace dawchecker::dsp {

struct RealtimeEvidenceEvent {
    float probability = 0.0f;
    std::uint64_t sequence = 0;
};

// Single-producer/single-consumer bounded queue. The audio callback is the
// sole producer; the evidence worker is the sole consumer. Full queues reject
// the newest event rather than blocking or allocating on the producer.
class RealtimeEvidenceQueue final {
public:
    static constexpr std::size_t kCapacity = 64;

    bool push(const RealtimeEvidenceEvent& event) noexcept {
        const std::size_t write = write_index_.load(std::memory_order_relaxed);
        const std::size_t next = (write + 1) % kCapacity;
        if (next == read_index_.load(std::memory_order_acquire)) return false;
        events_[write] = event;
        write_index_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(RealtimeEvidenceEvent& event) noexcept {
        const std::size_t read = read_index_.load(std::memory_order_relaxed);
        if (read == write_index_.load(std::memory_order_acquire)) return false;
        event = events_[read];
        read_index_.store((read + 1) % kCapacity, std::memory_order_release);
        return true;
    }

    bool hasPending() const noexcept {
        return read_index_.load(std::memory_order_acquire) !=
               write_index_.load(std::memory_order_acquire);
    }

private:
    std::array<RealtimeEvidenceEvent, kCapacity> events_{};
    alignas(64) std::atomic<std::size_t> read_index_{0};
    alignas(64) std::atomic<std::size_t> write_index_{0};
};

} // namespace dawchecker::dsp
