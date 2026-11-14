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

Configured centrally in `CMakeLists.txt`'s `hardening_flags` target and
applied to every target in the project:

| Mitigation | GCC/Clang flag | MSVC flag | What it stops |
|---|---|---|---|
| Stack smashing detection | `-fstack-protector-strong` | `/GS` | Classic stack buffer overflows |
| Stack clash protection | `-fstack-clash-protection` | — | Stack/heap collision attacks |
| Fortified libc calls | `-D_FORTIFY_SOURCE=2` | `/sdl` | Buffer-overrun in `memcpy`/`sprintf`-family calls |
| Position-independent exec | `-fPIE -pie` | `/DYNAMICBASE` | Makes ASLR effective for the main binary |
| Full RELRO | `-Wl,-z,relro -Wl,-z,now` | — | GOT-overwrite exploitation |
| Non-executable stack | `-Wl,-z,noexecstack` | `/NXCOMPAT` | Stack-based shellcode execution |
| Control-flow integrity | `-fcf-protection=full` (Intel CET) | `/guard:cf`, `/CETCOMPAT` | ROP/JOP-style control-flow hijacking |
| High-entropy ASLR | (default on 64-bit) | `/HIGHENTROPYVA` | Address-guessing attacks |
| Format-string safety | `-Wformat -Werror=format-security` | `/W4` | `printf`-family format-string bugs |
| Spectre v1 mitigation | — | `/Qspectre` | Speculative-execution side channels |

Optional (`-DDAWCHECKER_ENABLE_SANITIZERS=ON`, debug/test builds only):
AddressSanitizer + UndefinedBehaviorSanitizer for development-time bug
catching. Never enable this for a release build you distribute — it changes
memory layout and adds overhead.

## Application-level hardening (runtime)

Implemented in `src/core/SecurityHardening.{h,cpp}`:

- **Checked arithmetic** (`checkedMul`, `checkedAdd`) for every buffer-size
  calculation in the DSP path, so a crafted or unusual sample-rate/block-size
  combination can't silently overflow a `size_t` the way it has in real
  audio-plugin CVEs.
- **Secure memory zeroing** (`secureZero`, `ScopedSecureBuffer`) for any
  buffer that briefly holds audio pulled from a file/device the user is
  specifically trying to scrub identifying watermark data from — zeroed in a
  way the optimizer can't elide.
- **Path validation** (`isSafeExportPath`) before writing any "neutralized"
  export next to a user-selected file, rejecting traversal sequences.
- **Read-only startup self-check** (`runStartupSelfCheck`) that reports — but
  never acts on — whether ASLR looks active at the OS level, whether a
  debugger is attached, and whether the process is running elevated. This is
  purely informational, shown once in the app's "Security Notes" panel. It
  never disables features, exits, or otherwise changes behavior based on
  these signals — a security tool that got twitchy around debuggers would be
  actively hostile to the security researchers who are its natural allies.
- **Build integrity manifest**: the build produces a sibling `.sha256` file
  next to the compiled binary so you (or anyone auditing a distributed copy)
  can verify it hasn't been altered after compilation.

## What this project deliberately does *not* do

- No kernel-mode drivers, no firmware writes, no Secure Boot key management.
- No anti-debugging *evasion* (only informational detection).
- No telemetry, no network calls of any kind at runtime — the only URL in the
  whole GUI is a plain "Get Latest Release" link the user has to click.
- No license-server / phone-home checks.
