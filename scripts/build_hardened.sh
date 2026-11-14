#!/usr/bin/env bash
# ============================================================================
# build_hardened.sh — Production hardened build for DawChecker Secure Ultimate
#
# - Vendors ImGui if missing
# - Configures CMake Release with full hardening
# - Builds dawchecker_secure + engine_tests
# - Runs tests
# - Verifies PIE / RELRO / NX (best-effort, Linux)
# - Emits sibling .sha256 for chain-of-custody
# ============================================================================
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_ROOT}"

BUILD_DIR="${PROJECT_ROOT}/build"
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
USE_PORTAUDIO="${USE_PORTAUDIO:-ON}"

echo "============================================================"
echo "  DawChecker Secure Ultimate — Hardened Build"
echo "  Root: ${PROJECT_ROOT}"
echo "  Type: ${BUILD_TYPE}"
echo "============================================================"
echo

# ---------------------------------------------------------------------------
# 1. Vendor Dear ImGui
# ---------------------------------------------------------------------------
echo "→ Ensuring ImGui is vendored..."
chmod +x scripts/vendor_imgui.sh
./scripts/vendor_imgui.sh
echo

# ---------------------------------------------------------------------------
# 2. Configure
# ---------------------------------------------------------------------------
echo "→ Configuring CMake (${BUILD_TYPE} + hardening)..."
cmake -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DDAWCHECKER_USE_PORTAUDIO="${USE_PORTAUDIO}"
echo

# ---------------------------------------------------------------------------
# 3. Build
# ---------------------------------------------------------------------------
# Invalidate every known output sidecar before rebuilding. A failed build
# must never leave an old checksum looking like evidence for a new binary.
find "${BUILD_DIR}" -type f -name 'dawchecker_secure.sha256' -delete

echo "→ Building (jobs=${JOBS})..."
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"
echo

# ---------------------------------------------------------------------------
# 4. Test
# ---------------------------------------------------------------------------
echo "→ Checking that CTest registered at least one test..."
TEST_LIST="$(ctest --test-dir "${BUILD_DIR}" -N)"
printf '%s\\n' "${TEST_LIST}"
if ! grep -Eq 'Total Tests: [1-9][0-9]*' <<<"${TEST_LIST}"; then
    echo "ERROR: no tests are registered; refusing to report a successful build." >&2
    exit 1
fi

echo "→ Running engine tests..."
ctest --test-dir "${BUILD_DIR}" --output-on-failure -V
echo

# ---------------------------------------------------------------------------
# 5. Locate binary
# ---------------------------------------------------------------------------
BIN=""
for candidate in \
    "${BUILD_DIR}/dawchecker_secure" \
    "${BUILD_DIR}/Release/dawchecker_secure" \
    "${BUILD_DIR}/bin/dawchecker_secure"
do
    if [[ -x "${candidate}" ]]; then
        BIN="${candidate}"
        break
    fi
done

if [[ -z "${BIN}" ]]; then
    # GUI may have been skipped (no ImGui/GLFW) — still success if tests passed
    echo "⚠  dawchecker_secure binary not found (GUI may be disabled)."
    echo "   Core library and engine_tests built successfully."
    exit 0
fi

echo "→ Binary: ${BIN}"

# ---------------------------------------------------------------------------
# 6. Integrity sidecar
# ---------------------------------------------------------------------------
CHECKSUM_TMP="${BIN}.sha256.tmp.$"
cleanup_checksum() {
    rm -f -- "${CHECKSUM_TMP}"
}
trap cleanup_checksum EXIT

if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "${BIN}" > "${CHECKSUM_TMP}"
elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "${BIN}" > "${CHECKSUM_TMP}"
else
    echo "ERROR: no SHA-256 utility is available; refusing to leave an unverified binary." >&2
    exit 1
fi

# Publish the sidecar atomically, only after build and tests have succeeded.
mv -f -- "${CHECKSUM_TMP}" "${BIN}.sha256"
echo "  [OK] Integrity sidecar: ${BIN}.sha256"
echo

# ---------------------------------------------------------------------------
# 7. Hardening verification (Linux ELF, best-effort)
# ---------------------------------------------------------------------------
if command -v readelf >/dev/null 2>&1 && [[ "$(uname -s)" == "Linux" ]]; then
    echo "→ Hardening checks (readelf)..."
    # PIE / dynamic type
    if readelf -h "${BIN}" 2>/dev/null | grep -qi "Type:.*DYN"; then
        echo "  [OK] PIE (DYN)"
    else
        echo "  [!!] Not DYN/PIE — check -fPIE / -pie"
    fi
    # NX stack
    if readelf -l "${BIN}" 2>/dev/null | grep -A1 "GNU_STACK" | grep -q "RWE"; then
        echo "  [!!] Stack is executable (unexpected)"
    else
        echo "  [OK] NX stack"
    fi
    # Full RELRO
    if readelf -d "${BIN}" 2>/dev/null | grep -q "BIND_NOW"; then
        echo "  [OK] BIND_NOW (full RELRO likely)"
    else
        echo "  [!!] BIND_NOW not seen — partial RELRO?"
    fi
    echo
fi

if command -v checksec >/dev/null 2>&1; then
    echo "→ checksec:"
    checksec --file="${BIN}" || true
    echo
fi

echo "============================================================"
echo "  BUILD COMPLETE"
echo "  Binary:  ${BIN}"
echo "  Sidecar: ${BIN}.sha256"
echo "============================================================"
