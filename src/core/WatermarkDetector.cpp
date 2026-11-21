#include "WatermarkDetector.h"
#include <algorithm>
#include <cmath>

namespace dawchecker::dsp {

WatermarkDetector::WatermarkDetector(double sampleRate, double sensitivity)
{
    prepare(sampleRate, sensitivity);
}

void WatermarkDetector::prepare(double sampleRate, double sensitivity)
{
    sampleRate_  = sampleRate > 0.0 ? sampleRate : 48000.0;
    sensitivity_ = std::clamp(sensitivity, 1.0, 100.0);
    prevRef_ = prevLive_ = prevDelta_ = 0.0;
    smoothedStego_ = 0.0;
    deltaHistory_.fill(0.0);
    historyWrite_ = 0;
    historyCount_ = 0;
}

DetectorResult WatermarkDetector::process(double liveSample, double referenceSample) noexcept
{
    const double delta = liveSample - referenceSample;

    const double phaseDeviation = std::abs(liveSample * prevRef_ - referenceSample * prevLive_);
    prevRef_  = referenceSample;
    prevLive_ = liveSample;

    const bool structuredStep = std::abs(delta - prevDelta_) > (0.000001 * sensitivity_);
    prevDelta_ = delta;

    // Overwrite the oldest sample in-place. No deque growth/pop allocation
    // occurs on the real-time audio thread.
    deltaHistory_[historyWrite_] = delta;
    historyWrite_ = (historyWrite_ + 1) % kHistoryLen;
    if (historyCount_ < kHistoryLen)
        ++historyCount_;

    double deltaMean = 0.0;
    for (std::size_t i = 0; i < historyCount_; ++i)
        deltaMean += deltaHistory_[i];
    if (historyCount_ != 0)
        deltaMean /= static_cast<double>(historyCount_);

    double deltaVariance = 0.0;
    for (std::size_t i = 0; i < historyCount_; ++i)
    {
        const double difference = deltaHistory_[i] - deltaMean;
        deltaVariance += difference * difference;
    }
    if (historyCount_ != 0)
        deltaVariance /= static_cast<double>(historyCount_);

    const double persistentBias = std::abs(deltaMean);
    const double raw = phaseDeviation * 8.0 + (structuredStep ? 0.02 : 0.0) +
                       (deltaVariance < 1e-6 ? persistentBias * 40.0 : 0.0);

    smoothedStego_ += (raw - smoothedStego_) * 0.004;

    const double adaptiveThreshold = 0.0004 * sensitivity_ / 5.0;

    DetectorResult result;
    result.confidence = std::clamp(smoothedStego_ * (500.0 / std::max(sensitivity_, 1.0)), 0.0, 100.0);

    if (smoothedStego_ > adaptiveThreshold * 2.5)
    {
        result.status = DetectorStatus::Alert;
        result.label  = "Structured watermark payload detected";
    }
    else if (smoothedStego_ > adaptiveThreshold)
    {
        result.status = DetectorStatus::Active;
        result.label  = "Possible watermark residual";
    }
    else
    {
        result.status = DetectorStatus::Idle;
        result.label  = "No watermark signature detected";
    }
    return result;
}

} // namespace dawchecker::dsp
