# DawChecker Secure — Audio Analysis Core

## Project status

DawChecker contains a C++20 audio-analysis core, an optional desktop GUI, SHA-256 hashing support, and VST3 source files.

**Verification boundary:** the standalone core and its CTest suite are built in CI. The VST3 translation unit is not part of the current CMake build because the Steinberg VST3 SDK consumer target has not been added. The VST3 integration is therefore not build-verified or release-ready.

This project does not claim courtroom validation, proof of authenticity, signer authentication, or a calibrated probability of tampering. Its analysis outputs are heuristic signals that require expert interpretation and independent corroboration.

## Implemented components

### 1. Shared feature pipeline

`dawchecker::dsp::SharedFeaturePipeline` provides:

- 512-sample analysis frames with a 128-sample hop
- Preallocated frame, window, spectrum, and phase-history storage
- A bounded 16,384-sample input ring with a drop-oldest overflow policy
- No heap allocation in its sample-write and frame-processing methods
- A relative spectral split at half the Nyquist range, not a fixed-Hz cutoff

The `audio_pipeline_tests` target checks frame availability, overlap behavior, ring wrap/overflow, finite metrics, supported sample-rate configurations, and zero heap allocations using a test-only global allocation counter.

### 2. Logistic score mapping

The VST3 source contains a logistic mapping from phase-coherence and high-frequency-anomaly features to a score in the range `[0.0, 1.0]`.

The current coefficients (`-2.4` and `0.35`) are implementation constants, not a validated calibration dataset. The result is a **heuristic score**, not a calibrated posterior probability, likelihood ratio, or proof of tampering.

### 3. Evidence accumulation

The VST3 source uses `EvidenceAccumulator` to retain weighted evidence records and derive an accumulated score. This is a heuristic score-aggregation structure; it is not Bayesian inference and does not calculate posterior probabilities.

A bounded single-producer/single-consumer queue transfers fixed-size evidence events from the audio callback to a non-audio worker. The worker performs string and map updates. If the queue is full, the newest event is dropped rather than blocking the audio thread.

### 4. SHA-256 hashing

`dawchecker_crypto` uses OpenSSL EVP for SHA-256 digests. A digest can identify changed bytes when compared with a trusted reference. It does not create a digital signature, identify a signer, or independently establish provenance.

## Build and test the standalone core

Dependencies include a C++20 compiler, CMake, OpenSSL Crypto, Threads, and OpenGL development files. PortAudio is optional.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DDAWCHECKER_REQUIRE_CRYPTO=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The mandatory crypto option must remain enabled for crypto acceptance. The current CI gate rejects a build with zero registered tests.

For the hardened build path:

```bash
chmod +x scripts/build_hardened.sh
./scripts/build_hardened.sh
```

The hardened script removes stale checksum sidecars before rebuilding, aborts on failed or unregistered tests, and publishes a checksum sidecar only after successful build and tests.

## VST3 integration boundary

The VST3 source files are:

- `Source/dawchecker_vst3.cpp`
- `latest/dawchecker_vst3.cpp`

They contain the processor implementation, the evidence worker, channel-layout handling, and the optional 80 Hz tracking tone. These source changes have **not** been compiled against the Steinberg SDK in CI.

Before treating the VST3 integration as verified, add an SDK-backed CMake consumer target that includes `src/`, links `dawchecker_crypto` and `OpenSSL::Crypto`, and builds against a pinned/available Steinberg VST3 SDK. Until that exists and passes CI, the VST3 integration remains a manual release blocker.

## Parameter mapping

- `kParamSensitivity` — scales the high-frequency anomaly contribution.
- `kParamFusionThreshold` — threshold applied to the heuristic score.
- `kParamAntiForensicGate` — gate applied to the high-frequency anomaly feature.

The optional tracking tone is part of the VST3 source implementation only; its behavior is not verified until the SDK-backed target is built and tested.

## Server Protection Toolkit

The independent Linux server toolkit lives under `tools/server-protection/`. It provides CSF/ipset integration, blocklist updates, a loopback API, and a browser dashboard. It is not linked into the desktop audio application and does not add network behavior to the audio core.

See [tools/server-protection/README.md](tools/server-protection/README.md) for installation and safety details.

## Technical reference

See [docs/TECHNICAL_REFERENCE.md](docs/TECHNICAL_REFERENCE.md) for architecture, interfaces, and verification boundaries.

## License and attribution

- Developer: Lewis James Seals Jr. (`TheRealla`)
- Organization: TheRealla Audio Forensic Labs

*Automated analysis is advisory. Interpret results in context and corroborate them with independent methods.*
