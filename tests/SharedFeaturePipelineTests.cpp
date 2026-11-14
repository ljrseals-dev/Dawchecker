#include "core/SharedFeaturePipeline.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

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

    if (g_failures == 0) std::puts("All SharedFeaturePipeline tests passed.");
    else std::fprintf(stderr, "%d SharedFeaturePipeline test(s) failed.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
