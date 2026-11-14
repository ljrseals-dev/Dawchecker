#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace dawchecker::dsp {

enum class EvidenceScoreBand {
    StrongSignal,
    ElevatedSignal,
    Indeterminate,
    WeakSignal,
    MinimalSignal
};

struct EvidenceRecord {
    std::string id;
    float base_score = 0.0f;
    float accumulated_score = 0.0f;
    std::string narrative_summary;
    EvidenceScoreBand band = EvidenceScoreBand::Indeterminate;
};

// Worker-thread-owned weighted score accumulator. This is not a Bayesian
// model and does not produce posterior probabilities.
class EvidenceAccumulator final {
public:
    EvidenceAccumulator();

    void addEvidence(const std::string& id, float score,
                     const std::string& narrative);
    void relate(const std::string& parentId, const std::string& childId,
                float weight);
    void accumulateEvidenceScores();
    std::string generateForensicNarrative();

private:
    struct EvidenceLink {
        std::string child_id;
        float weight;
    };

    std::unordered_map<std::string, EvidenceRecord> records_;
    std::unordered_map<std::string, std::vector<EvidenceLink>> links_;
};

} // namespace dawchecker::dsp
