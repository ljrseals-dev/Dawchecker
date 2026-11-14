#pragma once
#include <array>
#include <cstddef>
#include "DetectorTypes.h"

namespace dawchecker::dsp {

// Detects embedded watermark artifacts by tracking the residual between a
// reference (pre-processing / sidechain) signal and the live output —
// stable, low-magnitude, structured deltas are consistent with watermark
// payloads rather than ordinary processing or noise.
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
    static constexpr std::size_t kHistoryLen = 256;
    std::array<double, kHistoryLen> deltaHistory_{};
    std::size_t historyWritePos_ = 0;
    std::size_t historyCount_ = 0;
};

} // namespace dawchecker::dsp
