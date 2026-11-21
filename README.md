# DawChecker Secure Ultimate

**DawChecker is an audio-forensics application with a VST3 plugin component.** It combines a standalone C++20 desktop application and analysis core with a Steinberg VST3 effect for use inside compatible audio workstations. The repository also contains separate developer and Linux server-protection utilities; those are not part of the audio-processing runtime.

The project is intended to help engineers and forensic examiners inspect audio characteristics and preserve useful integrity evidence. Its outputs are analytical indicators, not proof that a recording is authentic, manipulated, or suitable for a legal conclusion without independent examination.

## Project components

| Component | Location | Purpose |
| --- | --- | --- |
| Standalone application and core | `src/`, `CMakeLists.txt` | C++20 analysis engine, desktop GUI, device support, and tests |
| Steinberg VST3 component | `Source/dawchecker_vst3.cpp`, `latest/dawchecker_vst3.cpp` | In-DAW audio effect and forensic feature analysis |
| SHA-256 implementation | `src/core/ForensicHash.cpp`, `src/core/ForensicHash.h` | File/data digest using OpenSSL EVP |
| Automated tests | `tests/` | Core behavior and SHA-256 test coverage |
| Technical documentation | `docs/` | Architecture, security notes, and operational reference |
| Server protection utilities | `tools/server-protection/` | Separate Linux host hardening and CSF management tools |
| Clipboard MCP companion | `tools/clipboard-mcp/` | Optional, separate developer utility |

## VST3 component and Steinberg SDK

The VST3 component is implemented against the **Steinberg VST3 SDK**. Its source includes Steinberg's VST3 processor, factory, parameter-change, process-context, and base-string interfaces. The processor uses the single-component effect model (`SingleComponentEffect`) and registers stereo audio input/output buses and three parameters:

- Detection Sensitivity
- Evidence Calibration Ceiling
- Anti-Forensic Mitigation Gate

The plugin source is in `Source/dawchecker_vst3.cpp`; `latest/dawchecker_vst3.cpp` is the corresponding maintained snapshot. The Steinberg SDK is an external build dependency and is not vendored in this repository.

**Build-system limitation:** the top-level `CMakeLists.txt` configures only the standalone application/core and `engine_tests`. This repository currently contains no VST3 CMake target or checked-in SDK-aware plugin build configuration. The VST3 source is therefore not built or validated by the repository's standalone build. A separate, reproducible SDK-backed plugin build definition and host-load test are still required before the VST3 component can be considered a deliverable. Do not interpret a successful standalone CMake build as a VST3 build.

## Audio analysis

The VST3 source contains a frame-based feature pipeline with a 512-sample analysis frame and 128-sample hop. It calculates spectral magnitudes and phase changes, tracks a short coherence history, and derives high-frequency and noise-floor indicators. The current reference implementation uses a direct discrete Fourier transform loop; it should not be described as an FFTW/IPP-backed or optimized FFT implementation.

The plugin also contains a logistic evidence-fusion calculation using fixed coefficients and an evidence-graph data structure. These calculations are implementation-level indicators, not a validated forensic probability model. The repository does not establish courtroom admissibility, scientific validation across all codecs/recording chains, or a quantified false-positive/false-negative rate.

The standalone core includes modular analysis and utility code, including:

- Electric Network Frequency (ENF) analysis
- Ultrasonic content inspection
- Watermark-pattern detection
- Optional signal neutralization stages
- Codec/device identification helpers
- Checked arithmetic, export-path checks, and secure buffer-zeroing helpers

Neutralization changes audio. Preserve an untouched source and document any processing performed on an evidentiary copy.

## SHA-256 and integrity

The shared forensic hash implementation uses OpenSSL's EVP interface to calculate SHA-256. The VST3 adapter delegates to this shared implementation.

A SHA-256 digest can identify data and help detect whether bytes have changed when compared with a trusted digest. **A digest is not a digital signature**: by itself it does not identify who created a file, prove when it was created, or establish that its contents are authentic. Preserve the original media, record the tool/build version, and protect the reference digest separately.

The standalone hardened build script can emit a SHA-256 sidecar for the built executable when a compatible hash utility is available.

## Requirements

### Standalone application and core

- C++20 compiler
- CMake 3.16 or newer
- OpenGL development libraries
- Threads
- OpenSSL Crypto development libraries (mandatory by default for the SHA-256 acceptance build)
- Dear ImGui sources under `third_party/imgui` to build the GUI
- GLFW for the GUI target
- PortAudio is optional; without it, the application can build without live PortAudio input

The top-level build may skip the GUI when Dear ImGui is absent; the core library and tests are still configured. If ImGui is present but GLFW is unavailable, CMake stops with a dependency error.

### VST3 component

- Steinberg VST3 SDK
- A compatible C++20 compiler/toolchain
- The OpenSSL Crypto dependency required by the shared SHA-256 implementation

The SDK is not included in this repository. Use the SDK-aware plugin build setup for the VST3 artifact.

## Build the standalone application and tests

On a Linux system with the dependencies installed:

```bash
# Optional: vendor the pinned Dear ImGui source used by the GUI build
./scripts/vendor_imgui.sh

# Configure and build the standalone targets
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run the registered tests
ctest --test-dir build --output-on-failure
```

Or use the hardened build helper:

```bash
chmod +x scripts/build_hardened.sh
./scripts/build_hardened.sh
```

The helper vendors ImGui, configures the standalone CMake project, builds, runs CTest, and performs best-effort ELF hardening checks on Linux. If the GUI dependencies are unavailable, it can complete with the core and tests while reporting that the GUI executable was skipped.

### Relevant CMake options

| Option | Default | Meaning |
| --- | --- | --- |
| `DAWCHECKER_USE_PORTAUDIO` | `ON` | Attempt to enable PortAudio input when installed |
| `DAWCHECKER_REQUIRE_CRYPTO` | `ON` | Require OpenSSL Crypto and build the SHA-256 library/tests |
| `IMGUI_DIR` | `third_party/imgui` | Location of the Dear ImGui source tree |

To explicitly disable the mandatory crypto dependency for a local standalone configuration, pass `-DDAWCHECKER_REQUIRE_CRYPTO=OFF`. That configuration is not suitable for the crypto acceptance gate or for a build that requires the SHA-256 component.

## Tests and verification

The standalone test executable is `engine_tests`, registered with CTest. The test suite includes core engine checks and SHA-256 known-answer tests when the crypto option is enabled.

The repository also defines GitHub Actions workflows for the crypto acceptance gate and server-protection API security tests. A passing workflow proves only the checks actually executed by that workflow at the commit it tested. It does not, by itself, prove real-world forensic accuracy, complete real-time safety, compatibility with every DAW, or correctness on every supported operating system.

For the VST3 deliverable, verify the SDK-backed plugin build and load the resulting plugin in a compatible VST3 host. Keep that verification distinct from the standalone CMake/CTest results.

## Security and operating boundaries

- The audio application is designed to operate without telemetry or a license-server dependency.
- The server-protection toolkit is a separate Linux administration component. It has its own privileged operations and network behavior; it is not linked into the audio application.
- Do not expose the server-protection dashboard/API directly to the public internet. Review its documentation and bind/configuration requirements before deployment.
- Build hardening flags reduce selected classes of risk; they do not make software invulnerable.
- The startup security checks are informational. They are not a substitute for operating-system security controls or a formal security assessment.

## Repository documentation

- [Technical reference](docs/TECHNICAL_REFERENCE.md)
- [Security hardening notes](docs/HARDENING.md)
- [Server protection toolkit](tools/server-protection/README.md)
- [Clipboard MCP companion](tools/clipboard-mcp/README.md)

## License and attribution

Licensed under the MIT License; see [LICENSE](LICENSE).

**Developer:** Lewis James Seals Jr.  
**Organization:** TheRealla Audio Forensic Labs

---

*DawChecker is decision-support software. Preserve original recordings and maintain independent case notes, acquisition records, and chain-of-custody controls. Do not present automated scores or digests as standalone proof of authenticity or tampering.*
