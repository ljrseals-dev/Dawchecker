#pragma once

#include <algorithm>
#include <cmath>

namespace dawchecker::dsp {

// Monotonic logistic mapping for VST3 feature signals.
// This is a heuristic score, not a calibrated probability.
class EvidenceFusionEngine final {
public:
    float calculateTamperScore(float coherence,
                               float highFrequencyAnomaly,
                               float sensitivity) const noexcept {
        if (!std::isfinite(coherence) ||
            !std::isfinite(highFrequencyAnomaly) ||
            !std::isfinite(sensitivity)) {
            return 0.0f;
        }

        const float boundedCoherence = std::clamp(coherence, 0.0f, 1.0f);
        const float boundedAnomaly = std::max(highFrequencyAnomaly, 0.0f);
        const float boundedSensitivity = std::clamp(sensitivity, 0.0f, 1.0f);
        const float featureScore =
            ((1.0f - boundedCoherence) * 1.75f) +
            (boundedAnomaly * boundedSensitivity * 2.25f);

        // Positive slope: stronger anomaly features must never lower the score.
        constexpr double kSlope = 2.4;
        constexpr double kIntercept = 0.35;
        const double logit =
            (static_cast<double>(featureScore) * kSlope) + kIntercept;
        return static_cast<float>(1.0 / (1.0 + std::exp(-logit)));
    }
};

} // namespace dawchecker::dsp
