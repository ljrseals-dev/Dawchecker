#pragma once

#include <array>
#include <complex>
#include <cstddef>

namespace dawchecker::dsp {

struct FrameAnalysisResult {
    float phase_coherence = 0.0f;
    float high_freq_anomaly = 0.0f;
    float noise_floor = 0.0f;
    bool splice_flag = false;
};

class SharedFeaturePipeline final {
public:
    static constexpr std::size_t kFftSize = 512;
    static constexpr std::size_t kHopSize = 128;
    static constexpr std::size_t kInputCapacity = 16384;
    static constexpr std::size_t kBinCount = kFftSize / 2 + 1;

    explicit SharedFeaturePipeline(double sampleRate) noexcept;

    void writeSamples(const float* data, std::size_t sampleCount) noexcept;
    void writeStereoSamples(const float* left, const float* right,
                            std::size_t sampleCount) noexcept;

    bool isFrameAvailable() const noexcept;
    FrameAnalysisResult processNextFrame(float sensitivity,
                                         float anomalyGate) noexcept;

    std::size_t bufferedSamples() const noexcept { return buffered_samples_; }
    double sampleRate() const noexcept { return sample_rate_; }

private:
    void pushSample(float sample) noexcept;

    double sample_rate_;
    std::size_t write_pos_ = 0;
    std::size_t read_pos_ = 0;
    std::size_t buffered_samples_ = 0;

    std::array<float, kInputCapacity> input_buffer_{};
    std::array<float, kFftSize> fft_window_{};
    std::array<float, kFftSize> frame_buffer_{};
    std::array<std::complex<float>, kBinCount> spectrum_buffer_{};
    std::array<float, kBinCount> previous_phases_{};
    std::array<float, 16> history_coherence_{};
    std::size_t history_index_ = 0;
};

} // namespace dawchecker::dsp
