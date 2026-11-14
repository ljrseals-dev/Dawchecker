# DawChecker — Technical Reference

**Version:** 3.0.0  
**Organization:** TheRealla Audio Forensic Labs  
**License:** MIT  
**Companion toolkit:** Server Protection Pro (tools/server-protection)

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Repository Layout](#2-repository-layout)
3. [Audio Forensic Core](#3-audio-forensic-core)
4. [GUI Application](#4-gui-application)
5. [VST3 plugin component](#5-vst3-plugin-component)
6. [Security Hardening](#6-security-hardening)
7. [Build System](#7-build-system)
8. [Testing](#8-testing)
9. [Server Protection Pro](#9-server-protection-pro)
10. [HTTP API Reference](#10-http-api-reference)
11. [MSV Blocklist Integration](#11-msv-blocklist-integration)
12. [Operational Procedures](#12-operational-procedures)
13. [Threat Model & Limitations](#13-threat-model--limitations)
14. [Glossary](#14-glossary)

---

## 1. System Overview

DawChecker is an **audio-forensics application with a VST3 plugin component**. The repository also contains an independently operated Linux server-protection toolkit. These are separate programs with separate runtime boundaries:

| Domain | Component | Runtime | Network |
|--------|-----------|---------|---------|
| **A. Audio application** | Desktop GUI + VST3 plugin component | User session | **None** at runtime |
| **B. Server hardening** | Server Protection Pro | Root on Linux host | Loopback API + outbound list fetch (admin only) |

These domains share branding (“MSV”) but **do not share process space, libraries, or network policy**. The audio application never calls the server toolkit.

### 1.1 Design pillars (audio)

1. **Hermetic DSP** — core analysis avoids external FFT libraries in the VST3 path; modular detectors in the standalone engine.
2. **Evidence fusion** — multi-detector scores aggregated into a tamper score (0–100).
3. **Integrity and safe-handling utilities** — SHA-256 sidecar generation, secure buffer zeroing, and path-traversal rejection. A checksum alone does not establish chain of custody.
4. **Offline by policy** — no telemetry, no license server, no phone-home (see `docs/HARDENING.md`).

### 1.2 Design pillars (server)

1. **CSF-native blocklists** — only `NAME|INTERVAL|MAX|URL` lines in `csf.blocklists`.
2. **No demo mode** — API refuses to start without CSF.
3. **Loopback bind** — dashboard API defaults to `127.0.0.1:8080`.
4. **Root for mutations** — allow/deny/reload require root unless explicitly overridden.

---

## 2. Repository Layout

```
Dawchecker-Secure-Ultimate-main/
├── CMakeLists.txt                 # Standalone app + tests
├── README.md
├── LICENSE
├── docs/
│   ├── HARDENING.md
│   └── TECHNICAL_REFERENCE.md     # This document
├── scripts/
│   ├── build_hardened.sh          # Release build + tests
│   └── vendor_imgui.sh            # Dear ImGui v1.90.9
├── src/
│   ├── main.cpp
│   ├── core/                      # DSP + security library
│   └── gui/                       # ImGui panels
├── Source/dawchecker_vst3.cpp     # VST3 plugin component source
├── latest/dawchecker_vst3.cpp     # Alternate VST3 plugin source snapshot
├── tests/EngineTests.cpp
└── tools/server-protection/       # Server Protection Pro
    ├── install.sh
    ├── run_dashboard.sh
    ├── server_protection_api.py
    ├── msv-blocklist-update.sh
    ├── monitor.sh
    ├── dashboard/index.html
    ├── scripts/
    └── systemd/server-protection.service
```

---

## 3. Audio Forensic Core

**Namespace:** `dawchecker::dsp`  
**Library target:** `dawchecker_core` (static)

### 3.1 MSV_Engine

**Headers:** `src/core/MSV_Engine.h`  
**Source:** `src/core/MSV_Engine.cpp`

Central aggregator. Processes one sample pair per call:

```text
process(referenceSample, liveSample, out) → cleaned sample
snapshot() → EngineSnapshot   // atomic snapshot read by GUI thread
```

| Field / member | Type | Description |
|----------------|------|-------------|
| `tamperScore` | `double` (0–100) | Aggregate fusion score |
| `isCompromised` | `bool` | Thresholded alert flag |
| `watermark` / `enf` / `ultrasonic` | `DetectorResult` | Per-module results |
| `watermarkScrubEnabled` | `atomic<bool>` | Neutralizer toggle |
| `enfNotchEnabled` | `atomic<bool>` | Neutralizer toggle |
| `ultrasonicRolloffEnabled` | `atomic<bool>` | Neutralizer toggle |
| `bypassAll` | `atomic<bool>` | Pass-through |

**Thread model:** `process()` is real-time safe (audio thread). Cross-thread publication uses `std::atomic`. GUI must not allocate or lock in ways that block the audio callback.

### 3.2 DetectorTypes

**Header:** `src/core/DetectorTypes.h`

Shared result structures (`DetectorResult`, status enums) consumed by ENF, ultrasonic, watermark modules and the GUI dashboard.

### 3.3 ENFAnalyzer

**Role:** Electric Network Frequency (mains 50/60 Hz + harmonics) consistency analysis for provenance hints.

Environmental hum footprints can support (not alone prove) temporal/geographic claims when compared against reference grids.

### 3.4 UltrasonicScanner

**Role:** Energy / structure above the nominal audio band (e.g. ~19.2 kHz test tones).  
**Test expectation:** Injected 19.2 kHz tone raises ultrasonic confidence (`tests/EngineTests.cpp`).

### 3.5 WatermarkDetector

**Role:** Detect embedded forensic watermark patterns in the live path; pairs with scrub neutralizer when enabled.

### 3.6 Neutralizers

**Header:** `src/core/Neutralizers.h`

| Class | Function |
|-------|----------|
| `WatermarkScrubber` | Attenuate/remove detected watermark energy |
| `ENFNotchFilter` | Notch mains-related components when enabled |
| `UltrasonicRolloff` | High-frequency rolloff |

Controlled by atomics on `MSV_Engine`; applied inside `process()` when not bypassed.

### 3.6.1 SharedFeaturePipeline and real-time evidence queue

**Files:** `src/core/SharedFeaturePipeline.h/.cpp`, `src/core/RealtimeEvidenceQueue.h`

- Fixed-storage 512-sample analysis frame, 128-sample hop, and 16,384-sample input ring.
- Ring overflow uses a bounded drop-oldest policy; processing does not allocate or lock.
- The 16-frame phase-coherence history and FFT working buffers are preallocated.
- The spectral split is relative to half the Nyquist range, not a fixed-Hz cutoff.
- `EvidenceAccumulator` uses bounded weighted score blending; it is not a Bayesian model and does not produce posterior probabilities.
- `audio_pipeline_tests` checks 44.1/48/96 kHz configuration, frame availability, ring wrap/overflow, finite output, and zero heap allocations using a test-only global `operator new` counter.
- `RealtimeEvidenceQueue` is a bounded SPSC queue. The audio thread only enqueues fixed-size events; the non-audio worker performs evidence-graph string/map updates. Full queues drop the newest event rather than block the producer.

### 3.7 CodecScanner & AudioDeviceManager

**Namespaces:** `dawchecker::codec`, `dawchecker::audio`

- Enumerate devices via **PortAudio** (`DAWCHECKER_HAVE_PORTAUDIO`) or platform fallbacks (WASAPI / ALSA).
- Classify driver/codec strings against a known-substring table for the Codec panel.
- Standalone app falls back to a **synthetic demo signal** if no capture device exists (GUI never blank).

### 3.8 SecurityHardening (runtime helpers)

**Namespace:** `dawchecker::security`

| API | Purpose |
|-----|---------|
| `checkedMul` / `checkedAdd` | Overflow-safe size arithmetic |
| `inBounds` | Index checks |
| `isSafeExportPath` | Reject `..` traversal on export paths |
| `secureZero` / `ScopedSecureBuffer` | Non-elidable buffer wipe |
| `runStartupSelfCheck` | Informational ASLR / debugger / elevation report |

---

## 4. GUI Application

**Namespace:** `dawchecker::gui`  
**Entry:** `src/main.cpp` → `Application::run()`

| Panel | File | Role |
|-------|------|------|
| Application | `Application.cpp` | GLFW window, ImGui, audio stream PIMPL, main loop |
| DashboardPanel | `DashboardPanel.cpp` | Master tamper score, detector rows |
| CodecPanel | `CodecPanel.cpp` | Device/codec findings |
| ScanControls | `ScanControls.cpp` | Sensitivity, neutralizer toggles, rescan |
| SpectrumView | `SpectrumView.cpp` | GUI-thread FFT display (not on audio thread) |
| Theme | `Theme.cpp` | Palette + ImGui style |

### 4.1 Visual language

| Token | Approx. RGB | Meaning |
|-------|-------------|---------|
| `bgDeep` | #12141A | App background |
| `accentSafe` | teal | Clean / low score |
| `accentInfo` | blue | Active / info |
| `accentCaution` | amber | Attention |
| `accentAlert` | red | Tampering / alert only |

Standalone audio score labels: Clean ≤20 · Low-Level Artifacts ≤50 · Attention Recommended ≤80 · Tampering Detected >80. These are heuristic UI labels, not validated diagnostic thresholds.

### 4.2 Dependencies

- C++20, CMake ≥ 3.16  
- GLFW3, OpenGL  
- Dear ImGui (vendored under `third_party/imgui`)  
- Optional: PortAudio  

---

## 5. VST3 plugin component

**Files:** `Source/dawchecker_vst3.cpp`, `latest/dawchecker_vst3.cpp`

The VST3 plugin component is implemented as a Steinberg `SingleComponentEffect`:

- Shared fixed-storage feature pipeline (512-sample frame, 128-sample hop, bounded 16,384-sample ring)
- Preallocated radix-2 FFT workspace; no heap allocation in the pipeline write/process methods
- Logistic mapping to a bounded heuristic score; not a calibrated probability or posterior
- Bounded SPSC event queue; evidence-graph allocations are moved to a non-audio worker
- Mono pass-through, stereo processing, and safe fallback for mismatched or null channel buffers
- Persistent bounded 80 Hz phase accumulator; phase advances independently of tone audibility
- SHA-256 is provided by the separate `dawchecker_crypto` target; this plugin translation unit does not currently expose a hashing operation
- No digital-signature generation or signer-identity claim; a digest alone does not prove provenance
- Parameters: sensitivity, fusion threshold, anti-forensic gate

**Factory:** class registration and UID are declared in the translation unit. This metadata has not been validated by an SDK-backed build.

**Verification boundary:** CI builds and tests the extracted core pipeline and queue, including allocation-counter checks. It does not compile the VST3 translation unit. Build of VST3 requires the Steinberg VST3 SDK (not bundled in the standalone CMake app path), and the current CMake project still has no VST3 consumer target. The SDK-backed target must include `src/`, link `dawchecker_core` and the appropriate Steinberg SDK targets, and pass an SDK-backed build before VST3 integration is release-verified. Link `dawchecker_crypto` only if a future plugin interface actually uses the hash API.

---

## 6. Security Hardening

### 6.1 Compiler / linker (standalone)

From `CMakeLists.txt` / `scripts/build_hardened.sh` (GCC/Clang):

- `-fstack-protector-strong`; non-Apple Unix builds also use `-fPIE`, while Release executable link flags include `-pie`, `-Wl,-z,relro,-z,now`, and `-Wl,-z,noexecstack`. The script reports ELF hardening checks on a best-effort basis; it does not fail the build when those checks are absent.
- `-D_FORTIFY_SOURCE=2` (non-Debug)
- Optional: `-fstack-clash-protection`, `-fcf-protection=full`
- `scripts/build_hardened.sh` writes a sibling `dawchecker_secure.sha256` only after build and registered tests succeed; direct CMake builds do not generate it.

MSVC path: `/GS`, `/sdl`, `/GUARD:CF`, `/DYNAMICBASE`, `/NXCOMPAT`, `/HIGHENTROPYVA`.

### 6.2 Policy (audio app)

Documented in `docs/HARDENING.md`:

- No runtime network calls  
- No license/phone-home  
- Startup self-check is **informational only** (does not exit on debugger)

---

## 7. Build System

### 7.1 Standalone application

```bash
./scripts/vendor_imgui.sh
./scripts/build_hardened.sh
# artifacts: build/dawchecker_secure, build/engine_tests, build/audio_pipeline_tests, build/evidence_accumulator_tests
```

| CMake target | Description |
|--------------|-------------|
| `dawchecker_core` | Static core library |
| `dawchecker_gui` | GUI + ImGui (if ImGui present) |
| `dawchecker_secure` | Main executable |
| `engine_tests` | Core engine tests |
| `audio_pipeline_tests` | Shared feature pipeline and SPSC queue tests |
| `evidence_accumulator_tests` | Evidence score accumulation tests |
| `dawchecker_crypto` | OpenSSL EVP SHA-256 implementation (when crypto gate is enabled) |

ImGui missing → core + tests still build; GUI skipped with warning.

### 7.2 Options

| Option / define | Effect |
|-----------------|--------|
| `DAWCHECKER_USE_PORTAUDIO` | Prefer PortAudio for capture |
| `DAWCHECKER_HAVE_PORTAUDIO` | Compile-time enable |
| `DAWCHECKER_HAVE_ALSA` | Linux device fallback |
| `DAWCHECKER_PLATFORM_*` | Windows / macOS / Linux |

---

## 8. Testing

**Files:** `tests/EngineTests.cpp`, `tests/SharedFeaturePipelineTests.cpp`, `tests/EvidenceAccumulatorTests.cpp`  
**Runner:** CTest (`engine_tests`, `audio_pipeline_tests`, `evidence_accumulator_tests`)

Coverage includes:

- Silence → low tamper score, not compromised
- 19.2 kHz tone → ultrasonic confidence rises
- `checkedMul` overflow detection and `isSafeExportPath` traversal rejection
- `CodecScanner::scan()` smoke execution
- Shared feature frame/hop/ring behavior, low/high-band FFT separation,
  non-finite input sanitization, and allocation-counter checks
- SPSC queue capacity, FIFO ordering, concurrent producer/consumer behavior,
  and allocation-free enqueue/dequeue
- Evidence accumulator score-band and relationship behavior

No GoogleTest dependency (minimal harness).

---

## 9. Server Protection Pro

**Root:** `tools/server-protection/`  
**Install prefix:** `/opt/server-protection`  
**Service:** `server-protection.service`  
**Bind:** `127.0.0.1:8080` (override with `SPP_HOST` / `SPP_PORT`)

### 9.1 Components

| File | Role |
|------|------|
| `server_protection_api.py` | Threading HTTP server + JSON API + static dashboard |
| `dashboard/index.html` | Operator UI; lists and actions are API-backed, while host/device integrity data not exposed by the API is shown as unavailable |
| `msv-blocklist-update.sh` | CSF blocklist definition updater |
| `install.sh` | Packages, CSF, ipset config, cron, systemd |
| `run_dashboard.sh` | Foreground start with CSF preflight |
| `monitor.sh` | CLI status |
| `scripts/*.sh` | ClamAV, rkhunter, AIDE, csf-sync wrappers |

### 9.2 Install

```bash
cd tools/server-protection
sudo ./install.sh
```

Effects: CSF if missing; `LF_IPSET=1`; files under `/opt/server-protection`; symlinks in `/usr/local/bin`; cron every 2h for MSV; systemd enable+start.

### 9.3 Production constraints

- Process **exits** if `csf` is not on `PATH`  
- Mutations require root when `SPP_REQUIRE_ROOT=1` (default)  
- No simulated success for missing ClamAV/rkhunter  

---

## 10. HTTP API Reference

Base URL: `http://127.0.0.1:8080/api`

| Method | Path | Body | Description |
|--------|------|------|-------------|
| GET | `/status` | — | CSF deny-file count, named ipset counts, jobs, root flag; no risk score is calculated |
| GET | `/bans` | — | Entries from `/etc/csf/csf.deny` |
| GET | `/whitelist` | — | Entries from `/etc/csf/csf.allow` |
| GET | `/logs` | — | Recent API log ring buffer |
| GET | `/jobs` | — | Background job states |
| POST | `/unblock` | `{"ip":"..."}` | `csf -dr` then `csf -a` |
| POST | `/whitelist` | `{"entry":"..."}` | `csf -a` |
| DELETE | `/whitelist` | `{"entry":"..."}` | `csf -ar` |
| POST | `/refresh` | `{}` | `csf -r` |
| POST | `/scan/clamav` | `{}` | Async ClamAV job |
| POST | `/scan/rkhunter` | `{}` | Async rkhunter job |
| POST | `/panic` | `{}` | CSF reload plus scan/update job attempts; reports partial failure rather than unconditional success |
| POST | `/blocklist/update` | `{}` | Run `msv-blocklist-update.sh` |

**Status codes:** `200` ok · `400` bad input · `403` need root · `503` CSF/tool missing · `500` execution failure.

**Validation:** IPv4, optional CIDR, or domain; rejects empty/overlong strings.

---

## 11. MSV Blocklist Integration

### 11.1 CSF format (mandatory)

```text
NAME|INTERVAL|MAX|URL
```

| Field | Rules |
|-------|--------|
| NAME | Uppercase, ≤25 chars, used as chain/set identity |
| INTERVAL | Seconds, minimum 3600 recommended |
| MAX | Cap on entries used; `0` = all |
| URL | HTTP(S) list of IP/CIDR per line |

**Do not** append raw IP dumps into `csf.blocklists`. CSF downloads URLs itself after `csf -ra`.

### 11.2 Default sources

| NAME | INTERVAL | MAX | Source |
|------|----------|-----|--------|
| BITWIRE_IN | 7200 | 20000 | bitwire-it/ipblocklist inbound |
| IPSUM_L3 | 86400 | 0 | stamparm/ipsum levels/3.txt |

### 11.3 Updater algorithm

1. Root + CSF checks  
2. Backup `/etc/csf/csf.blocklists`  
3. Strip previous `BITWIRE_IN` / `IPSUM_L3` lines  
4. Append definition lines only  
5. Delete `/var/lib/csf/csf.block.BITWIRE_IN` and `...IPSUM_L3`  
6. `csf -ra` (restore backup on failure)  
7. Optional Googlebot allow ranges  
8. Log + ipset/CSF stats  

---

## 12. Operational Procedures

### 12.1 Build workstation binary

```bash
./scripts/vendor_imgui.sh
./scripts/build_hardened.sh
./build/dawchecker_secure
```

### 12.2 Deploy server toolkit

```bash
sudo ./tools/server-protection/install.sh
sudo msv-monitor.sh
curl -s http://127.0.0.1:8080/api/status | jq .
```

### 12.3 Emergency (server)

- UI **PANIC** or `POST /api/panic` → CSF reload + scanner jobs + MSV update attempt  
- Console: `csf -r` / `csf -ra`; verify SSH allow before remote restart  

### 12.4 Forensic use (audio)

Treat automated scores as **advisory decision-support signals**, not findings of authenticity or tampering. Preserve originals; export only via validated paths; record tool version and binary `.sha256` with case notes. A checksum can detect byte changes relative to a trusted digest but does not independently establish provenance or chain of custody.

---

## 13. Threat Model & Limitations

### 13.1 Audio engine

- Not a substitute for full lab chain-of-custody or expert testimony  
- Ultrasonic/ENF/watermark signals depend on capture bandwidth and prior processing  
- Neutralizers alter the signal — document when enabled  
- VST3 SHA-256 digest is not a digital signature and does not authenticate a signer  

### 13.2 Server toolkit

- Third-party IP lists cause **false positives**  
- Compromised list upstream is a supply-chain risk (no signature verification on feeds)  
- Large sets require ipset (`LF_IPSET=1`) and adequate `maxelem`  
- Dashboard is not an internet-facing multi-user IAM system — loopback only  

### 13.3 Explicit non-goals

- No kernel rootkit beyond invoking rkhunter/AIDE  
- No mobile MDM enforcement in the API  
- No coupling of audio GUI to CSF  

---

## 14. Glossary

| Term | Meaning |
|------|---------|
| **MSV** | Multi-Spectral / modular forensic engine brand used in audio core and server toolkit naming |
| **ENF** | Electric Network Frequency analysis |
| **CSF** | ConfigServer Security & Firewall |
| **ipset** | Kernel set-based matching used by CSF for large lists |
| **LFD** | Login Failure Daemon (CSF companion) |
| **Logistic score mapping** | Heuristic mapping of extracted features into a bounded score; not validated calibration or posterior probability |
| **Neutralizer** | Optional DSP stage that attenuates a detected class of artifact |
| **Hermetic DSP** | Analysis path minimizing external binary dependencies |

---

*End of Technical Reference — DawChecker v3.0.0*

---

## Appendix A — Advanced Clipboard MCP Server

**Path:** `tools/clipboard-mcp/`

Optional stdio MCP server for system clipboard access from AI hosts. Independent process; not linked into `dawchecker_secure`.

| Item | Detail |
|------|--------|
| Entry | `clipboard_server_advanced.py` |
| Deps | `mcp[cli]`, `pyperclip`; Linux `xclip`/`xsel` |
| Config | `clipboard_advanced_config.json` |
| Consent | Required (`consent_given` or GUI dialog) |
| Sensitive patterns | SSN, card-like 16-digit, passwords, API keys, PEM keys, common cloud tokens |

Install: `./install_clipboard_server.sh` · Run: `./start_clipboard_server.sh`
