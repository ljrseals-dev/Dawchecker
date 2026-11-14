#!/usr/bin/env bash
set -euo pipefail
export BUILD_FAIL=0 TEST_COUNT=1 CTEST_EXIT=0

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
MOCKBIN="$TMP/mockbin"
mkdir -p "$MOCKBIN"

cat > "$MOCKBIN/cmake" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
if [[ "$1" == "-B" ]]; then
  mkdir -p "$2"
  exit 0
fi
if [[ "$1" == "--build" ]]; then
  mkdir -p "$2"
  if [[ "$BUILD_FAIL" == "1" ]]; then exit 9; fi
  printf 'test executable placeholder\n' > "$2/dawchecker_secure"
  chmod +x "$2/dawchecker_secure"
  exit 0
fi
exit 0
SH

cat > "$MOCKBIN/ctest" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
if [[ "$1" == "--test-dir" ]]; then shift 2; fi
if [[ "$1" == "-N" ]]; then
  printf 'Total Tests: %s\n' "$TEST_COUNT"
  exit 0
fi
if [[ "$1" == "--output-on-failure" ]]; then
  exit "$CTEST_EXIT"
fi
exit 0
SH
chmod +x "$MOCKBIN/cmake" "$MOCKBIN/ctest"

prepare_project() {
  local name="$1"
  local project="$TMP/$name"
  mkdir -p "$project/scripts" "$project/build"
  cp "$ROOT/scripts/build_hardened.sh" "$project/scripts/build_hardened.sh"
  cat > "$project/scripts/vendor_imgui.sh" <<'SH'
#!/usr/bin/env bash
exit 0
SH
  chmod +x "$project/scripts/vendor_imgui.sh"
  printf 'stale checksum\n' > "$project/build/dawchecker_secure.sha256"
  printf '%s\n' "$project"
}

zero_project="$(prepare_project zero-tests)"
if (cd "$zero_project" && PATH="$MOCKBIN:$PATH" TEST_COUNT=0 CTEST_EXIT=0 ./scripts/build_hardened.sh); then
  echo "zero-test build unexpectedly succeeded" >&2
  exit 1
fi
test ! -e "$zero_project/build/dawchecker_secure.sha256"
! compgen -G "$zero_project/build/*.sha256.tmp.*" >/dev/null

failed_test_project="$(prepare_project failed-tests)"
if (cd "$failed_test_project" && PATH="$MOCKBIN:$PATH" TEST_COUNT=1 CTEST_EXIT=7 ./scripts/build_hardened.sh); then
  echo "failing CTest unexpectedly succeeded" >&2
  exit 1
fi
test ! -e "$failed_test_project/build/dawchecker_secure.sha256"
! compgen -G "$failed_test_project/build/*.sha256.tmp.*" >/dev/null

failed_build_project="$(prepare_project failed-build)"
if (cd "$failed_build_project" && PATH="$MOCKBIN:$PATH" BUILD_FAIL=1 TEST_COUNT=1 ./scripts/build_hardened.sh); then
  echo "failing build unexpectedly succeeded" >&2
  exit 1
fi
test ! -e "$failed_build_project/build/dawchecker_secure.sha256"

success_project="$(prepare_project successful-build)"
(cd "$success_project" && PATH="$MOCKBIN:$PATH" TEST_COUNT=2 CTEST_EXIT=0 ./scripts/build_hardened.sh)
test -s "$success_project/build/dawchecker_secure.sha256"
! compgen -G "$success_project/build/*.sha256.tmp.*" >/dev/null
(cd "$success_project/build" && sha256sum -c dawchecker_secure.sha256)

echo "Hardened build contract tests passed."
