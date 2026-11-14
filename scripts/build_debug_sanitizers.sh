#!/usr/bin/env bash
# Debug build with ASan + UBSan (development only — not for production ship)
set -euo pipefail
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_ROOT}"
BUILD_DIR="${PROJECT_ROOT}/build-asan"
JOBS="$(nproc 2>/dev/null || echo 4)"

echo "=== DawChecker — Debug + Address/UB Sanitizers ==="
./scripts/vendor_imgui.sh

cmake -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DDAWCHECKER_USE_PORTAUDIO=ON

cmake --build "${BUILD_DIR}" --parallel "${JOBS}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure -V

echo "Sanitizer build OK → ${BUILD_DIR}"
