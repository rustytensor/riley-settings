#!/usr/bin/env bash
# Gate for a screen's colour depth (bitdepth = 8 or 10 in hl.monitor). One PASS/FAIL line per check, exit 0 only if
# all pass.
#
#   tests/run-bitdepth.sh                 build into /srv/devops/forge/build/riley-settings/bitdepth and check
#   tests/run-bitdepth.sh --plant-fault   build with -DRILEY_PLANT_FAULT_BITDEPTH=ON into .../bitdepth-fault: no
#                                         bitdepth in settings.lua, the HDMI deep-colour flags not read, no 10-bit
#                                         seed from the running screens; the checks that need them must fail
#
# Expected results, none made by the code under test (tests/bitdepth/make-files.py made them once; sha256 pinned):
#   - edid/*.bin: made-up EDIDs; the bits per colour of each were worked out by hand, and edid-decode (an independent
#     decoder) must agree;
#   - expected/*.settings.lua and *.model.json: Riley's core.py (riley 0027087) render and json.dumps;
#   - expected/b4.import.values.xml: written by hand (bitdepth kept only when it is not 8).
# The live check reads your screens' EDIDs from /sys (read only) and compares with edid-decode's results on them
# (2026-11-13): DP-1, DP-2, DP-3 10 bits per primary; HDMI-A-2 DC_30bit; HDMI-A-1 no deep colour. A connector that
# is not there is skipped. Everything else runs in throwaway homes with fakes for hyprctl and Hyprland; your files
# are checked untouched at the end.
set -u

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BASE=/srv/devops/forge/build/riley-settings
FAULT=OFF
for a in "$@"; do
    case $a in
        --plant-fault) FAULT=ON ;;
        -h|--help) sed -n '2,19p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown argument: $a" >&2; exit 2 ;;
    esac
done
if [ "$FAULT" = ON ]; then BUILD_DIR=${BUILD_DIR:-$BASE/bitdepth-fault}; else BUILD_DIR=${BUILD_DIR:-$BASE/bitdepth}; fi
RUN=$BUILD_DIR/run
H_BUILD=$BUILD_DIR/harness
BIN=$H_BUILD/riley-config
QT=$H_BUILD/riley-settings-qt
TST=$H_BUILD/tst_displays
SHARE=$REPO/share/riley-settings
DESC=$SHARE/settings.xml
B=$REPO/tests/bitdepth

passed=0
failed=0
pass() { echo "PASS $1"; passed=$((passed + 1)); }
fail() { echo "FAIL $1"; [ -n "${2:-}" ] && printf '     %s\n' "$2"; failed=$((failed + 1)); }
first_line() { head -n 1 "$1" 2>/dev/null | cut -c1-200; }

REAL_CFG=${XDG_CONFIG_HOME:-$HOME/.config}
REAL_FILES="$REAL_CFG/riley/settings.xml $REAL_CFG/hypr/settings.lua $REAL_CFG/hypr/settings.json"
state() { for f in $REAL_FILES; do if [ -e "$f" ]; then stat -c '%n %Y %s' "$f"; else echo "$f absent"; fi; done; }
REAL_BEFORE=$(state)

mkdir -p "$BUILD_DIR"
rm -rf -- "$RUN"
mkdir -p "$RUN/w" "$RUN/import/.config/hypr" "$RUN/fakebin" "$RUN/drm" "$RUN/tmp" "$RUN/runtime"
chmod 700 "$RUN/runtime"

# ---- build: the harness project builds the repo too (riley-config, riley-settings-qt) --------------------------
LOG=$RUN/build.log
if cmake -S "$B/harness" -B "$H_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRILEY_PLANT_FAULT_BITDEPTH="$FAULT" \
        >"$LOG" 2>&1 && ninja -C "$H_BUILD" -j8 tst_displays riley-config riley-settings-qt >>"$LOG" 2>&1 \
    && [ -x "$TST" ] && [ -x "$BIN" ] && [ -x "$QT" ]; then
    pass "build (RILEY_PLANT_FAULT_BITDEPTH=$FAULT) gives tst_displays, riley-config, riley-settings-qt"
    built=1
else
    fail "build (RILEY_PLANT_FAULT_BITDEPTH=$FAULT)" "$(grep -m1 -iE 'error' "$LOG" | cut -c1-200) (log: $LOG)"
    built=0
fi
need_bin() { [ "$built" = 1 ] || { fail "$1" "not built"; return 1; }; }
rc() { env -i PATH=/usr/bin:/bin HOME="$RUN/w" LANG=C.UTF-8 RILEY_SETTINGS_SHARE="$SHARE" "$BIN" "$@"; }

# ---- the fixtures and expected files have not drifted ----------------------------------------------------------
declare -A SUM=(
    [edid/analog.bin]=c56924ff21d2b868a9f1eb0ad68747d6dc36c17ab1cc9c46ea951011fb6faa41
    [edid/dp-10.bin]=877f5880a425c8f1f87f3fe9ada4a4f02ddd8368a18b2bcd652faf119c86b9a7
    [edid/dp-8.bin]=f2f340a29949f7c76b0810a7aa6110990854e400bf02b0323f06bdd1094b5072
    [edid/hdmi-8.bin]=d5743d903bcbee8190050b16bc3bb33bf459a3b9156441a294878e11ea7b803a
    [edid/hdmi-dc30.bin]=bd5d25258d7f3d14d2da38b480ad974d08f5e43d9a7bcc2ee2a40e4778636294
    [expected/b1.settings.lua]=e510a3dbb0eea223bfb8ba0624b9aaccde1cc6a810be8f932fff7cea2f62281d
    [expected/b1-edit.model.json]=a67cbe01a7014a7b332301262545de5cb64afcbd50b09237eb1d3c34533ba98d
    [expected/b1.model.json]=a7173b4ac9fe1d2d548f7ac82538ffa0485c225810cc5ba99b967cc8da7c1baa
    [expected/b3.settings.lua]=f5b2f097df75e35dde14ffa519bc177675c9ce0e91240a28f754ae8ae8c6307d
    [expected/b3.model.json]=3bd9c7aa7799c91513b3a37adfe7604f5b8a51212511822798f81ee7503ef077
    [expected/b4.import.values.xml]=7cd9b9128bcc6d03804686bc728ae326bb80858db20905dd40aec2fd85bd9663
    [inputs/b1.values.xml]=5c08af1419629eda90282c7af7592e5e18d22315d0464ec2f748d51067725d50
    [inputs/b2.values.xml]=d2f4ba62cf2ed92c2207d0c759ee6b6fe6d4bd53ef95071cef248a3c33fcd12c
    [inputs/b4.settings.json]=f02c50af90f44c47058812bf7fcedd2c766231dc97bd3423d38b48ea5cd66d6c
    [inputs/monitors-all.json]=4fcc2857c7ab178fac6142248b3e67ee782ef0952397f7a0597b8657110f7f2a
)
for f in $(printf '%s\n' "${!SUM[@]}" | sort); do
    got=$(sha256sum "$B/$f" 2>/dev/null | cut -d' ' -f1)
    if [ "$got" = "${SUM[$f]}" ]; then pass "bitdepth/$f unchanged (sha256)"
    else fail "bitdepth/$f unchanged (sha256)" "got ${got:-no file}, want ${SUM[$f]}"; fi
done

# ---- EDIDs: made-up ones, worked out by hand and confirmed by edid-decode ----------------------------------------
# name  bits  what edid-decode must print (a grep -E pattern; "-" = it must print none of the depth lines)
while read -r name want decode; do
    f=$B/edid/$name.bin
    if [ "$decode" = - ]; then
        if command -v edid-decode >/dev/null && ! edid-decode "$f" 2>/dev/null | grep -qE 'Bits per primary|DC_(30|36|48)bit'
        then pass "edid-decode: $name.bin states no colour depth"
        else fail "edid-decode: $name.bin states no colour depth" "$(edid-decode "$f" 2>&1 | grep -E 'Bits per primary|DC_' | head -n1)"; fi
    elif command -v edid-decode >/dev/null && edid-decode "$f" 2>/dev/null | grep -qE "$decode"; then
        pass "edid-decode: $name.bin says /$decode/"
    else
        fail "edid-decode: $name.bin says /$decode/" "edid-decode missing or it says otherwise"
    fi
    label="riley-config edid $name.bin gives $want"
    if need_bin "$label"; then
        got=$(rc edid "$f" 2>&1)
        if [ "$got" = "$want" ]; then pass "$label"; else fail "$label" "got $got"; fi
    fi
done <<'EOF'
dp-10      10  Bits per primary color channel: 10
dp-8       8   Bits per primary color channel: 8
hdmi-dc30  10  DC_30bit
hdmi-8     8   -
analog     0   -
EOF

# ---- your screens, read only -------------------------------------------------------------------------------------
while read -r conn want; do
    f=$(ls -d /sys/class/drm/card*-"$conn" 2>/dev/null | head -n1)/edid
    if [ ! -r "$f" ] || [ "$(head -c 128 "$f" 2>/dev/null | wc -c)" -lt 128 ]; then
        echo "SKIP live $conn: no EDID at /sys/class/drm/card*-$conn"
        continue
    fi
    label="live $conn: riley-config edid gives $want (as edid-decode found)"
    if need_bin "$label"; then
        head -c 4096 "$f" >"$RUN/w/live-$conn.edid"
        got=$(rc edid "$RUN/w/live-$conn.edid" 2>&1)
        if [ "$got" = "$want" ]; then pass "$label"; else fail "$label" "got $got"; fi
    fi
done <<'EOF'
DP-1 10
DP-2 10
DP-3 10
HDMI-A-2 10
HDMI-A-1 8
EOF
rm -f "$RUN"/w/live-*.edid   # your screens' EDIDs hold serial numbers: not kept

# ---- the writers ----------------------------------------------------------------------------------------------
for t in "hyprland-lua b1.settings.lua" "riley-model b1.model.json"; do
    target=${t%% *} want=${t#* }
    label="writer $target: inputs/b1 gives expected/$want byte for byte"
    need_bin "$label" || continue
    out=$RUN/w/$want
    if ! rc write "$target" --description "$DESC" --values "$B/inputs/b1.values.xml" --out "$out" 2>"$RUN/err"
    then fail "$label" "write failed: $(first_line "$RUN/err")"
    elif cmp "$out" "$B/expected/$want" >"$RUN/cmp" 2>&1; then pass "$label"
    else fail "$label" "$(first_line "$RUN/cmp")"; fi
done

# ---- the import keeps 10 and leaves 8 out -------------------------------------------------------------------------
label="import: inputs/b4.settings.json (10, 10 and an explicit 8) gives expected/b4.import.values.xml"
if need_bin "$label"; then
    H=$RUN/import
    cp "$B/inputs/b4.settings.json" "$H/.config/hypr/settings.json"
    env -i PATH=/usr/bin:/bin HOME="$H" XDG_CONFIG_HOME="$H/.config" XDG_CACHE_HOME="$H/.cache" \
        XDG_DATA_HOME="$H/.local/share" XDG_RUNTIME_DIR="$RUN/runtime" TMPDIR="$RUN/tmp" LANG=C.UTF-8 LC_ALL=C.UTF-8 \
        RILEY_SETTINGS_SHARE="$SHARE" RILEY_SETTINGS_ACTION_LOG="$RUN/import-actions.log" \
        "$QT" --import >"$RUN/import.out" 2>"$RUN/import.err"
    code=$?
    if [ "$code" != 0 ]; then fail "$label" "exit $code: $(first_line "$RUN/import.err")"
    elif cmp "$H/.config/riley/settings.xml" "$B/expected/b4.import.values.xml" >"$RUN/cmp" 2>&1; then pass "$label"
    else fail "$label" "$(first_line "$RUN/cmp")"; fi
fi

# ---- the Displays page, with fakes ------------------------------------------------------------------------------
cat >"$RUN/fakebin/hyprctl" <<EOF
#!/bin/sh
case "\$*" in
    "monitors all -j"|"monitors -j") cat "$B/inputs/monitors-all.json" ;;
    *) echo "hyprctl \$*" >>"$RUN/unexpected.log"; exit 1 ;;
esac
EOF
cat >"$RUN/fakebin/Hyprland" <<EOF
#!/bin/sh
[ "\$1" = --verify-config ] && { echo "config ok"; exit 0; }
echo "Hyprland \$*" >>"$RUN/unexpected.log"; exit 1
EOF
chmod +x "$RUN/fakebin/hyprctl" "$RUN/fakebin/Hyprland"
for pair in HDMI-A-1:hdmi-8 DP-1:dp-10 DP-3:dp-10 DP-2:dp-8 HDMI-A-2:hdmi-dc30; do
    conn=${pair%%:*}
    mkdir -p "$RUN/drm/card1-$conn"
    cp "$B/edid/${pair#*:}.bin" "$RUN/drm/card1-$conn/edid"
done

for c in b1 b2 b3; do
    label="Displays $c: harness ran"
    need_bin "$label" || continue
    H=$RUN/page-$c
    mkdir -p "$H/.config/riley" "$H/xdg/cache" "$H/xdg/data" "$H/xdg/state"
    in=b1; [ "$c" = b2 ] && in=b2
    cp "$B/inputs/$in.values.xml" "$H/.config/riley/settings.xml"
    env -i PATH="$RUN/fakebin:/usr/bin:/bin" HOME="$H" XDG_CONFIG_HOME="$H/.config" XDG_CACHE_HOME="$H/xdg/cache" \
        XDG_DATA_HOME="$H/xdg/data" XDG_STATE_HOME="$H/xdg/state" XDG_RUNTIME_DIR="$RUN/runtime" TMPDIR="$RUN/tmp" \
        LANG=C.UTF-8 LC_ALL=C.UTF-8 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QUICK_CONTROLS_STYLE=Basic \
        QML_DISABLE_DISK_CACHE=1 RILEY_SETTINGS_SHARE="$SHARE" RILEY_SETTINGS_ACTION_LOG="$H/actions.log" \
        RILEY_DRM_DIR="$RUN/drm" RILEY_BITDEPTH_EXPECTED="$B/expected" \
        timeout 120 "$TST" "$c" >"$RUN/page-$c.log" 2>"$RUN/page-$c.err"
    code=$?
    while IFS= read -r line; do
        case $line in
            "PASS "*) echo "$line"; passed=$((passed + 1)) ;;
            "FAIL "*) echo "$line"; failed=$((failed + 1)) ;;
            "     "*) echo "$line" ;;
        esac
    done <"$RUN/page-$c.log"
    want=8; [ "$c" = b2 ] || want=9
    n=$(grep -c '^\(PASS\|FAIL\) ' "$RUN/page-$c.log")
    if grep -qx "DONE $want" "$RUN/page-$c.log" && [ "$n" = "$want" ]; then pass "$label: $want checks (exit $code)"
    else fail "$label: $want checks" "exit $code, $n checks; stderr: $(first_line "$RUN/page-$c.err")"; fi
done
if [ ! -e "$RUN/unexpected.log" ]; then pass "the fakes got only the calls they answer"
else fail "the fakes got only the calls they answer" "$(first_line "$RUN/unexpected.log")"; fi

# ---- your files ---------------------------------------------------------------------------------------------------
if [ "$(state)" = "$REAL_BEFORE" ]; then pass "your settings.xml, settings.lua and settings.json untouched"
else fail "your settings.xml, settings.lua and settings.json untouched" "$(diff <(echo "$REAL_BEFORE") <(state) | head -n 2 | tr '\n' ' ')"; fi

echo "$passed passed, $failed failed"
[ "$FAULT" = ON ] && echo "(planted-fault build: the HDMI EDID checks, the settings.lua checks and the b2 seed checks must fail)"
[ "$failed" = 0 ]
