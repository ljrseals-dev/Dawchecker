#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# spp-discover.sh — fail-closed, read-only-by-default firewall discovery.
#
# Default behavior: stdout only. No files, package changes, service changes,
# firewall changes, or logging.
#   SPP_SNAPSHOT_DIR=/path  opt in to timestamped command-output snapshots.
#   SPP_USE_LOGGER=1        opt in to a single summary message via logger.
#   SPP_DEBUG=1             print probe diagnostics.
#
# Exit codes: 0 confirmed; 1 partial; 2 unknown/conflict (installer must refuse).

set -u

DEBUG="${SPP_DEBUG:-0}"
OS_RELEASE_FILE="${SPP_OS_RELEASE_FILE:-/etc/os-release}"
SYSTEMD_RUNTIME_DIR="${SPP_SYSTEMD_RUNTIME_DIR:-/run/systemd/system}"
MANAGER=unknown
BACKEND=unknown
CONFIDENCE=unknown
OWNERSHIP=unproven
RESULT_LINES=()
SNAP_OK=()
SNAP_FAIL=()

dbg() { [ "$DEBUG" = 1 ] && printf 'debug: %s\n' "$*" >&2; return 0; }
warn() { printf 'WARN: %s\n' "$*" >&2; }
record() { RESULT_LINES+=("$1=$2"); }
emit_result() { printf '%s\n' "${RESULT_LINES[@]}"; }

finish() {
    local rc=$1
    if [ "${SPP_USE_LOGGER:-0}" = 1 ] && command -v logger >/dev/null 2>&1; then
        logger -t spp-discover -- "exit=$rc confidence=$CONFIDENCE manager=$MANAGER backend=$BACKEND" 2>/dev/null || true
    fi
    exit "$rc"
}

# Parse os-release as data, never source it as shell code. This keeps a
# caller-controlled fixture path from becoming a command-execution vector.
OS_ID=unknown OS_LIKE= OS_VERSION= OS_PRETTY=unknown
if [ -r "$OS_RELEASE_FILE" ]; then
    while IFS='=' read -r key value || [ -n "$key" ]; do
        case "$key" in
            ID) OS_ID="$value" ;;
            ID_LIKE) OS_LIKE="$value" ;;
            VERSION_ID) OS_VERSION="$value" ;;
            PRETTY_NAME) OS_PRETTY="$value" ;;
        esac
    done < "$OS_RELEASE_FILE"
    for key in OS_ID OS_LIKE OS_VERSION OS_PRETTY; do
        value="${!key}"
        case "$value" in
            \"*\") value=${value#\\\"}; value=${value%\\\"} ;;
            \'*\') value=${value#\\'}; value=${value%\\'} ;;
        esac
        printf -v "$key" '%s' "$value"
    done
    [ -n "$OS_ID" ] || OS_ID=unknown
    [ -n "$OS_PRETTY" ] || OS_PRETTY="$OS_ID"
fi
record OS_ID "$OS_ID"
record OS_LIKE "$OS_LIKE"
record OS_VERSION "$OS_VERSION"
record OS_PRETTY "$OS_PRETTY"

PKG=unknown
if command -v dnf >/dev/null 2>&1; then PKG=dnf
elif command -v apt-get >/dev/null 2>&1; then PKG=apt
fi
record PKG "$PKG"

HAS_SYSTEMD=0
if [ -d "$SYSTEMD_RUNTIME_DIR" ] && command -v systemctl >/dev/null 2>&1; then
    HAS_SYSTEMD=1
fi
record HAS_SYSTEMD "$HAS_SYSTEMD"

svc_active() {
    [ "$HAS_SYSTEMD" = 1 ] || return 1
    systemctl is-active --quiet "$1" 2>/dev/null
}

# Capture command output without relying on grep/awk/sed in the probed PATH.
nft_rules=""
ipt_rules=""
ipt_version=""
fw_state=""
fw_backend=""
ufw_status=""

if command -v nft >/dev/null 2>&1; then
    nft_rules="$(nft list ruleset 2>/dev/null || true)"
fi
if command -v iptables-save >/dev/null 2>&1; then
    ipt_rules="$(iptables-save 2>/dev/null || true)"
fi
if command -v iptables >/dev/null 2>&1; then
    ipt_version="$(iptables --version 2>/dev/null || true)"
fi
if command -v firewall-cmd >/dev/null 2>&1; then
    fw_state="$(firewall-cmd --state 2>/dev/null || true)"
    fw_backend="$(firewall-cmd --get-firewall-backend 2>/dev/null || true)"
fi
if command -v ufw >/dev/null 2>&1; then
    ufw_status="$(ufw status 2>/dev/null || true)"
fi

# Presence is deliberately separate from ownership. A Docker/CrowdSec table
# or chain proves rules exist, not that the corresponding tool owns the host.
NFT_RULES_PRESENT=no
case "$nft_rules" in *"table "*) NFT_RULES_PRESENT=yes ;; esac
# A third-party table alone must not be attributed to an active nftables
# service. Require at least one non-Docker/non-CrowdSec table as ownership
# evidence; mixed rulesets remain visible but are treated conservatively.
NFT_NON_THIRDPARTY_RULES=no
while IFS= read -r line; do
    case "$line" in
        "table "*)
            case "$line" in
                "table inet docker"*|"table ip docker"*|"table ip6 docker"*|\
                "table inet crowdsec"*|"table ip crowdsec"*|"table ip6 crowdsec"*) ;;
                *) NFT_NON_THIRDPARTY_RULES=yes ;;
            esac
            ;;
    esac
done <<< "$nft_rules"
IPTABLES_RULES_PRESENT=no
case "$ipt_rules" in *"-A "*) IPTABLES_RULES_PRESENT=yes ;; esac
IPTABLES_NON_THIRDPARTY_RULES=no
while IFS= read -r line; do
    case "$line" in
        "-A "*)
            case "$line" in
                *"-j DOCKER"*|*"-j CROWDSEC"*) ;;
                *) IPTABLES_NON_THIRDPARTY_RULES=yes ;;
            esac
            ;;
    esac
done <<< "$ipt_rules"

NFT_FIREWALLD=no NFT_UFW=no
while IFS= read -r line; do
    case "$line" in
        "table inet firewalld"*|"table ip firewalld"*|"table ip6 firewalld"*) NFT_FIREWALLD=yes ;;
        "table inet ufw"*|"table ip ufw"*|"table ip6 ufw"*) NFT_UFW=yes ;;
    esac
done <<< "$nft_rules"

IPT_FIREWALLD=no IPT_UFW=no
while IFS= read -r line; do
    case "$line" in
        :IN_*|:FW*|:FIREWALLD*) IPT_FIREWALLD=yes ;;
        :ufw-*|:ufw6-*|:UBUNTU*) IPT_UFW=yes ;;
    esac
done <<< "$ipt_rules"

IPT_BACKEND=unknown
case "$ipt_version" in
    *nf_tables*) IPT_BACKEND=iptables-nft ;;
    *legacy*) IPT_BACKEND=iptables-legacy ;;
esac

FW_ACTIVE=0 UFW_ACTIVE=0 NFT_SERVICE_ACTIVE=0 IPT_SERVICE_ACTIVE=0
svc_active firewalld && FW_ACTIVE=1
# UFW is not a persistent daemon on common Ubuntu releases. Its own active
# status plus its rule signature is stronger evidence than a systemd unit.
case "$ufw_status" in *"Status: active"*) UFW_ACTIVE=1 ;; esac
svc_active nftables && NFT_SERVICE_ACTIVE=1
if svc_active netfilter-persistent || svc_active iptables; then IPT_SERVICE_ACTIVE=1; fi

FW_OWNS=0 UFW_OWNS=0 NFT_OWNS=0 IPT_OWNS=0
if [ "$FW_ACTIVE" -eq 1 ] && [ "$fw_state" = running ]    && { [ "$NFT_FIREWALLD" = yes ] || [ "$IPT_FIREWALLD" = yes ]; }; then
    FW_OWNS=1
fi
if [ "$UFW_ACTIVE" -eq 1 ] && [[ "$ufw_status" == *"Status: active"* ]]    && { [ "$NFT_UFW" = yes ] || [ "$IPT_UFW" = yes ]; }; then
    UFW_OWNS=1
fi
# Standalone managers require both an active manager service and a non-empty
# ruleset. Generic application-owned rules alone never establish ownership.
if [ "$NFT_SERVICE_ACTIVE" -eq 1 ] && [ "$NFT_RULES_PRESENT" = yes ]    && [ "$NFT_NON_THIRDPARTY_RULES" = yes ]    && [ "$NFT_FIREWALLD" = no ] && [ "$NFT_UFW" = no ]; then
    NFT_OWNS=1
fi
if [ "$IPT_SERVICE_ACTIVE" -eq 1 ] && [ "$IPTABLES_RULES_PRESENT" = yes ]    && [ "$IPTABLES_NON_THIRDPARTY_RULES" = yes ]    && [ "$IPT_FIREWALLD" = no ] && [ "$IPT_UFW" = no ]; then
    IPT_OWNS=1
fi

record RULES_PRESENT_NFT "$NFT_RULES_PRESENT"
record RULES_PRESENT_IPTABLES "$IPTABLES_RULES_PRESENT"

owners=0
owner_name=""
owner_parts=()
for candidate in firewalld ufw nftables-standalone iptables-standalone; do
    owned=0
    case "$candidate" in
        firewalld) [ "$FW_OWNS" -eq 1 ] && owned=1 ;;
        ufw) [ "$UFW_OWNS" -eq 1 ] && owned=1 ;;
        nftables-standalone) [ "$NFT_OWNS" -eq 1 ] && owned=1 ;;
        iptables-standalone) [ "$IPT_OWNS" -eq 1 ] && owned=1 ;;
    esac
    if [ "$owned" -eq 1 ]; then
        owners=$((owners + 1))
        owner_name="$candidate"
        owner_parts+=("$candidate:confirmed")
    fi
done

# nft and iptables-legacy are separate kernel rule planes. If both contain
# rules, discovery cannot safely choose one manager even if a service is up.
DUAL_BACKEND_CONFLICT=no
if [ "$NFT_RULES_PRESENT" = yes ] && [ "$IPTABLES_RULES_PRESENT" = yes ]    && [ "$IPT_BACKEND" = iptables-legacy ]; then
    DUAL_BACKEND_CONFLICT=yes
fi

active_partial=""
if [ "$FW_ACTIVE" -eq 1 ] && [ "$FW_OWNS" -eq 0 ]; then active_partial=firewalld; fi
if [ "$UFW_ACTIVE" -eq 1 ] && [ "$UFW_OWNS" -eq 0 ]; then
    [ -z "$active_partial" ] && active_partial=ufw || active_partial=multiple
fi
if [ "$NFT_SERVICE_ACTIVE" -eq 1 ] && [ "$NFT_OWNS" -eq 0 ]; then
    [ -z "$active_partial" ] && active_partial=nftables || active_partial=multiple
fi
if [ "$IPT_SERVICE_ACTIVE" -eq 1 ] && [ "$IPT_OWNS" -eq 0 ]; then
    [ -z "$active_partial" ] && active_partial=iptables || active_partial=multiple
fi

if [ "$DUAL_BACKEND_CONFLICT" = yes ] || [ "$owners" -gt 1 ] || [ "$active_partial" = multiple ]; then
    MANAGER=conflict
    OWNERSHIP=conflict
    CONFIDENCE=unknown
elif [ "$owners" -eq 1 ]; then
    MANAGER="$owner_name"
    if [ -n "$active_partial" ]; then
        OWNERSHIP=owner-confirmed-other-manager-active
        CONFIDENCE=partial
    else
        OWNERSHIP=confirmed
        CONFIDENCE=confirmed
    fi
elif [ -n "$active_partial" ]; then
    MANAGER="$active_partial"
    OWNERSHIP=unproven
    CONFIDENCE=partial
else
    MANAGER=unknown
    OWNERSHIP=unproven
    CONFIDENCE=unknown
fi

case "$MANAGER" in
    firewalld)
        case "$fw_backend" in
            nftables) BACKEND=nftables ;;
            iptables) BACKEND=iptables ;;
            *) BACKEND=unknown ;;
        esac ;;
    ufw) BACKEND="$IPT_BACKEND" ;;
    nftables-standalone) BACKEND=nftables ;;
    iptables-standalone) BACKEND="$IPT_BACKEND" ;;
    *) BACKEND=unknown ;;
esac
if [ "$CONFIDENCE" = confirmed ] && [ "$BACKEND" = unknown ]; then
    CONFIDENCE=partial
    OWNERSHIP=confirmed-backend-unknown
fi

record FIREWALL_MANAGER "$MANAGER"
record FIREWALL_BACKEND "$BACKEND"
record OWNERSHIP "$OWNERSHIP"
if [ "$OWNERSHIP" = confirmed ]; then record MANAGER_OWNERSHIP_CONFIRMED yes
else record MANAGER_OWNERSHIP_CONFIRMED no; fi
record CONFIDENCE "$CONFIDENCE"
record DUAL_BACKEND_CONFLICT "$DUAL_BACKEND_CONFLICT"

# Inventory services and security tooling without changing their state.
if command -v fail2ban-client >/dev/null 2>&1; then
    if fail2ban-client ping >/dev/null 2>&1; then
        record FAIL2BAN running
        jail_output="$(fail2ban-client status 2>/dev/null || true)"
        jails=""
        while IFS= read -r line; do
            case "$line" in
                *"Jail list:"*)
                    jails="${line#*Jail list:}"
                    jails="${jails#"${jails%%[![:space:]]*}"}"
                    jails="${jails%"${jails##*[![:space:]]}"}"
                    ;;
            esac
        done <<< "$jail_output"
        record FAIL2BAN_JAILS "${jails:-none}"
    else
        record FAIL2BAN installed-stopped
        record FAIL2BAN_JAILS none
    fi
else
    record FAIL2BAN absent
    record FAIL2BAN_JAILS none
fi

if command -v cscli >/dev/null 2>&1; then
    record CROWDSEC installed
    if svc_active crowdsec; then record CROWDSEC_RUNNING yes
    else record CROWDSEC_RUNNING no; fi
else
    record CROWDSEC absent
    record CROWDSEC_RUNNING no
fi

# Optional snapshots: every successful probe writes the actual command output.
snapshot_probe() {
    local name=$1 file="$2"; shift 2
    if ! command -v "$1" >/dev/null 2>&1; then
        SNAP_FAIL+=("$name:command-absent")
        return 0
    fi
    if "$@" >"$file" 2>/dev/null; then
        SNAP_OK+=("$name")
        if command -v sha256sum >/dev/null 2>&1; then
            sha256sum "$file" >>"$SNAP_DIR/SHA256" 2>/dev/null || SNAP_FAIL+=("$name:hash-failed")
        fi
    else
        : >"$file"
        SNAP_FAIL+=("$name:probe-failed")
    fi
}

SNAP_DIR=""
if [ -n "${SPP_SNAPSHOT_DIR:-}" ]; then
    stamp="$(date -u +%Y%m%dT%H%M%SZ 2>/dev/null || printf unknown)"
    SNAP_DIR="${SPP_SNAPSHOT_DIR%/}/$stamp"
    if mkdir -p "$SNAP_DIR" 2>/dev/null; then
        : >"$SNAP_DIR/SHA256"
        snapshot_probe nft_ruleset "$SNAP_DIR/nft_ruleset" nft list ruleset
        snapshot_probe iptables_save "$SNAP_DIR/iptables_save" iptables-save
        snapshot_probe ip6tables_save "$SNAP_DIR/ip6tables_save" ip6tables-save
        snapshot_probe ipset_list "$SNAP_DIR/ipset_list" ipset list
        snapshot_probe firewalld_zones "$SNAP_DIR/firewalld_zones" firewall-cmd --list-all-zones
        snapshot_probe firewalld_ipsets "$SNAP_DIR/firewalld_ipsets" firewall-cmd --get-ipsets
        snapshot_probe ufw_status "$SNAP_DIR/ufw_status" ufw status numbered
        record SNAPSHOT_DIR "$SNAP_DIR"
    else
        SNAP_FAIL+=("snapshot-directory:create-failed")
        record SNAPSHOT_DIR unavailable
    fi
else
    record SNAPSHOT_DIR disabled
fi
record SNAPSHOTS_OK "${SNAP_OK[*]:-}"
record SNAPSHOTS_FAIL "${SNAP_FAIL[*]:-}"

printf '%s\n' '=== spp-discover ==='
printf 'OS: %s\n' "$OS_PRETTY"
printf 'Package manager: %s\n' "$PKG"
printf 'Firewall manager: %s\n' "$MANAGER"
printf 'Firewall backend: %s\n' "$BACKEND"
printf 'Confidence: %s\n' "$CONFIDENCE"
printf '%s\n' '--- RESULT ---'
emit_result
if [ -n "$SNAP_DIR" ]; then
    emit_result >"$SNAP_DIR/RESULT"
    printf 'Snapshot: %s\n' "$SNAP_DIR" >&2
fi

case "$CONFIDENCE" in
    confirmed) finish 0 ;;
    partial) warn "manager detected but ownership or backend is incomplete; installer must require explicit acceptance"; finish 1 ;;
    *) warn "manager ownership is unknown or conflicting; installer must refuse"; finish 2 ;;
esac
