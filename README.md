# DawChecker Secure Ultimate (VST3 Edition)

## Overview

**DawChecker Secure Ultimate** is a high-fidelity, production-hardened real-time VST3 forensic audio analysis platform. Built specifically for court-defensible digital audio forensics, chain-of-custody preservation, and signal authentication, the system bypasses fragile single-signature heuristics. Instead, it utilizes a multi-layered detection matrix combined via a **Platt-scaled Logistic Evidence Fusion Engine** to quantify tampering probability with absolute mathematical rigor.

This repository hosts the ultra-optimized monolithic compilation target, unifying the real-time Short-Time Fourier Transform (STFT) pipeline, phase coherence analysis, sub-harmonic telemetry reporting, and parameter state management into a single, isolated execution unit.

---

## Architectural Pillars

```
+-----------------------------------------------------------------------------+
|                                AUDIO RECON                                  |
|  +---------------------------+     +-------------------------------------+  |
|  |   Shared STFT Pipeline    | --> | Magnitude-Weighted Phase Coherence  |  |
|  +---------------------------+     +-------------------------------------+  |
|                                       |                                     |
|                                       v                                     |
|                    +-------------------------------------+                  |
|                    |  Platt-Scaled Logistic Regression   |                  |
|                    +-------------------------------------+                  |
|                                       |                                     |
|                                       v                                     |
|  +---------------------------+     +-------------------------------------+  |
|  | Real-Time Sub-Harmonic   | <-- |   Bayesian Evidence Calibration     |  |
|  |     Telemetry Output      |     |               Engine                |  |
|  +---------------------------+     +-------------------------------------+  |
+-----------------------------------------------------------------------------+

```

### 1. Shared Feature Pipeline & Hermetic DSP

The engine runs a localized $O(N^2)$ Discrete Fourier Transform implementation designed to maintain complete runtime isolation. By avoiding external linkage dependencies (such as standard FFTW or Intel IPP libraries), it eliminates side-channel vulnerabilities and potential compilation shifts across target environments.

### 2. Magnitude-Weighted Phase Coherence

Tracks cross-bin phase velocity variations and instantaneous dispersion. Splices, edits, or deepfake vocoder rendering boundaries introduce non-linear phase randomization; the analyzer flags these micro-discontinuities against a continuous exponential decay tracking window.

### 3. Platt-Scaled Evidence Fusion

Translates structural high-frequency anomalies, noise-floor variations, and phase dispersion into a calibrated posterior probability. Rather than relying on arbitrary cutoff scores, it scales multidimensional features into a strict $[0.0, 1.0]$ probability scale using localized Platt-regression coefficients ($a = -2.4$, $b = 0.35$).

### 4. Video PRNU Profiling Extension

Fully prepared to ingest 2D Photo Response Non-Uniformity matrices. It extracts high-frequency sensor noise residuals via a 2D adaptive Wiener filter mask, allowing developers to correlate audio phase drops with frame-level sensor mismatches for unified video/audio tampering localization.

---

## Technical Specifications

| Parameter | Specification |
| --- | --- |
| **Plugin Format** | VST3 (SingleComponentEffect Architecture) |
| **C++ Standard** | C++20 |
| **Default Window Size** | 512 samples (Hann windowing) |
| **Default Hop Size** | 128 samples (75% overlap) |
| **Processing Path** | Hardened 32-bit floating-point SIMD-ready buffers |
| **Output Signalling** | Real-time dual-phase sub-harmonic signaling ($80\text{ Hz}$ at $-34\text{ dBFS}$) |

---

## Parameter Mapping Matrix

* **Forensic Sensitivity (`kParamSensitivity`)** `[ID: 0, Range: 0.0 - 1.0]`: Adjusts the scaling multiplier for high-frequency spectral anomaly tracking. Heightens vigilance in low-amplitude or heavily compressed noise floors.
* **Fusion Model Threshold (`kParamFusionThreshold`)** `[ID: 1, Range: 0.0 - 1.0]`: Sets the critical Platt-scaling logistic limit. Crossing this boundary triggers immediate anomaly logging and activates the defensive telemetry subsystem.
* **Anti-Forensic Gate (`kParamAntiForensicGate`)** `[ID: 2, Range: 0.0 - 1.0]`: Determines the noise-floor variance tolerance baseline, preventing adversarial noise-injection (dithering masks, spectral painting) from blinding the structural analysis blocks.

---

## Compilation & Deployment Hardening

The codebase enforces strict memory security bounds and binary defenses to resist exploitation or reverse-engineering in hostile analytical conditions.

### Compilation Flags & Binary Specifications

#### For Linux / macOS (GCC / Clang):

```bash
-Wall -Wextra -Werror -fstack-protector-strong -fPIE -D_FORTIFY_SOURCE=2 -O2 -pie -z relro -z now

```

* `-fstack-protector-strong`: Implements structural stack canary verification around local buffers.
* `-fPIE` / `-pie`: Forces Position-Independent Executable generation for full Address Space Layout Randomization (ASLR) compatibility.
* `-z relro -z now`: Hardens ELF headers, forcing immediate resolution of dynamic symbols to render the Global Offset Table (GOT) strictly read-only.

#### For Windows (MSVC):

```cmd
/W4 /WX /GS /sdl /guard:cf /analyze /GUARD:CF /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA

```

* `/guard:cf` & `/GUARD:CF`: Compiler and linker-level Control Flow Guard (CFG) verification to arrest indirect call hijacking.
* `/HIGHENTROPYVA`: Forces the operating system to apply 64-bit high-entropy ASLR for security boundary separation.

### Automated Hardened Build Deployment

To compile the production-ready binary, navigate to the project directory and execute the hardened build toolchain script:

```bash
chmod +x scripts/build_hardened.sh
./scripts/build_hardened.sh

```

---

## Verification & Forensic Chain-of-Custody

When integrating into automated legal processing stations or multi-modal forensic engines:

1. **Telemetry Capture:** Monitor the plugin’s output bus for inverted phase $80\text{ Hz}$ sub-harmonics during suspicious segments. This allows downstream recording hardware to print an unalterable forensic alarm trace directly into the audio print.
2. **Graph Injection:** Ensure your parent environment implements `dawchecker::dsp::EvidenceGraph`. Real-time findings append directly into the DAG with immutable weights, preserving the prior probabilities necessary for structural Bayesian courtroom presentation.

---



---



## Optional: Advanced Clipboard MCP Server

Developer companion for AI/MCP hosts (not part of the forensic audio binary).

```bash
cd tools/clipboard-mcp
./install_clipboard_server.sh   # creates .venv + deps
./start_clipboard_server.sh
# Claude Code:
# claude mcp add clipboard-advanced -- .venv/bin/python clipboard_server_advanced.py
```

See [tools/clipboard-mcp/README.md](tools/clipboard-mcp/README.md).

## Technical Reference

Full architecture, API, and operations: [docs/TECHNICAL_REFERENCE.md](docs/TECHNICAL_REFERENCE.md)

---

## Optional: Server Protection Toolkit

Companion scripts under `tools/server-protection/` for **Linux server** hardening (CSF + ipset blocklists). They are **not** part of the desktop/VST3 application and do not add network behavior to DawChecker.

| Script | Purpose |
|--------|---------|
| `msv-blocklist-update.sh` | Correct CSF blocklist updater (definition lines only) |
| `install.sh` | Install CSF/ipset config, cron, and updater |
| `monitor.sh` | Status report |

```bash
cd tools/server-protection
sudo ./install.sh
sudo /usr/local/bin/msv-blocklist-update.sh   # manual refresh
sudo /usr/local/bin/msv-monitor.sh            # status
```

See [tools/server-protection/README.md](tools/server-protection/README.md) for sources, safety notes, and design constraints. The audio application remains offline (no telemetry, no firewall control from the GUI).

## License & Attribution

* **Developer:** Lewis James Seals Jr (**TheRealla**)
* **Organization:** TheRealla Audio Forensic Labs
* **Dedicated to:** Honoring the technical history of the Seals family.

*Disclaimer: This tool is intended for expert audio engineers, provenance researchers, and digital forensic examiners. All automated conclusions should be contextualized alongside a comprehensive human review.*
