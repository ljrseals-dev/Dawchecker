#!/usr/bin/env bash
set -euxo pipefail

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

# The executable installer must still reject non-root execution.
if bash "$INSTALLER" >"$TMP/root-check.out" 2>&1; then
  echo "installer unexpectedly ran without root" >&2
  exit 1
fi
grep -q 'Run as root' "$TMP/root-check.out"

echo "Installer contract tests passed."
