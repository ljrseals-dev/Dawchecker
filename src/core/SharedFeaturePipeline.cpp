#include "core/SharedFeaturePipeline.h"

#include <algorithm>
#include <cmath>

namespace dawchecker::dsp {
namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
}

SharedFeaturePipeline::SharedFeaturePipeline(double sampleRate) noexcept
    : sample_rate_(std::isfinite(sampleRate) && sampleRate > 0.0
                       ? sampleRate
                       : 44100.0) {
    for (std::size_t i = 0; i < kFftSize; ++i) {
        const double phase = (2.0 * kPi * static_cast<double>(i)) /
                             static_cast<double>(kFftSize - 1);
        fft_window_[i] = static_cast<float>(0.5 * (1.0 - std::cos(phase)));
    }
    history_coherence_.fill(1.0f);
}

void SharedFeaturePipeline::pushSample(float sample) noexcept {
    input_buffer_[write_pos_] = sample;
    write_pos_ = (write_pos_ + 1) % kInputCapacity;

    if (buffered_samples_ == kInputCapacity) {
        // Bounded drop-oldest policy: preserve the newest input on overflow.
        read_pos_ = (read_pos_ + 1) % kInputCapacity;
        --buffered_samples_;
    }
    ++buffered_samples_;
}

void SharedFeaturePipeline::writeSamples(const float* data,
                                         std::size_t sampleCount) noexcept {
    if (data == nullptr) return;
    for (std::size_t i = 0; i < sampleCount; ++i) {
        pushSample(data[i]);
    }
}

void SharedFeaturePipeline::writeStereoSamples(const float* left,
                                               const float* right,
                                               std::size_t sampleCount) noexcept {
    if (left == nullptr && right == nullptr) return;
    if (left == nullptr) {
        writeSamples(right, sampleCount);
        return;
    }
    if (right == nullptr) {
        writeSamples(left, sampleCount);
        return;
    }

    for (std::size_t i = 0; i < sampleCount; ++i) {
        pushSample(0.5f * (left[i] + right[i]));
    }
}

bool SharedFeaturePipeline::isFrameAvailable() const noexcept {
    return buffered_samples_ >= kFftSize;
}

FrameAnalysisResult SharedFeaturePipeline::processNextFrame(
    float sensitivity, float anomalyGate) noexcept {
    FrameAnalysisResult result{};
    if (!isFrameAvailable()) return result;

    std::size_t cursor = read_pos_;
    for (std::size_t i = 0; i < kFftSize; ++i) {
        frame_buffer_[i] = input_buffer_[cursor] * fft_window_[i];
        cursor = (cursor + 1) % kInputCapacity;
    }

    read_pos_ = (read_pos_ + kHopSize) % kInputCapacity;
    buffered_samples_ -= kHopSize;

    float high_frequency_energy = 0.0f;
    float low_frequency_energy = 0.0f;
    float total_phase_difference = 0.0f;

    for (std::size_t k = 0; k < kBinCount; ++k) {
        float real_sum = 0.0f;
        float imaginary_sum = 0.0f;
        const float angle_factor = static_cast<float>(
            -2.0 * kPi * static_cast<double>(k) /
            static_cast<double>(kFftSize));

        for (std::size_t n = 0; n < kFftSize; ++n) {
            const float angle = angle_factor * static_cast<float>(n);
            real_sum += frame_buffer_[n] * std::cos(angle);
            imaginary_sum += frame_buffer_[n] * std::sin(angle);
        }

        spectrum_buffer_[k] = {real_sum, imaginary_sum};
        const float magnitude = std::abs(spectrum_buffer_[k]);
        const float phase = std::arg(spectrum_buffer_[k]);

        // This is a relative spectral-shape split at half the Nyquist range,
        // not a fixed-frequency cutoff shared by all sample rates.
        if (k > kBinCount / 2) high_frequency_energy += magnitude;
        else low_frequency_energy += magnitude;

        const float phase_difference = phase - previous_phases_[k];
        previous_phases_[k] = phase;
        total_phase_difference += std::abs(std::atan2(
            std::sin(phase_difference), std::cos(phase_difference)));
    }

    const float normalized_coherence = 1.0f -
        (total_phase_difference /
         (static_cast<float>(kBinCount) * static_cast<float>(kPi)));
    history_coherence_[history_index_] = normalized_coherence;
    history_index_ = (history_index_ + 1) % history_coherence_.size();

    float sum_coherence = 0.0f;
    for (float value : history_coherence_) sum_coherence += value;
    const float mean_coherence =
        sum_coherence / static_cast<float>(history_coherence_.size());

    result.phase_coherence = mean_coherence;
    result.high_freq_anomaly = low_frequency_energy > 0.0f
        ? high_frequency_energy / low_frequency_energy
        : 0.0f;
    result.noise_floor =
        low_frequency_energy / static_cast<float>(kBinCount);
    result.splice_flag =
        (result.high_freq_anomaly * sensitivity > anomalyGate) &&
        (result.phase_coherence < 0.50f);

    return result;
}

} // namespace dawchecker::dsp
