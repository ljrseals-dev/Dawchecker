#!/usr/bin/env bash
# Regression coverage for missing cron/systemd and cron idempotence.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALLER="$ROOT/tools/server-protection/install.sh"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/empty" "$TMP/mockbin"
cat > "$TMP/empty/dirname" <<'SH'
#!/bin/sh
exec /usr/bin/dirname "$@"
SH
chmod +x "$TMP/empty/dirname"
cat > "$TMP/empty/grep" <<'SH'
#!/bin/sh
exec /usr/bin/grep "$@"
SH
chmod +x "$TMP/empty/grep"
cat > "$TMP/empty/cat" <<'SH'
#!/bin/sh
exec /usr/bin/cat "$@"
SH
chmod +x "$TMP/empty/cat"
cat > "$TMP/empty/date" <<'SH'
#!/bin/sh
exec /usr/bin/date "$@"
SH
chmod +x "$TMP/empty/date"

# Missing cron and systemd tools must be a warning-only, successful path.
missing_output="$(
  PATH="$TMP/empty" SPP_INSTALLER_LIB_ONLY=1 /bin/bash -c '
    source "$1"
    setup_cron
    setup_systemd
  ' _ "$INSTALLER" 2>&1
)"
grep -q 'crontab is unavailable' <<<"$missing_output"
grep -q 'systemd is unavailable' <<<"$missing_output"

# A missing user crontab must be treated as an empty crontab, not a fatal error.
cat > "$TMP/mockbin/crontab" <<'SH'
#!/bin/sh
case "$1" in
  -l)
    if [ "${CRON_HAS_ENTRY:-0}" = "1" ]; then
      echo '0 */2 * * * /usr/local/bin/msv-blocklist-update.sh'
      exit 0
    fi
    exit 1
    ;;
  -)
    cat > "$CRON_CAPTURE"
    ;;
  *)
    exit 2
    ;;
esac
SH
chmod +x "$TMP/mockbin/crontab"

CRON_CAPTURE="$TMP/cron.new" CRON_HAS_ENTRY=0 PATH="$TMP/mockbin:$TMP/empty" \
  SPP_INSTALLER_LIB_ONLY=1 /bin/bash -c 'source "$1"; setup_cron' _ "$INSTALLER"
grep -q '^0 \*/2 \* \* \* /usr/local/bin/msv-blocklist-update.sh ' "$TMP/cron.new"

# Existing entries must not be duplicated or rewritten.
rm -f "$TMP/cron.duplicate"
CRON_CAPTURE="$TMP/cron.duplicate" CRON_HAS_ENTRY=1 PATH="$TMP/mockbin:$TMP/empty" \
  SPP_INSTALLER_LIB_ONLY=1 /bin/bash -c 'source "$1"; setup_cron' _ "$INSTALLER"
test ! -e "$TMP/cron.duplicate"

# Required package installation failures must abort instead of being hidden.
cat > "$TMP/mockbin/apt-get" <<'SH'
#!/bin/sh
case "$1" in
  update) exit "${APT_UPDATE_FAIL:-0}" ;;
  install)
    case " $* " in
      *" clamav "*) exit "${APT_OPTIONAL_FAIL:-0}" ;;
      *) exit "${APT_REQUIRED_FAIL:-0}" ;;
    esac
    ;;
  *) exit 2 ;;
esac
SH
chmod +x "$TMP/mockbin/apt-get"

if PATH="$TMP/mockbin:$TMP/empty" APT_REQUIRED_FAIL=8 \
  SPP_INSTALLER_LIB_ONLY=1 /bin/bash -c 'source "$1"; OS=ubuntu; install_pkgs' _ "$INSTALLER" \
  >"$TMP/required-package.out" 2>&1; then
  echo "installer ignored a required package failure" >&2
  exit 1
fi

optional_output="$(PATH="$TMP/mockbin:$TMP/empty" APT_OPTIONAL_FAIL=9 \
  SPP_INSTALLER_LIB_ONLY=1 /bin/bash -c 'source "$1"; OS=ubuntu; install_pkgs' _ "$INSTALLER" 2>&1)"
grep -q 'optional scanner packages could not be installed' <<<"$optional_output"

# CSF must not be downloaded/executed as root without an independently
# supplied digest, and a mismatched archive must fail before install.sh runs.
mkdir -p "$TMP/csf-source/csf" "$TMP/csf-mockbin"
cat > "$TMP/csf-source/csf/install.sh" <<'SH'
#!/bin/sh
touch "$CSF_TEST_MARKER"
cat > "$CSF_TEST_MOCKBIN/csf" <<'BIN'
#!/bin/sh
exit 0
BIN
chmod +x "$CSF_TEST_MOCKBIN/csf"
SH
tar -czf "$TMP/csf.tgz" -C "$TMP/csf-source" csf
cat > "$TMP/mockbin/wget" <<'SH'
#!/bin/sh
if [ "$1" = "-q" ] && [ "$2" = "-O" ]; then
  cp "$CSF_TEST_ARCHIVE" "$3"
  exit 0
fi
exit 2
SH
chmod +x "$TMP/mockbin/wget"

if env -u CSF_SHA256 CSF_TEST_ARCHIVE="$TMP/csf.tgz" \
  CSF_TEST_MARKER="$TMP/csf-missing-digest-ran" \
  CSF_TEST_MOCKBIN="$TMP/csf-mockbin" PATH="$TMP/mockbin:/usr/bin:/bin" \
  SPP_INSTALLER_LIB_ONLY=1 /bin/bash -c 'source "$1"; install_csf' _ "$INSTALLER" \
  >"$TMP/csf-missing-digest.out" 2>&1; then
  echo "CSF install unexpectedly proceeded without a digest" >&2
  exit 1
fi
test ! -e "$TMP/csf-missing-digest-ran"

if CSF_SHA256="not-a-sha256" CSF_TEST_ARCHIVE="$TMP/csf.tgz" \
  CSF_TEST_MARKER="$TMP/csf-invalid-digest-ran" \
  CSF_TEST_MOCKBIN="$TMP/csf-mockbin" PATH="$TMP/mockbin:/usr/bin:/bin" \
  SPP_INSTALLER_LIB_ONLY=1 /bin/bash -c 'source "$1"; install_csf' _ "$INSTALLER" \
  >"$TMP/csf-invalid-digest.out" 2>&1; then
  echo "CSF install unexpectedly accepted a malformed digest" >&2
  exit 1
fi
test ! -e "$TMP/csf-invalid-digest-ran"

CSF_EXPECTED="$(sha256sum "$TMP/csf.tgz" | awk '{print $1}')"
if CSF_SHA256="$(printf '%064d' 0)" CSF_TEST_ARCHIVE="$TMP/csf.tgz" \
  CSF_TEST_MARKER="$TMP/csf-mismatch-ran" \
  CSF_TEST_MOCKBIN="$TMP/csf-mockbin" PATH="$TMP/mockbin:/usr/bin:/bin" \
  SPP_INSTALLER_LIB_ONLY=1 /bin/bash -c 'source "$1"; install_csf' _ "$INSTALLER" \
  >"$TMP/csf-mismatch.out" 2>&1; then
  echo "CSF install unexpectedly accepted a mismatched digest" >&2
  exit 1
fi
test ! -e "$TMP/csf-mismatch-ran"

CSF_TEST_MARKER="$TMP/csf-valid-ran" CSF_TEST_MOCKBIN="$TMP/csf-mockbin" \
  CSF_TEST_ARCHIVE="$TMP/csf.tgz" CSF_SHA256="$CSF_EXPECTED" \
  PATH="$TMP/mockbin:$TMP/csf-mockbin:/usr/bin:/bin" SPP_INSTALLER_LIB_ONLY=1 \
  /bin/bash -c 'source "$1"; install_csf' _ "$INSTALLER"
test -f "$TMP/csf-valid-ran"
test -x "$TMP/csf-mockbin/csf"

# The executable installer must still reject non-root execution.
if bash "$INSTALLER" >"$TMP/root-check.out" 2>&1; then
  echo "installer unexpectedly ran without root" >&2
  exit 1
fi
grep -q 'Run as root' "$TMP/root-check.out"

echo "Installer contract tests passed."
