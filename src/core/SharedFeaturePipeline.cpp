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
    // Invalid floating-point samples must not poison every later FFT frame.
    if (!std::isfinite(sample)) sample = 0.0f;
    sample = std::clamp(sample, -1.0e6f, 1.0e6f);
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

    // Iterative radix-2 Cooley-Tukey FFT. All workspace is fixed-size and
    // preallocated in the pipeline object; no allocation or lock occurs here.
    for (std::size_t i = 0; i < kFftSize; ++i) {
        spectrum_buffer_[i] = {frame_buffer_[i], 0.0f};
    }

    std::size_t bit_reversed = 0;
    for (std::size_t i = 1; i < kFftSize; ++i) {
        std::size_t bit = kFftSize >> 1;
        while ((bit_reversed & bit) != 0) {
            bit_reversed ^= bit;
            bit >>= 1;
        }
        bit_reversed ^= bit;
        if (i < bit_reversed) {
            std::swap(spectrum_buffer_[i], spectrum_buffer_[bit_reversed]);
        }
    }

    for (std::size_t length = 2; length <= kFftSize; length <<= 1) {
        const float angle = static_cast<float>(
            -2.0 * kPi / static_cast<double>(length));
        const std::complex<float> root(std::cos(angle), std::sin(angle));
        const std::size_t half = length >> 1;
        for (std::size_t base = 0; base < kFftSize; base += length) {
            std::complex<float> twiddle(1.0f, 0.0f);
            for (std::size_t offset = 0; offset < half; ++offset) {
                const auto even = spectrum_buffer_[base + offset];
                const auto odd = spectrum_buffer_[base + offset + half] * twiddle;
                spectrum_buffer_[base + offset] = even + odd;
                spectrum_buffer_[base + offset + half] = even - odd;
                twiddle *= root;
            }
        }
    }

    float high_frequency_energy = 0.0f;
    float low_frequency_energy = 0.0f;
    float total_phase_difference = 0.0f;

    const float bounded_sensitivity = std::isfinite(sensitivity)
        ? std::clamp(sensitivity, 0.0f, 1.0f) : 0.0f;
    const float bounded_anomaly_gate = std::isfinite(anomalyGate)
        ? std::clamp(anomalyGate, 0.0f, 1.0f) : 0.0f;

    for (std::size_t k = 0; k < kBinCount; ++k) {
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
    result.high_freq_anomaly =
        calculateHighFrequencyAnomaly(high_frequency_energy, low_frequency_energy);
    result.noise_floor =
        low_frequency_energy / static_cast<float>(kBinCount);
    result.splice_flag =
        (result.high_freq_anomaly * bounded_sensitivity > bounded_anomaly_gate) &&
        (result.phase_coherence < 0.50f);

    return result;
}

} // namespace dawchecker::dsp
