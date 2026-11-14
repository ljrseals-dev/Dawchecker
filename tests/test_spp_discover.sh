#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Mock-only tests for tools/server-protection/spp-discover.sh.
# The child process receives a PATH containing only explicit stubs. OS release
# and systemd runtime paths are fixtures, never the test host's real paths.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRIPT="$ROOT/tools/server-protection/spp-discover.sh"
[ -f "$SCRIPT" ] || { printf 'missing %s\n' "$SCRIPT" >&2; exit 1; }

T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
BIN="$T/bin"
mkdir -p "$BIN" "$T/systemd"
printf 'ID=testos\nID_LIKE=test\nVERSION_ID=1\nPRETTY_NAME="Test OS 1"\nEVIL=$(touch "$HOME/os-release-executed")\n' >"$T/os-release"

pass=0
fail=0
ok() { printf 'ok   %s\n' "$1"; pass=$((pass + 1)); }
bad() { printf 'FAIL %s (got <%s>, want <%s>)\n' "$1" "$2" "$3"; fail=$((fail + 1)); }
eq() { if [ "$2" = "$3" ]; then ok "$1"; else bad "$1" "$2" "$3"; fi; }

# Every command the discovery script may probe is shadowed. Missing mocks
# therefore cannot fall through to a host executable.
stub_body='
cmd="${0##*/}"
case "$cmd" in
  systemctl)
    case "$3" in
      firewalld) case "$SPP_CASE" in fw-confirmed|fw-partial|dual-backend|fail2ban) exit 0;; esac ;;
      ufw) case "$SPP_CASE" in ufw-confirmed|manager-conflict) exit 0;; esac ;;
      nftables) case "$SPP_CASE" in nft-standalone|nft-third-party-active) exit 0;; esac ;;
      netfilter-persistent) case "$SPP_CASE" in iptables-standalone|iptables-third-party-active) exit 0;; esac ;;
      crowdsec) exit 1 ;;
    esac
    exit 1 ;;
  nft)
    [ "$1" = list ] && [ "$2" = ruleset ] || exit 1
    case "$SPP_CASE" in
      fw-confirmed|fail2ban) printf "table inet firewalld {\n chain input {}\n}\n" ;;
      fw-partial|nft-standalone|dual-backend) printf "table inet filter {\n chain input {}\n}\n" ;;
      third-party-nft|nft-third-party-active) printf "table inet docker {\n chain forward {}\n}\n" ;;
      manager-conflict) printf "table inet firewalld {\n chain input {}\n}\n" ;;
    esac
    exit 0 ;;
  iptables)
    [ "$1" = --version ] || exit 1
    case "$SPP_CASE" in dual-backend|iptables-standalone|ufw-confirmed|manager-conflict) echo "iptables v1.8.9 (legacy)" ;;
      *) echo "iptables v1.8.9 (nf_tables)" ;;
    esac
    exit 0 ;;
  iptables-save)
    case "$SPP_CASE" in
      ufw-confirmed|ufw-no-unit|manager-conflict)
        printf ":INPUT ACCEPT [0:0]\n:ufw-before-input - [0:0]\n-A INPUT -j ufw-before-input\n" ;;
      iptables-standalone)
        printf ":INPUT ACCEPT [0:0]\n-A INPUT -p tcp --dport 22 -j ACCEPT\n" ;;
      dual-backend)
        printf ":INPUT ACCEPT [0:0]\n-A INPUT -p tcp --dport 22 -j ACCEPT\n" ;;
      third-party-iptables|iptables-third-party-active)
        printf ":DOCKER - [0:0]\n-A FORWARD -j DOCKER\n" ;;
    esac
    exit 0 ;;
  firewall-cmd)
    case "$1" in
      --state) echo running ;;
      --get-firewall-backend) echo nftables ;;
      --list-all-zones) echo "public (active)" ;;
      --get-ipsets) echo "spp-test" ;;
    esac
    exit 0 ;;
  ufw)
    case "$1" in status) echo "Status: active" ;; esac
    exit 0 ;;
  fail2ban-client)
    case "$1" in
      ping) exit 0 ;;
      status) printf "Status\n|- Number of jail: 2\n\`- Jail list: sshd, nginx\n" ;;
    esac
    exit 0 ;;
  ip6tables-save) echo "# IPv6 rules"; exit 0 ;;
  ipset) echo "Name: spp-test"; exit 0 ;;
  date) echo "20261114T120000Z"; exit 0 ;;
  mkdir) exec /bin/mkdir "$@" ;;
  sha256sum) exec /usr/bin/sha256sum "$@" ;;
  *) exit 127 ;;
esac
'
for cmd in systemctl nft iptables iptables-save ip6tables-save firewall-cmd ufw \
           fail2ban-client ipset date mkdir sha256sum; do
  printf '#!/bin/bash\n%s\n' "$stub_body" >"$BIN/$cmd"
  chmod +x "$BIN/$cmd"
done

run_case() {
  local name=$1 case_name=$2 want_rc=$3 want_conf=$4 want_mgr=$5 want_back=$6
  local rc=0 out got
  out="$(env -i PATH="$BIN" HOME="$T" SPP_CASE="$case_name" \
      SPP_OS_RELEASE_FILE="$T/os-release" SPP_SYSTEMD_RUNTIME_DIR="$T/systemd" \
      /bin/bash "$SCRIPT" 2>"$T/stderr")" || rc=$?
  eq "$name exit" "$rc" "$want_rc"
  for spec in "CONFIDENCE:$want_conf" "FIREWALL_MANAGER:$want_mgr" "FIREWALL_BACKEND:$want_back"; do
    local key=${spec%%:*} want=${spec#*:}
    got="$(printf '%s\n' "$out" | awk -F= -v k="$key" '$1==k {print $2; exit}')"
    eq "$name $key" "$got" "$want"
  done
  printf '%s' "$out"
}

# OS release metadata is parsed as data; command substitutions are never evaluated.
run_case no-manager none 2 unknown unknown unknown >/dev/null
[ ! -e "$T/os-release-executed" ] && ok "os-release is not executed" || bad "os-release is not executed" executed safe

# Active firewalld without its signature is partial, not confirmed.
run_case firewalld-partial fw-partial 1 partial firewalld nftables >/dev/null

# A firewalld-owned table is confirmed even though nft output includes braces.
run_case firewalld-confirmed fw-confirmed 0 confirmed firewalld nftables >/dev/null

# UFW is commonly active without a systemd unit; status plus rule signature suffices.
run_case ufw-confirmed ufw-confirmed 0 confirmed ufw iptables-legacy >/dev/null
run_case ufw-without-systemd ufw-no-unit 0 confirmed ufw iptables-legacy >/dev/null

# Standalone managers require their own active systemd unit plus rules.
run_case nft-standalone nft-standalone 0 confirmed nftables-standalone nftables >/dev/null
run_case iptables-standalone iptables-standalone 0 confirmed iptables-standalone iptables-legacy >/dev/null

# Docker-only tables/chains do not establish standalone firewall ownership.
run_case third-party-nft third-party-nft 2 unknown unknown unknown >/dev/null
run_case third-party-iptables third-party-iptables 2 unknown unknown unknown >/dev/null
# An active standalone service plus only third-party rules is partial, not owned.
run_case active-nft-with-docker nft-third-party-active 1 partial nftables unknown >/dev/null
run_case active-iptables-with-docker iptables-third-party-active 1 partial iptables unknown >/dev/null

# Independent nft + iptables-legacy rulesets must be reported as conflict.
out="$(run_case dual-backend dual-backend 2 unknown conflict unknown)"
eq "dual backend conflict flag" "$(printf '%s\n' "$out" | awk -F= '$1=="DUAL_BACKEND_CONFLICT"{print $2}')" yes

# Two active managers that both claim ownership are a conflict.
run_case manager-conflict manager-conflict 2 unknown conflict unknown >/dev/null

# Fail2ban's real status indentation is parsed without losing comma separation.
out="$(run_case fail2ban fail2ban 0 confirmed firewalld nftables)"
eq "fail2ban state" "$(printf '%s\n' "$out" | awk -F= '$1=="FAIL2BAN"{print $2}')" running
eq "fail2ban jail list" "$(printf '%s\n' "$out" | awk -F= '$1=="FAIL2BAN_JAILS"{print $2}')" "sshd, nginx"

# Default execution is stdout-only: no snapshot directory and no logger call.
before="$(find "$T" -type f | sort)"
env -i PATH="$BIN" HOME="$T" SPP_CASE=fw-confirmed \
  SPP_OS_RELEASE_FILE="$T/os-release" SPP_SYSTEMD_RUNTIME_DIR="$T/systemd" \
  /bin/bash "$SCRIPT" >/dev/null 2>/dev/null || rc=$?
after="$(find "$T" -type f | sort)"
eq "default mode creates no persistent output" "$after" "$before"

# Opt-in snapshot mode stores actual command output, a RESULT, and hashes.
env -i PATH="$BIN" HOME="$T" SPP_CASE=fw-confirmed \
  SPP_OS_RELEASE_FILE="$T/os-release" SPP_SYSTEMD_RUNTIME_DIR="$T/systemd" \
  SPP_SNAPSHOT_DIR="$T/snapshots" /bin/bash "$SCRIPT" >/dev/null 2>/dev/null || true
SNAP="$T/snapshots/20261114T120000Z"
[ -s "$SNAP/nft_ruleset" ] && ok "snapshot captures nft command output" || bad "snapshot captures nft command output" missing present
[ -s "$SNAP/ufw_status" ] && ok "snapshot captures UFW status output" || bad "snapshot captures UFW status output" missing present
[ -s "$SNAP/RESULT" ] && ok "snapshot stores RESULT" || bad "snapshot stores RESULT" missing present
[ -s "$SNAP/SHA256" ] && ok "snapshot stores SHA-256 manifest" || bad "snapshot stores SHA-256 manifest" missing present

printf '\n%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
