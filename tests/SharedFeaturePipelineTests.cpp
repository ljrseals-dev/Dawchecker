#include "core/SharedFeaturePipeline.h"
#include "core/RealtimeEvidenceQueue.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>

namespace {
std::atomic<bool> g_track_allocations{false};
std::atomic<std::size_t> g_allocation_count{0};
int g_failures = 0;

void check(bool condition, const char* message) {
    if (condition) std::printf("[ OK ] %s\n", message);
    else {
        std::fprintf(stderr, "[FAIL] %s\n", message);
        ++g_failures;
    }
}
}

void* operator new(std::size_t size) {
    if (g_track_allocations.load(std::memory_order_relaxed)) {
        g_allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (g_track_allocations.load(std::memory_order_relaxed)) {
        g_allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    using dawchecker::dsp::SharedFeaturePipeline;

    SharedFeaturePipeline pipeline44100(44100.0);
    SharedFeaturePipeline pipeline48000(48000.0);
    SharedFeaturePipeline pipeline96000(96000.0);
    check(!pipeline44100.isFrameAvailable(), "empty pipeline has no frame");
    check(pipeline44100.sampleRate() == 44100.0 &&
          pipeline48000.sampleRate() == 48000.0 &&
          pipeline96000.sampleRate() == 96000.0,
          "44.1, 48, and 96 kHz sample rates are retained");

    std::array<float, SharedFeaturePipeline::kFftSize> mono{};
    for (std::size_t i = 0; i < mono.size(); ++i) {
        mono[i] = 0.25f * std::sin(
            2.0f * 3.14159265358979323846f * 1000.0f *
            static_cast<float>(i) / 48000.0f);
    }

    pipeline48000.writeSamples(mono.data(), mono.size() - 1);
    check(!pipeline48000.isFrameAvailable(), "511 samples do not form a 512-sample frame");
    pipeline48000.writeSamples(mono.data() + mono.size() - 1, 1);
    check(pipeline48000.isFrameAvailable(), "512 samples form a frame");

    g_allocation_count.store(0, std::memory_order_relaxed);
    g_track_allocations.store(true, std::memory_order_release);
    const auto result = pipeline48000.processNextFrame(0.5f, 0.25f);
    g_track_allocations.store(false, std::memory_order_release);
    check(g_allocation_count.load(std::memory_order_relaxed) == 0,
          "processNextFrame performs zero heap allocations");
    check(std::isfinite(result.phase_coherence) &&
          std::isfinite(result.high_freq_anomaly) &&
          std::isfinite(result.noise_floor),
          "frame analysis returns finite metrics");
    check(pipeline48000.bufferedSamples() ==
              SharedFeaturePipeline::kFftSize - SharedFeaturePipeline::kHopSize,
          "overlap processing consumes exactly one hop");

    std::array<float, SharedFeaturePipeline::kInputCapacity + 3616> long_input{};
    for (std::size_t i = 0; i < long_input.size(); ++i) {
        long_input[i] = static_cast<float>(i % 97) / 97.0f;
    }

    g_allocation_count.store(0, std::memory_order_relaxed);
    g_track_allocations.store(true, std::memory_order_release);
    pipeline44100.writeSamples(long_input.data(), long_input.size());
    const bool wrapped_frame_available = pipeline44100.isFrameAvailable();
    const auto wrapped = pipeline44100.processNextFrame(0.7f, 0.3f);
    g_track_allocations.store(false, std::memory_order_release);
    check(wrapped_frame_available, "overflow retains a complete newest frame");
    check(pipeline44100.bufferedSamples() ==
              SharedFeaturePipeline::kInputCapacity - SharedFeaturePipeline::kHopSize,
          "ring overflow drops oldest samples and remains bounded");
    check(g_allocation_count.load(std::memory_order_relaxed) == 0,
          "write and process remain allocation-free across ring wrap");
    check(std::isfinite(wrapped.noise_floor), "wrapped frame remains processable");

    std::array<float, 1024> left{};
    std::array<float, 1024> right{};
    left.fill(0.2f);
    right.fill(-0.2f);
    g_allocation_count.store(0, std::memory_order_relaxed);
    g_track_allocations.store(true, std::memory_order_release);
    pipeline96000.writeStereoSamples(left.data(), right.data(), left.size());
    while (pipeline96000.isFrameAvailable()) {
        (void)pipeline96000.processNextFrame(0.5f, 0.25f);
    }
    g_track_allocations.store(false, std::memory_order_release);
    check(g_allocation_count.load(std::memory_order_relaxed) == 0,
          "stereo write and repeated analysis allocate no heap memory");

    dawchecker::dsp::RealtimeEvidenceQueue queue;
    dawchecker::dsp::RealtimeEvidenceEvent event{};
    check(!queue.hasPending() && !queue.pop(event), "empty SPSC queue is empty");

    for (std::size_t i = 0; i < dawchecker::dsp::RealtimeEvidenceQueue::kCapacity - 1; ++i) {
        check(queue.push({static_cast<float>(i), static_cast<std::uint64_t>(i)}),
              "SPSC queue accepts events up to its bounded capacity");
    }
    check(!queue.push({1.0f, 999}), "full SPSC queue rejects newest event without blocking");
    check(queue.pop(event) && event.sequence == 0, "SPSC queue preserves FIFO order");
    check(queue.push({1.0f, 999}), "SPSC queue reuses capacity after consumer pop");

    dawchecker::dsp::RealtimeEvidenceQueue concurrentQueue;
    constexpr std::uint64_t eventCount = 50000;
    std::atomic<bool> producerDone{false};
    std::atomic<int> sequenceErrors{0};
    std::thread producer([&] {
        for (std::uint64_t i = 0; i < eventCount; ++i) {
            const dawchecker::dsp::RealtimeEvidenceEvent next{0.5f, i};
            while (!concurrentQueue.push(next)) std::this_thread::yield();
        }
        producerDone.store(true, std::memory_order_release);
    });
    std::uint64_t expectedSequence = 0;
    while (expectedSequence < eventCount) {
        if (concurrentQueue.pop(event)) {
            if (event.sequence != expectedSequence) {
                sequenceErrors.fetch_add(1, std::memory_order_relaxed);
            }
            ++expectedSequence;
        } else {
            std::this_thread::yield();
        }
    }
    producer.join();
    check(producerDone.load(std::memory_order_acquire) &&
          sequenceErrors.load(std::memory_order_relaxed) == 0,
          "SPSC queue preserves ordering across concurrent producer and consumer");

    if (g_failures == 0) std::puts("All audio pipeline and evidence queue tests passed.");
    else std::fprintf(stderr, "%d SharedFeaturePipeline test(s) failed.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
