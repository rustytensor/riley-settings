#!/usr/bin/env bash
# Gate for the two key bindings (brief 2026-11-13-riley-settings-keys.md): the writer hyprland-keys, which writes
# ~/.config/hypr/keys-user.lua for Riley's hypr/keys.lua. One PASS/FAIL line per check, exit 0 only if all pass.
#
#   tests/run-keys.sh                 build into /srv/devops/forge/build/riley-settings/keys and check
#   tests/run-keys.sh --plant-fault   build with -DRILEY_PLANT_FAULT_KEYS=ON into .../keys-fault: the writer ignores
#                                     the values file and the recorder drops Super pressed alone, so the k1, k2 and
#                                     k4 writer checks and recorder checks 1 (both) and 3 must fail
#
# Expected results: tests/keys/expected/*, written by hand from the brief before the writer existed; their sha256
# sums are pinned below. luajit (an independent Lua) loads each expected file and must read back the values the
# inputs set, so the Lua escaping is checked by a real Lua, not by the writer. The tool runs with HOME pointed at an
# empty throwaway directory; Marc's real ~/.config/hypr/keys-user.lua must not appear. The recorder harness
# (tests/keys/recorder) opens the real panel offscreen in a second throwaway home and presses real keys on
# keys.overview; its expected results are in its header, worked out by hand.
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
if [ "$FAULT" = ON ]; then BUILD_DIR=${BUILD_DIR:-$BASE/keys-fault}; else BUILD_DIR=${BUILD_DIR:-$BASE/keys}; fi
RUN=$BUILD_DIR/run
BIN=$BUILD_DIR/riley-config
REC_BUILD=$BUILD_DIR/recorder
REC=$REC_BUILD/tst_recorder
SHARE=$REPO/share/riley-settings
DESC=$SHARE/settings.xml
KEYS=$REPO/tests/keys
REAL_KEYS=${XDG_CONFIG_HOME:-$HOME/.config}/hypr/keys-user.lua

passed=0
failed=0
pass() { echo "PASS $1"; passed=$((passed + 1)); }
fail() { echo "FAIL $1"; [ -n "${2:-}" ] && printf '     %s\n' "$2"; failed=$((failed + 1)); }
first_line() { head -n 1 "$1" 2>/dev/null | cut -c1-200; }

real_before=absent
[ -e "$REAL_KEYS" ] && real_before=$(stat -c '%s %Y' "$REAL_KEYS")

mkdir -p "$BUILD_DIR"
rm -rf -- "$RUN"
mkdir -p "$RUN/home" "$RUN/w" "$RUN/rec/home/.config" "$RUN/rec/xdg/cache" "$RUN/rec/xdg/data" "$RUN/rec/xdg/state" \
    "$RUN/rec/runtime"
chmod 700 "$RUN/rec/runtime"

rc() {
    env -i PATH=/usr/bin:/bin HOME="$RUN/home" XDG_CONFIG_HOME="$RUN/home/.config" LANG=C.UTF-8 \
        RILEY_SETTINGS_SHARE="$SHARE" "$BIN" "$@"
}

# ---- build ----------------------------------------------------------------------------------------------------
LOG=$RUN/build.log
if cmake -S "$REPO" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRILEY_PLANT_FAULT_KEYS="$FAULT" >"$LOG" 2>&1 \
    && ninja -C "$BUILD_DIR" -j8 riley-config >>"$LOG" 2>&1 && [ -x "$BIN" ]; then
    pass "build (RILEY_PLANT_FAULT_KEYS=$FAULT) gives $BIN"
    built=1
else
    fail "build (RILEY_PLANT_FAULT_KEYS=$FAULT) gives $BIN" "$(grep -m1 -iE 'error' "$LOG" | cut -c1-200) (log: $LOG)"
    built=0
fi
need_bin() { [ "$built" = 1 ] || { fail "$1" "riley-config not built"; return 1; }; }

# ---- the expected files have not drifted, and a real Lua reads them as the inputs say ---------------------------
declare -A SUM=(
    [k1.keys-user.lua]=812db79adf7b8ba9724cfb1f7342fd50f93ea42d86e9e20a4912530f2f40d03d
    [k2.keys-user.lua]=ce5637951242892b78206beb0933db4759dae40619d8c60572bc0c9616afceeb
    [k4.keys-user.lua]=927d51104d5a9f9d79855680018399e99c1969a908213e30e3f5b1ad0c1567e1
)
for f in $(printf '%s\n' "${!SUM[@]}" | sort); do
    got=$(sha256sum "$KEYS/expected/$f" 2>/dev/null | cut -d' ' -f1)
    if [ "$got" = "${SUM[$f]}" ]; then pass "keys/expected/$f unchanged (sha256)"
    else fail "keys/expected/$f unchanged (sha256)" "got ${got:-no file}, want ${SUM[$f]}"; fi
done
# What luajit must print for each file: its keys in sorted order, "key=value", one per line.
lua_dump() {
    F="$1" luajit -e 'local t = dofile(os.getenv("F")); local k = {}; for n in pairs(t) do k[#k+1] = n end; table.sort(k)
               for _, n in ipairs(k) do io.write(n, "=", t[n], "\n") end'
}
check_lua() {   # label file want
    local out
    if ! out=$(lua_dump "$2" 2>&1); then fail "$1" "luajit: $(printf '%s' "$out" | head -n1)"
    elif [ "$out" = "$3" ]; then pass "$1"
    else fail "$1" "luajit read: $(printf '%s' "$out" | tr '\n' ' ')"; fi
}
check_lua "luajit reads keys/expected/k1 as agent_stop SUPER+ALT+Escape only" "$KEYS/expected/k1.keys-user.lua" \
    'keys.agent_stop=SUPER+ALT+Escape'
check_lua "luajit reads keys/expected/k2 with the quote and backslash of keys.sleep" "$KEYS/expected/k2.keys-user.lua" \
    "$(printf '%s\n%s' 'keys.agent_stop=SUPER+SHIFT+Escape' 'keys.sleep=SUPER+CTRL+"x\')"
check_lua "luajit reads keys/expected/k4 as agent_stop SUPER+ALT+Escape and overview SUPER+SUPER_R" \
    "$KEYS/expected/k4.keys-user.lua" "$(printf '%s\n%s' 'keys.agent_stop=SUPER+ALT+Escape' 'keys.overview=SUPER+SUPER_R')"

# ---- the description: both options, their defaults, type, target and action ------------------------------------
label="description: validate accepts the real settings.xml"
if need_bin "$label"; then
    if rc validate "$DESC" >/dev/null 2>"$RUN/err"; then pass "$label"; else fail "$label" "$(first_line "$RUN/err")"; fi
fi
for want in 'keys.agent_stop|keybinding|SUPER+SHIFT+Escape|hyprland-keys|hyprland-reload|Stop the desktop agent' \
            'keys.sleep|keybinding|SUPER+CTRL+Escape|hyprland-keys|hyprland-reload|Sleep' \
            'keys.overview|keybinding|SUPER+SUPER_L|hyprland-keys|hyprland-reload|Activities overview'; do
    key=${want%%|*}
    label="description: $key is ${want#*|}"
    got=$(python3 -I - "$DESC" "$key" <<'PY'
import sys, xml.etree.ElementTree as ET
for o in ET.parse(sys.argv[1]).iter("option"):
    if o.get("key") == sys.argv[2]:
        print("|".join(o.get(a, "") for a in ("key", "type", "default", "target", "action", "label")))
PY
)
    if [ "$got" = "$want" ]; then pass "$label"; else fail "$label" "got: ${got:-no such option}"; fi
done

# ---- the writer -----------------------------------------------------------------------------------------------
for case in k1 k2 k4; do
    label="writer hyprland-keys: keys/inputs/$case gives keys/expected/$case.keys-user.lua byte for byte"
    need_bin "$label" || continue
    out=$RUN/w/$case.lua
    if ! rc write hyprland-keys --description "$DESC" --values "$KEYS/inputs/$case.values.xml" --out "$out" 2>"$RUN/err"
    then fail "$label" "write failed: $(first_line "$RUN/err")"
    elif cmp "$out" "$KEYS/expected/$case.keys-user.lua" >"$RUN/cmp" 2>&1; then pass "$label"
    else fail "$label" "$(first_line "$RUN/cmp")"; fi
done

label="writer hyprland-keys: no key set (k3) removes an older keys-user.lua"
if need_bin "$label"; then
    out=$RUN/w/k3.lua
    cp "$KEYS/expected/k1.keys-user.lua" "$out"
    if ! rc write hyprland-keys --description "$DESC" --values "$KEYS/inputs/k3.values.xml" --out "$out" 2>"$RUN/err"
    then fail "$label" "write failed: $(first_line "$RUN/err")"
    elif [ -e "$out" ]; then fail "$label" "the file is still there"
    else pass "$label"; fi
fi

label="writer hyprland-keys: no temporary file left next to the target"
if need_bin "$label"; then
    left=$(find "$RUN/w" -mindepth 1 ! -name k1.lua ! -name k2.lua ! -name k4.lua -printf '%f ')
    if [ -z "$left" ]; then pass "$label"; else fail "$label" "left: $left"; fi
fi

# ---- the recorder: real key presses on keys.overview in the real panel ------------------------------------------
label="recorder harness: build (RILEY_PLANT_FAULT_KEYS=$FAULT) gives $REC"
if cmake -S "$KEYS/recorder" -B "$REC_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRILEY_PLANT_FAULT_KEYS="$FAULT" \
        >"$RUN/rec-build.log" 2>&1 && ninja -C "$REC_BUILD" -j8 tst_recorder >>"$RUN/rec-build.log" 2>&1 && [ -x "$REC" ]
then
    pass "$label"
    env -i PATH=/usr/bin:/bin HOME="$RUN/rec/home" XDG_CONFIG_HOME="$RUN/rec/home/.config" \
        XDG_CACHE_HOME="$RUN/rec/xdg/cache" XDG_DATA_HOME="$RUN/rec/xdg/data" XDG_STATE_HOME="$RUN/rec/xdg/state" \
        XDG_RUNTIME_DIR="$RUN/rec/runtime" LANG=C.UTF-8 LC_ALL=C.UTF-8 \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QUICK_CONTROLS_STYLE=Basic QML_DISABLE_DISK_CACHE=1 \
        RILEY_SETTINGS_SHARE="$SHARE" RILEY_SETTINGS_ACTION_LOG="$RUN/rec/actions.log" \
        timeout 120 "$REC" >"$RUN/rec.log" 2>"$RUN/rec.err"
    code=$?
    while IFS= read -r line; do
        case $line in
            "PASS "*) echo "$line"; passed=$((passed + 1)) ;;
            "FAIL "*) echo "$line"; failed=$((failed + 1)) ;;
            "     "*) echo "$line" ;;
        esac
    done <"$RUN/rec.log"
    n=$(grep -c '^\(PASS\|FAIL\) ' "$RUN/rec.log")
    if grep -qx "DONE 6" "$RUN/rec.log" && [ "$n" = 6 ]; then pass "recorder harness ran its 6 checks (exit $code)"
    else fail "recorder harness ran its 6 checks" "exit $code, $n checks; stderr: $(first_line "$RUN/rec.err")"; fi
else
    fail "$label" "$(grep -m1 -iE 'error' "$RUN/rec-build.log" | cut -c1-200) (log: $RUN/rec-build.log)"
fi

# ---- nothing written outside the files named on the command line ----------------------------------------------
label="the throwaway home is still empty"
if need_bin "$label"; then
    found=$(find "$RUN/home" -mindepth 1 -printf '%P ')
    if [ -z "$found" ]; then pass "$label"; else fail "$label" "found: $found"; fi
fi
real_after=absent
[ -e "$REAL_KEYS" ] && real_after=$(stat -c '%s %Y' "$REAL_KEYS")
if [ "$real_before" = "$real_after" ]; then pass "Marc's real keys-user.lua untouched ($real_after)"
else fail "Marc's real keys-user.lua untouched" "before: $real_before, after: $real_after"; fi

echo "$passed passed, $failed failed"
[ "$FAULT" = ON ] && echo "(planted-fault build: the k1, k2 and k4 writer checks and recorder checks 1 (both) and 3 must fail; the gate is proven when they do)"
[ "$failed" = 0 ]
