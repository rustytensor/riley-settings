#!/usr/bin/env bash
# Gate for M2, the panel drawn from the description. One command, one PASS/FAIL line per check, exit 0 only if all
# pass. The contract it checks: docs/panel-contract.md.
#
#   tests/run-panel.sh                 build tests/panel/ (the harness plus the repo) into
#                                      /srv/devops/forge/build/riley-settings/panel and check
#   tests/run-panel.sh --plant-fault   build with -DRILEY_PLANT_FAULT_PANEL=ON into .../panel-fault: the second
#                                      option's control is bound to the third option's key, so the checks of
#                                      power.lock_after and every values file from step 2 on must fail
#   BUILD_DIR=/some/dir tests/run-panel.sh   another build directory
#
# The harness runs offscreen with an empty throwaway home under $BUILD_DIR/run, the test description
# tests/panel/settings.xml, and nothing else from this session. Actions are recorded in $BUILD_DIR/run/actions.log,
# never run. Expected results: tests/panel/expected/*, written by hand before the panel existed; their sha256 sums
# and the test description's are pinned below.
set -u

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BASE=/srv/devops/forge/build/riley-settings
FAULT=OFF
for a in "$@"; do
    case $a in
        --plant-fault) FAULT=ON ;;
        -h|--help) sed -n '2,16p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown argument: $a" >&2; exit 2 ;;
    esac
done
if [ "$FAULT" = ON ]; then BUILD_DIR=${BUILD_DIR:-$BASE/panel-fault}; else BUILD_DIR=${BUILD_DIR:-$BASE/panel}; fi
RUN=$BUILD_DIR/run
PANEL=$REPO/tests/panel
EXPECTED=$PANEL/expected

passed=0
failed=0
pass() { echo "PASS $1"; passed=$((passed + 1)); }
fail() { echo "FAIL $1"; [ -n "${2:-}" ] && printf '     %s\n' "$2"; failed=$((failed + 1)); }
first_line() { head -n 1 "$1" 2>/dev/null | cut -c1-200; }

# Marc's real files: their state now, compared at the end. Read only, never written.
REAL_CFG=${XDG_CONFIG_HOME:-$HOME/.config}
REAL_FILES="$REAL_CFG/riley/settings.xml $REAL_CFG/hypr/idle.conf $REAL_CFG/hypr/hyprpaper.conf $REAL_CFG/hypr/settings.lua"
state() { for f in $REAL_FILES; do if [ -e "$f" ]; then stat -c '%n %y %s' "$f"; else echo "$f absent"; fi; done; }
REAL_BEFORE=$(state)

mkdir -p "$BUILD_DIR"
rm -rf -- "$RUN"
mkdir -p "$RUN/home/.config" "$RUN/share" "$RUN/trap" "$RUN/xdg/cache" "$RUN/xdg/data" "$RUN/xdg/state" "$RUN/runtime"
chmod 700 "$RUN/runtime"

# ---- build ----------------------------------------------------------------------------------------------------
LOG=$RUN/build.log
TST=$BUILD_DIR/tst_panel
if cmake -S "$PANEL" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRILEY_PLANT_FAULT=OFF \
        -DRILEY_PLANT_FAULT_PANEL="$FAULT" >"$LOG" 2>&1 \
    && ninja -C "$BUILD_DIR" -j8 >>"$LOG" 2>&1 && [ -x "$TST" ]; then
    pass "build (RILEY_PLANT_FAULT_PANEL=$FAULT) gives $TST"
    built=1
else
    fail "build (RILEY_PLANT_FAULT_PANEL=$FAULT) gives $TST" "$(grep -m1 -iE 'error' "$LOG" | cut -c1-200) (log: $LOG)"
    built=0
fi
if [ "$built" = 1 ] && [ -x "$BUILD_DIR/riley-settings-qt" ]; then pass "build gives the panel program riley-settings-qt"
else fail "build gives the panel program riley-settings-qt" "no $BUILD_DIR/riley-settings-qt"; fi

# ---- the expected files and the test description have not drifted --------------------------------------------
declare -A SUM=(
    [actions.log]=aa17d7d0380f41ce97ab512c4b2b06fdcc3d35cb606e40d88487ca3280bca25e
    [hyprpaper-04.conf]=f192ed290ce28cc5e8742047df7dedc8d6a91a659fca5decad57134c68657889
    [hyprpaper-08.conf]=7ebae4d22c549ca882ab180522eb588004ceee31c7c216f01ced4a6ff88159eb
    [hyprpaper-11.conf]=609691aa75b7fd47ea7ccaa10c3c09e68000076b4b3ba9a30a67f6977758b0ca
    [idle-01.conf]=1109690c9b8654c3038abe6aa1e976d34034917ac394b43cda87f0b4be7a5852
    [idle-02.conf]=6dad9e594726a43b68d2f1980b0da56a98a517edc283e8b967c18174051711c3
    [values-01.xml]=179f4609597b8319f8f2ecb2f39d72b283ee9dbb74b2a9bb8149b93d82ea25f4
    [values-02.xml]=92499e7889e22a2d903716921b6c529c16caaf102bccd7ace1861e459c9038e2
    [values-03.xml]=3a9eb17ec71829fda0e4aca6d3395f0a1285075185fa4f7e5980bdc08f6d2e85
    [values-04.xml]=362708059c0d786ed87749a3fb0d8c997e04d30e3f2448034ca6b316b8ab6b45
    [values-05.xml]=5e47e39e6ef3a8e9fd6f8cff66d6ef660112504ce17cdb630791ec97d6099ea4
    [values-06.xml]=763aab3c1fab23276e49ddc592c1cb4b10f9e28ecc1524910aeaaa236dcbf0bf
    [values-07.xml]=a9b0d85e6e00a990ef96b8394d895e34972c385d1ac0dc931ce2e81772d6335e
    [values-08.xml]=fcf8bc2db8c6146bc0fadc0c38570cc8c971252f573e251ca8ef190a5129be08
    [values-09.xml]=851dc0da5dee13f69780f44d7554f17cd7e9ba32c73b0365ed14c162d3428ba1
    [values-10.xml]=c54544ba3d70638db040306ea9a91ad716bf28426d7667a6d975edf1a8f8596b
    [values-11.xml]=c7f669d92efebcf47a80e5cea8fcbbacc27f0aef4585af6468401c6773e9e8de
    [values-12.xml]=35e0ed5f82c593a7b0f2807ce2188b4ffbfa8c4cb83400416208bf7ccb718052
)
DESC_SUM=472486a7025a741025407a43c2e1c46f2973f1401cb250a5b11ef13ec59c0d40   # widget renamed to m2-placeholder for M3 (docs/pages-contract.md)
for f in $(printf '%s\n' "${!SUM[@]}" | sort); do
    got=$(sha256sum "$EXPECTED/$f" 2>/dev/null | cut -d' ' -f1)
    if [ "$got" = "${SUM[$f]}" ]; then pass "panel/expected/$f unchanged (sha256)"
    else fail "panel/expected/$f unchanged (sha256)" "got ${got:-no file}, want ${SUM[$f]}"; fi
done
extra=$(cd "$EXPECTED" && for f in *; do [ -n "${SUM[$f]:-}" ] || printf '%s ' "$f"; done)
if [ -z "$extra" ]; then pass "tests/panel/expected holds only the pinned files"
else fail "tests/panel/expected holds only the pinned files" "not pinned: $extra"; fi
got=$(sha256sum "$PANEL/settings.xml" 2>/dev/null | cut -d' ' -f1)
if [ "$got" = "$DESC_SUM" ]; then pass "tests/panel/settings.xml unchanged (sha256)"
else fail "tests/panel/settings.xml unchanged (sha256)" "got ${got:-no file}, want $DESC_SUM"; fi

# ---- the test description is valid under the repo's schema ----------------------------------------------------
if xmllint --noout --schema "$REPO/share/riley-settings/schema/settings.xsd" "$PANEL/settings.xml" >"$RUN/xsd.err" 2>&1
then pass "tests/panel/settings.xml validates against share/riley-settings/schema/settings.xsd"
else fail "tests/panel/settings.xml validates against share/riley-settings/schema/settings.xsd" \
    "$(grep -m1 -v 'fails to validate' "$RUN/xsd.err" | cut -c1-200)"; fi

# ---- the panel's code names no option --------------------------------------------------------------------------
label="the panel's code (src/panel, src/app, qml) names no option key"
dirs=()
for d in src/panel src/app qml; do [ -d "$REPO/$d" ] && dirs+=("$REPO/$d"); done
if [ "${#dirs[@]}" = 0 ]; then
    fail "$label" "none of src/panel, src/app, qml exists"
else
    keys=$(sed -n 's/.*<option[^>]*[[:space:]]key="\([^"]*\)".*/\1/p' "$REPO/share/riley-settings/settings.xml" \
        "$PANEL/settings.xml" | sort -u)
    nkeys=$(grep -c . <<<"$keys")
    hits=$(grep -rnwF -f <(printf '%s\n' "$keys") "${dirs[@]}" 2>/dev/null | head -n 3 | cut -c1-200)
    if [ "$nkeys" -lt 40 ]; then fail "$label" "found only $nkeys keys in the two descriptions"
    elif [ -z "$hits" ]; then pass "$label ($nkeys keys)"
    else fail "$label" "$(head -n 1 <<<"$hits")"; fi
fi

# ---- run the harness ---------------------------------------------------------------------------------------------
# The share directory: the test description and the repo's schemas.
cp "$PANEL/settings.xml" "$RUN/share/settings.xml"
cp -r "$REPO/share/riley-settings/schema" "$RUN/share/schema"
# Traps named like the desktop's commands, first on PATH: any call is recorded and fails the gate.
for c in hyprctl hypridle hyprpaper hyprlock systemctl loginctl pkill killall riley-dock riley-greeter-sync \
         hypr-theme hypr-theme.py hypr-reload quickshell qs waybar pkexec sudo notify-send; do
    printf '#!/bin/sh\necho "%s $*" >>"%s"\nexit 1\n' "$c" "$RUN/trap.log" >"$RUN/trap/$c"
    chmod +x "$RUN/trap/$c"
done

RESULTS=$RUN/results
: >"$RESULTS"
if [ "$built" = 1 ]; then
    env -i PATH="$RUN/trap:/usr/bin:/bin" HOME="$RUN/home" XDG_CONFIG_HOME="$RUN/home/.config" \
        XDG_CACHE_HOME="$RUN/xdg/cache" XDG_DATA_HOME="$RUN/xdg/data" XDG_STATE_HOME="$RUN/xdg/state" \
        XDG_RUNTIME_DIR="$RUN/runtime" LANG=C.UTF-8 LC_ALL=C.UTF-8 \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QUICK_CONTROLS_STYLE=Basic QML_DISABLE_DISK_CACHE=1 \
        RILEY_SETTINGS_SHARE="$RUN/share" RILEY_SETTINGS_ACTION_LOG="$RUN/actions.log" \
        RILEY_PANEL_EXPECTED="$EXPECTED" RILEY_PANEL_RESULTS="$RESULTS" \
        timeout 300 "$TST" -maxwarnings 0 >"$RUN/harness.log" 2>&1
    code=$?
    while IFS= read -r line; do
        case $line in
            "PASS "*) pass "${line#PASS }" ;;
            "FAIL "*) fail "${line#FAIL }" ;;
            "     "*) printf '%s\n' "$line" ;;
        esac
    done < <(grep -v '^DONE ' "$RESULTS")
    done_line=$(grep '^DONE ' "$RESULTS" | tail -n 1)
    if [ -n "$done_line" ]; then pass "harness ran to the end (${done_line#DONE } checks)"
    else fail "harness ran to the end" "exit $code, no DONE line: $(grep -m1 -E 'FAIL!|QFATAL|Segmentation|error' "$RUN/harness.log" | cut -c1-200) (log: $RUN/harness.log)"; fi
    hf=$(grep -c '^FAIL ' "$RESULTS")
    if { [ "$code" = 0 ] && [ "$hf" = 0 ]; } || { [ "$code" != 0 ] && [ "$hf" != 0 ]; }; then
        pass "harness exit code ($code) agrees with its checks ($hf failed)"
    else fail "harness exit code ($code) agrees with its checks ($hf failed)" "log: $RUN/harness.log"; fi
else
    fail "harness ran to the end" "not built"
fi

# ---- nothing written outside the throwaway home's expected files, nothing run -------------------------------
label="the throwaway home holds only .config/riley/settings.xml, .config/hypr/idle.conf, .config/hypr/hyprpaper.conf"
if [ "$built" = 1 ]; then
    found=$(cd "$RUN/home" && find . -mindepth 1 ! -type d -printf '%P\n' | sort)
    want=$(printf '%s\n' .config/hypr/hyprpaper.conf .config/hypr/idle.conf .config/riley/settings.xml)
    odd=$(cd "$RUN/home" && find . -mindepth 1 -type d ! -path ./.config ! -path ./.config/riley ! -path ./.config/hypr -printf '%P ')
    if [ "$found" = "$want" ] && [ -z "$odd" ]; then pass "$label"
    else fail "$label" "found: $(tr '\n' ' ' <<<"$found")${odd:+ dirs: $odd}"; fi
else
    fail "$label" "not built"
fi
if [ ! -e "$RUN/trap.log" ]; then pass "no desktop command was run (traps on PATH untouched)"
else fail "no desktop command was run (traps on PATH untouched)" "$(first_line "$RUN/trap.log")"; fi
REAL_AFTER=$(state)
if [ "$REAL_BEFORE" = "$REAL_AFTER" ]; then pass "Marc's real ~/.config/riley/settings.xml and hypr targets unchanged (mtime, size)"
else fail "Marc's real ~/.config/riley/settings.xml and hypr targets unchanged (mtime, size)" \
    "$(diff <(echo "$REAL_BEFORE") <(echo "$REAL_AFTER") | grep '^>' | head -n 1)"; fi

echo "$passed passed, $failed failed"
[ "$FAULT" = ON ] && echo "(planted-fault build: power.lock_after's checks and the values files from step 2 on must fail; the gate is proven when they do)"
[ "$failed" = 0 ]
