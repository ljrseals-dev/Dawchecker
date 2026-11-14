# Security Hardening — What's Actually In This Build

A note on terminology first, because it affects what you should expect from
this project: hardening **CMOS/BIOS/UEFI firmware** (Secure Boot, TPM
attestation, firmware passwords, boot-order locking) happens in the
motherboard firmware setup screen or via OS-level tools like `bcdedit`
(Windows), `mokutil` (Linux Secure Boot), or Apple's `bless`/Startup Security
Utility — it is **not something an application binary can or should do**. Any
desktop app that tried to reach into firmware settings would itself be a
serious security red flag. If you want that layer hardened, do it directly in
your firmware setup screen; this document won't send you further down that
path.

What DawChecker Secure *does* hardened is the layer that's actually within an
application's control: the binary itself, and its own runtime behavior.

## Build-time (compiler/linker) hardening

CMake applies compiler and linker options directly; there is no separate
`hardening_flags` target.

For GCC/Clang, the current configuration enables:

- Warnings for common correctness issues, including conversion, shadowing,
  return-type, uninitialized-value, and format-security diagnostics.
- `-fstack-protector-strong`.
- `-fstack-clash-protection` and `-fcf-protection=full` when the compiler
  accepts those options.
- `-D_FORTIFY_SOURCE=2` outside Debug builds.
- On non-Apple Unix Release executables: `-fPIE`, `-pie`,
  `-Wl,-z,relro,-z,now`, and `-Wl,-z,noexecstack`.

For MSVC, CMake enables `/W4`, `/WX`, `/GS`, `/sdl`,
`/permissive-`, and `/Zc:__cplusplus`; executable link options include
`/DYNAMICBASE`, `/NXCOMPAT`, `/HIGHENTROPYVA`, and `/GUARD:CF`.

Development-only AddressSanitizer and UndefinedBehaviorSanitizer
instrumentation is available with
`-DDAWCHECKER_ENABLE_SANITIZERS=ON` on GCC/Clang. The
`scripts/build_debug_sanitizers.sh` helper enables this option. Do not
distribute sanitizer-instrumented builds as release binaries.

These settings reduce exposure to selected classes of implementation errors;
they do not prove that the application or resulting binary is secure. The
hardened build script's PIE/RELRO/NX checks are best-effort reports and do not
currently fail the build when a property is missing.

## Application-level hardening (runtime)

Implemented in `src/core/SecurityHardening.{h,cpp}`:

- **Checked arithmetic helpers** (`checkedMul`, `checkedAdd`) are
  implemented for callers that need overflow-safe size calculations. They are
  not currently wired into every allocation or buffer-size path, so no
  project-wide overflow-proof claim is made.
- **Secure memory zeroing utilities** (`secureZero`,
  `ScopedSecureBuffer`) are implemented. `ScopedSecureBuffer` wipes its own
  owned storage on destruction; this does not mean every audio, device, or
  analysis buffer in the application is automatically erased.
- **Path validation helper** (`isSafeExportPath`) rejects traversal-style
  paths. It is a utility for callers; this document does not claim a complete
  file-export workflow is currently wired to it.
- **Read-only startup self-check** (`runStartupSelfCheck`) that reports — but
  never acts on — whether ASLR looks active at the OS level, whether a
  debugger is attached, and whether the process is running elevated. This is
  purely informational, shown once in the app's "Security Notes" panel. It
  never disables features, exits, or otherwise changes behavior based on
  these signals — a security tool that got twitchy around debuggers would be
  actively hostile to the security researchers who are its natural allies.
- **Build integrity sidecar**: `scripts/build_hardened.sh` removes stale
  sidecars before dependency vendoring/configuration, requires at least one
  registered CTest test, runs the test suite, and writes a sibling `.sha256`
  file atomically only after build and tests succeed. A direct CMake build does
  not generate this sidecar. A checksum verifies bytes against a trusted
  reference; it is not a digital signature or chain-of-custody record.

## What this project deliberately does *not* do

- No kernel-mode drivers, no firmware writes, no Secure Boot key management.
- No anti-debugging *evasion* (only informational detection).
- No telemetry, no network calls of any kind at runtime — the only URL in the
  whole GUI is a plain "Get Latest Release" link the user has to click.
- No license-server / phone-home checks.
