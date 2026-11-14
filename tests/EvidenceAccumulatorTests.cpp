#include "core/EvidenceAccumulator.h"

#include <cstdio>
#include <string>

int main() {
    int failures = 0;
    auto check = [&failures](bool condition, const char* message) {
        if (condition) std::printf("[ OK ] %s\n", message);
        else {
            std::fprintf(stderr, "[FAIL] %s\n", message);
            ++failures;
        }
    };

    dawchecker::dsp::EvidenceAccumulator accumulator;
    accumulator.addEvidence("high", 1.5f, "High input score is clamped.");
    accumulator.addEvidence("low", -0.5f, "Low input score is clamped.");
    accumulator.relate("root_provenance", "high", 0.75f);
    accumulator.relate("missing-parent", "low", 0.5f);
    accumulator.accumulateEvidenceScores();

    const std::string report = accumulator.generateForensicNarrative();
    const std::string repeatedReport = accumulator.generateForensicNarrative();
    check(report == repeatedReport, "repeated score accumulation is deterministic and idempotent");
    check(report.find("FORENSIC EVIDENCE SUMMARY") != std::string::npos,
          "report identifies itself as an evidence summary");
    check(report.find("Score:") != std::string::npos,
          "report labels outputs as scores, not probabilities");
    check(report.find("Probability:") == std::string::npos,
          "report does not claim probability values");
    check(report.find("Bayesian") == std::string::npos &&
          report.find("posterior") == std::string::npos,
          "report makes no Bayesian or posterior claim");
    check(report.find("87.5") != std::string::npos,
          "weighted score blending combines evidence and parent score");
    check(report.find("Score: 0%") != std::string::npos,
          "negative input score is clamped to zero");

    if (failures == 0) std::puts("All EvidenceAccumulator tests passed.");
    else std::fprintf(stderr, "%d EvidenceAccumulator test(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
