from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class VST3SourceContractTests(unittest.TestCase):
    def test_primary_and_snapshot_sources_are_identical(self):
        primary = (ROOT / "Source/dawchecker_vst3.cpp").read_bytes()
        snapshot = (ROOT / "latest/dawchecker_vst3.cpp").read_bytes()
        self.assertEqual(primary, snapshot)

    def test_evidence_worker_uses_real_accumulator_api(self):
        source = (ROOT / "Source/dawchecker_vst3.cpp").read_text(encoding="utf-8")
        self.assertIn("evidence_graph_ = std::make_unique<EvidenceAccumulator>()", source)
        self.assertIn("evidence_graph_->addEvidence(", source)
        self.assertIn("evidence_graph_->relate(", source)
        self.assertNotIn("evidence_accumulator_", source)
        self.assertNotIn("addNode(", source)
        self.assertNotIn("addEdge(", source)

    def test_audio_callback_does_not_publish_over_input_parameter_id(self):
        source = (ROOT / "Source/dawchecker_vst3.cpp").read_text(encoding="utf-8")
        self.assertNotIn("outputParameterChanges", source)
        self.assertIn("std::fill_n(output, data.numSamples, 0.0f)", source)

    def test_removed_undefined_container_stub_is_not_reintroduced(self):
        source = (ROOT / "Source/dawchecker_vst3.cpp").read_text(encoding="utf-8")
        self.assertNotIn("ContainerEvidence", source)
        self.assertNotIn("ContainerType", source)
        self.assertIn('"DawChecker Audio Forensics"', source)
        self.assertIn("BEGIN_FACTORY(", source)
        self.assertNotIn("BEGIN_FACTORY_DEF", source)


if __name__ == "__main__":
    unittest.main()
