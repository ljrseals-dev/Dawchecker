#!/usr/bin/env bash
# Vendors a pinned release of Dear ImGui into third_party/imgui.
# Run once before the first CMake configure, or any time you want to refresh.
set -euo pipefail

IMGUI_VERSION="v1.90.9"
IMGUI_DIR="third_party/imgui"
REPO_URL="https://github.com/ocornut/imgui.git"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

if [[ -f "${IMGUI_DIR}/imgui.h" ]]; then
    echo "ImGui already present at ${IMGUI_DIR}"
    # Quick sanity: required backends must exist
    for f in backends/imgui_impl_glfw.cpp backends/imgui_impl_opengl3.cpp; do
        if [[ ! -f "${IMGUI_DIR}/${f}" ]]; then
            echo "WARNING: missing ${IMGUI_DIR}/${f} — re-vendoring" >&2
            rm -rf "${IMGUI_DIR}"
            break
        fi
    done
    if [[ -f "${IMGUI_DIR}/imgui.h" ]]; then
        echo "Backends OK. Nothing to do."
        exit 0
    fi
fi

echo "Vendoring Dear ImGui ${IMGUI_VERSION} into ${IMGUI_DIR} ..."
mkdir -p third_party

if command -v git >/dev/null 2>&1; then
    git clone --depth 1 --branch "${IMGUI_VERSION}" "${REPO_URL}" "${IMGUI_DIR}"
else
    echo "git not found; attempting tarball download..."
    TARBALL_URL="https://github.com/ocornut/imgui/archive/refs/tags/${IMGUI_VERSION}.tar.gz"
    mkdir -p "${IMGUI_DIR}"
    curl -fsSL "${TARBALL_URL}" | tar -xz --strip-components=1 -C "${IMGUI_DIR}"
fi

if [[ ! -f "${IMGUI_DIR}/imgui.h" ]]; then
    echo "ERROR: ImGui vendor failed — imgui.h not found" >&2
    exit 1
fi

echo "✓ ImGui ${IMGUI_VERSION} vendored successfully"
echo "  Required backends:"
ls "${IMGUI_DIR}/backends/" | grep -E "glfw|opengl3" || true
