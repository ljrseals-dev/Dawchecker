#pragma once
#include <array>
#include <cstddef>
#include "DetectorTypes.h"

namespace dawchecker::dsp {

// Detects embedded watermark artifacts by tracking the residual between a
// reference (pre-processing / sidechain) signal and the live output.
class WatermarkDetector
{
public:
    explicit WatermarkDetector(double sampleRate = 48000.0, double sensitivity = 5.0);

    void prepare(double sampleRate, double sensitivity);
    DetectorResult process(double liveSample, double referenceSample) noexcept;

    double lastDelta() const noexcept { return prevDelta_; }

private:
    double sampleRate_  = 48000.0;
    double sensitivity_ = 5.0;
    double prevRef_ = 0.0, prevLive_ = 0.0, prevDelta_ = 0.0;
    double smoothedStego_ = 0.0;
    // Fixed-capacity history: process() is called from the audio callback and
    // must not allocate or resize a container while processing samples.
    std::array<double, 256> deltaHistory_{};
    std::size_t historyWrite_ = 0;
    std::size_t historyCount_ = 0;
    static constexpr std::size_t kHistoryLen = 256;
};

} // namespace dawchecker::dsp
