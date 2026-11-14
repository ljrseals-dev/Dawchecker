# Verification status

**Snapshot:** 2026-11-14 UTC  
**Working branch:** `repair/integrated-fixes`  
**Main:** intentionally unchanged during this repair.

## Acceptance evidence

| Area | Successful run | Tested commit | Scope |
|---|---:|---|---|
| Core + crypto | `43570472992` | `6ef6d950` | Mandatory OpenSSL EVP SHA-256 gate, registered CTest suite, ASan/UBSan build and tests, VST3 source-contract checks |
| Server API security | `43570473001` | `6ef6d950` | API compile and regression tests, including Host/Origin, body-size, loopback bind, port validation, status counts, and partial panic failure |
| VST3 SDK compile | `43570255587` | `27166113` | Compiles `Source/dawchecker_vst3.cpp` against pinned Steinberg SDK commit `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` |
| Dashboard | `43569781124` | `0477c1ab` | JavaScript syntax, API behavior tests, and real-browser DOM/action tests |
| Installer | `43570806427` | `36cfb0b1` | Shell syntax, installer contract tests, checksum-gated CSF install tests, and hardened-build sidecar failure tests |

The runs above completed successfully. Earlier failed runs were superseded by later commits and successful reruns; they are not presented as passing evidence.

## Changes covered by those tests

- Removed real-time heap allocations from detector labels and watermark history storage; added an allocation-counter test for `MSV_Engine::process`.
- Replaced the shared pipeline's quadratic DFT with a preallocated radix-2 FFT; tested spectral separation and non-finite input handling.
- Corrected the VST3 heuristic score direction so stronger anomaly features increase, rather than decrease, the score.
- Prevented the ultrasonic scanner from relabeling out-of-Nyquist bands as lower-frequency detections.
- Fixed VST3 evidence-accumulator calls, the factory macro, null-input output handling, and the input/output parameter-ID collision.
- Removed undefined, unused container/hash stubs from the VST3 translation unit.
- Replaced the dashboard's count-derived threat score with the authoritative CSF deny-file count; scan exit codes are not labeled as proof of cleanliness.
- Hardened API binding, Host/Origin checks, JSON body parsing, job snapshots, and partial-failure reporting.
- Made required package failures fatal, scanner packages explicitly optional, and CSF installation fail closed unless an independently verified archive digest is supplied.
- Invalidated stale checksum sidecars before vendoring/configuration and only wrote a new sidecar after successful build and registered tests.
- Corrected documentation to describe DawChecker as an audio-forensics application with a VST3 plugin component, not a VST3 framework.

## Remaining verification limits

1. **VST3 is compile-checked, not release-validated.** CI compiles the plugin translation unit as an object against the pinned SDK. It does not link/package a `.vst3` bundle, load it in a DAW, or test runtime audio, automation, or the optional tracking tone.
2. **No production host installation was performed.** API and installer tests use test servers and mocked system commands. They do not prove behavior against a live CSF firewall, real systemd service, or production package manager.
3. **Real-time allocation evidence is scoped.** The allocation-counter test covers the standalone engine callback and separate pipeline/queue methods on the CI platform. It is not a host-level measurement of every VST3 callback path on every supported OS.
4. **Analysis scores remain heuristic.** They are not calibrated probabilities, proof of authenticity, or courtroom validation.
5. **Hardening checks are partly advisory.** The hardened build script reports ELF hardening properties but does not fail the build when a property is absent.
6. **CSF installation needs an independent digest.** If CSF is absent, the installer requires `CSF_SHA256` from a trusted release source before executing the downloaded installer as root. No checksum is invented or derived from the same download.

These results close the listed CI gates; they do not prove that no other defects exist.
