#include "core/EvidenceAccumulator.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace dawchecker::dsp {

EvidenceAccumulator::EvidenceAccumulator() {
    addEvidence("root_provenance", 0.5f,
                "Initial system baseline analysis trace.");
}

void EvidenceAccumulator::addEvidence(const std::string& id, float score,
                                      const std::string& narrative) {
    const float boundedScore = std::clamp(score, 0.0f, 1.0f);
    EvidenceRecord record;
    record.id = id;
    record.base_score = boundedScore;
    record.accumulated_score = boundedScore;
    record.narrative_summary = narrative;

    if (boundedScore > 0.85f) record.band = EvidenceScoreBand::StrongSignal;
    else if (boundedScore > 0.65f) record.band = EvidenceScoreBand::ElevatedSignal;
    else if (boundedScore > 0.35f) record.band = EvidenceScoreBand::Indeterminate;
    else if (boundedScore > 0.15f) record.band = EvidenceScoreBand::WeakSignal;
    else record.band = EvidenceScoreBand::MinimalSignal;

    records_[id] = std::move(record);
}

void EvidenceAccumulator::relate(const std::string& parentId,
                                 const std::string& childId, float weight) {
    links_[parentId].push_back(
        {childId, std::clamp(weight, 0.0f, 1.0f)});
}

void EvidenceAccumulator::accumulateEvidenceScores() {
    for (auto& entry : records_) {
        entry.second.accumulated_score = entry.second.base_score;
    }

    for (auto& [parentId, childLinks] : links_) {
        const auto parent = records_.find(parentId);
        if (parent == records_.end()) continue;
        const float parentScore = parent->second.accumulated_score;

        for (const auto& link : childLinks) {
            const auto childIt = records_.find(link.child_id);
            if (childIt == records_.end()) continue;
            auto& child = childIt->second;

            const float updated =
                (child.base_score * link.weight) +
                (parentScore * (1.0f - link.weight));
            child.accumulated_score = std::clamp(updated, 0.0f, 1.0f);
        }
    }
}

std::string EvidenceAccumulator::generateForensicNarrative() {
    accumulateEvidenceScores();
    std::stringstream stream;
    stream << "=== FORENSIC EVIDENCE SUMMARY ===\n";
    for (const auto& [id, record] : records_) {
        stream << "Record [" << id << "] -> Score: "
               << (record.accumulated_score * 100.0f) << "%\n"
               << "   Narrative: " << record.narrative_summary << "\n";
    }
    return stream.str();
}

} // namespace dawchecker::dsp
