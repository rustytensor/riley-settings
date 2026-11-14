#!/usr/bin/env bash
# Gate for the Overview page (brief 2026-11-14-riley-overview-fixes.md): the writer riley-overview, which writes
# ~/.config/riley-overview/config.xml for riley-overview, and the page in the real panel, which restarts
# riley-overview.service after a change. One PASS/FAIL line per check, exit 0 only if all pass.
#
#   tests/run-overview.sh                 build into /srv/devops/forge/build/riley-settings/overview and check
#   tests/run-overview.sh --plant-fault   build with -DRILEY_PLANT_FAULT_OVERVIEW=ON into .../overview-fault: the
#                                         writer drops the elements it does not manage and the restart runs nothing,
#                                         so the o2 and r4 writer checks and panel checks 2, 3 and 4 (systemctl) must
#                                         fail
#
# Expected results: tests/overview/expected/*, written by hand from riley-overview's data/config.xsd before the
# writer existed (the derivation is in the header of each input); their sha256 sums are pinned below. xmllint (an
# independent validator) checks each expected file, and each file the writer leaves, against riley-overview's own
# schema. The tool runs with HOME pointed at an empty throwaway directory; Marc's real config.xml must not change.
set -u

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BASE=/srv/devops/forge/build/riley-settings
FAULT=OFF
for a in "$@"; do
    case $a in
        --plant-fault) FAULT=ON ;;
        -h|--help) sed -n '2,15p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown argument: $a" >&2; exit 2 ;;
    esac
done
if [ "$FAULT" = ON ]; then BUILD_DIR=${BUILD_DIR:-$BASE/overview-fault}; else BUILD_DIR=${BUILD_DIR:-$BASE/overview}; fi
RUN=$BUILD_DIR/run
BIN=$BUILD_DIR/riley-config
PAN_BUILD=$BUILD_DIR/panel
PAN=$PAN_BUILD/tst_overview
SHARE=$REPO/share/riley-settings
DESC=$SHARE/settings.xml
OV=$REPO/tests/overview
XSD=${OVERVIEW_XSD:-$HOME/git/riley-overview/data/config.xsd}
REAL_OV=${XDG_CONFIG_HOME:-$HOME/.config}/riley-overview/config.xml

passed=0
failed=0
pass() { echo "PASS $1"; passed=$((passed + 1)); }
fail() { echo "FAIL $1"; [ -n "${2:-}" ] && printf '     %s\n' "$2"; failed=$((failed + 1)); }
first_line() { head -n 1 "$1" 2>/dev/null | cut -c1-200; }
stamp() { if [ -e "$1" ]; then stat -c '%s %Y' "$1"; else echo absent; fi; }

real_before=$(stamp "$REAL_OV")

mkdir -p "$BUILD_DIR"
rm -rf -- "$RUN"
mkdir -p "$RUN/home" "$RUN/data/riley-overview" "$RUN/w" "$RUN/pan/home/.config" "$RUN/pan/xdg/cache" \
    "$RUN/pan/xdg/data/riley-overview" "$RUN/pan/xdg/state" "$RUN/pan/runtime" "$RUN/pan/fakebin"
chmod 700 "$RUN/pan/runtime"
# riley-overview's schema where the installed overview keeps it ($XDG_DATA_HOME/riley-overview/config.xsd)
cp "$XSD" "$RUN/data/riley-overview/config.xsd"
cp "$XSD" "$RUN/pan/xdg/data/riley-overview/config.xsd"

rc() {
    env -i PATH=/usr/bin:/bin HOME="$RUN/home" XDG_CONFIG_HOME="$RUN/home/.config" XDG_DATA_HOME="$RUN/data" \
        XDG_DATA_DIRS="$RUN/none" LANG=C.UTF-8 RILEY_SETTINGS_SHARE="$SHARE" "$BIN" "$@"
}
valid() { xmllint --noout --schema "$XSD" "$1" >/dev/null 2>"$RUN/xmllint.err"; }

# ---- build ----------------------------------------------------------------------------------------------------
LOG=$RUN/build.log
if cmake -S "$REPO" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRILEY_PLANT_FAULT_OVERVIEW="$FAULT" >"$LOG" 2>&1 \
    && ninja -C "$BUILD_DIR" -j8 riley-config >>"$LOG" 2>&1 && [ -x "$BIN" ]; then
    pass "build (RILEY_PLANT_FAULT_OVERVIEW=$FAULT) gives $BIN"
    built=1
else
    fail "build (RILEY_PLANT_FAULT_OVERVIEW=$FAULT) gives $BIN" "$(grep -m1 -iE 'error' "$LOG" | cut -c1-200) (log: $LOG)"
    built=0
fi
need_bin() { [ "$built" = 1 ] || { fail "$1" "riley-config not built"; return 1; }; }

# ---- the expected files have not drifted, and riley-overview's schema takes them --------------------------------
declare -A SUM=(
    [o1.config.xml]=8581ccf44ceacd15a3d299a85967ca68b62926793ec496a885383399bb4d35bd
    [o2.config.xml]=9cef31efcbc593f8933b8624e3be53b04c7c8fdbfcd4343905c518086cdac8e1
    [o4.config.xml]=335c2df035593d6785fd463b3b6cf936f73019eb85fc0f2258acc3b3e970df68
    [p1.config.xml]=8a5ea8c65e40f7edf9767503f5f538ed790b536660ccb9f1cdee3756d1c3a571
    [p2.config.xml]=a506c25929c33827609805aa58c522766850d0f42c4491d8ad6312997451ed71
)
for f in $(printf '%s\n' "${!SUM[@]}" | sort); do
    got=$(sha256sum "$OV/expected/$f" 2>/dev/null | cut -d' ' -f1)
    if [ "$got" = "${SUM[$f]}" ]; then pass "overview/expected/$f unchanged (sha256)"
    else fail "overview/expected/$f unchanged (sha256)" "got ${got:-no file}, want ${SUM[$f]}"; fi
    if valid "$OV/expected/$f"; then pass "xmllint: overview/expected/$f is valid against riley-overview's config.xsd"
    else fail "xmllint: overview/expected/$f is valid against riley-overview's config.xsd" "$(first_line "$RUN/xmllint.err")"; fi
done

# ---- the description: the page and its five options ------------------------------------------------------------
label="description: validate accepts the real settings.xml"
if need_bin "$label"; then
    if rc validate "$DESC" >/dev/null 2>"$RUN/err"; then pass "$label"; else fail "$label" "$(first_line "$RUN/err")"; fi
fi
got=$(python3 -I - "$DESC" <<'PY'
import sys, xml.etree.ElementTree as ET
for p in ET.parse(sys.argv[1]).iter("page"):
    if p.get("id") == "overview":
        print(p.get("title"), "|", " ".join(o.get("key") for o in p.iter("option")))
PY
)
want='Overview | overview.click overview.click_key overview.hot_corner overview.hot_corner_screens overview.hot_corner_delay'
if [ "$got" = "$want" ]; then pass "description: page overview holds its five options in order"
else fail "description: page overview holds its five options in order" "got: ${got:-no page overview}"; fi
for want in 'overview.click|choice|fullscreen|||fullscreen maximize focus|riley-overview|overview-restart|Click on a window' \
            'overview.click_key|text|F||||riley-overview|overview-restart|Key for the highlighted window' \
            'overview.hot_corner|choice|top-left|||top-left top-right bottom-left bottom-right off|riley-overview|overview-restart|Hot corner' \
            'overview.hot_corner_screens|text|all||||riley-overview|overview-restart|Hot corner screens' \
            'overview.hot_corner_delay|slider|150|50|1000||riley-overview|overview-restart|Hot corner delay'; do
    key=${want%%|*}
    label="description: $key is ${want#*|}"
    got=$(python3 -I - "$DESC" "$key" <<'PY'
import sys, xml.etree.ElementTree as ET
for o in ET.parse(sys.argv[1]).iter("option"):
    if o.get("key") == sys.argv[2]:
        ch = " ".join(c.get("value") for c in o.findall("choice"))
        print("|".join([o.get("key"), o.get("type", ""), o.get("default", ""), o.get("min", ""), o.get("max", ""), ch,
                        o.get("target", ""), o.get("action", ""), o.get("label", "")]))
PY
)
    if [ "$got" = "$want" ]; then pass "$label"; else fail "$label" "got: ${got:-no such option}"; fi
done

# ---- the writer -----------------------------------------------------------------------------------------------
write_case() {   # case existing-or-empty
    local case=$1 in=$2 out=$RUN/w/$1.xml label
    label="writer riley-overview: overview/inputs/$case${in:+ over $(basename "$in")} gives overview/expected/$case.config.xml byte for byte"
    need_bin "$label" || return
    rm -f "$out"
    if [ -n "$in" ]; then cp "$in" "$out"; set -- --in "$in"; else set --; fi
    if ! rc write riley-overview --description "$DESC" --values "$OV/inputs/$case.values.xml" --out "$out" "$@" 2>"$RUN/err"
    then fail "$label" "write failed: $(first_line "$RUN/err")"
    elif cmp "$out" "$OV/expected/$case.config.xml" >"$RUN/cmp" 2>&1; then pass "$label"
    else fail "$label" "$(first_line "$RUN/cmp"); got: $(tr '\n' ' ' <"$out" | cut -c1-200)"; fi
    if [ -e "$out" ]; then
        if valid "$out"; then pass "xmllint: the $case file the writer left is valid against riley-overview's config.xsd"
        else fail "xmllint: the $case file the writer left is valid against riley-overview's config.xsd" "$(first_line "$RUN/xmllint.err")"; fi
    fi
}
write_case o1 ""
write_case o2 "$OV/inputs/o2.existing.xml"
write_case o4 "$OV/inputs/o4.existing.xml"

label="writer riley-overview: no Overview key set (o3) leaves an existing config.xml byte for byte"
if need_bin "$label"; then
    out=$RUN/w/o3.xml
    cp "$OV/inputs/o2.existing.xml" "$out"
    if ! rc write riley-overview --description "$DESC" --values "$OV/inputs/o3.values.xml" --out "$out" \
            --in "$OV/inputs/o2.existing.xml" 2>"$RUN/err"
    then fail "$label" "write failed: $(first_line "$RUN/err")"
    elif cmp -s "$out" "$OV/inputs/o2.existing.xml"; then pass "$label"
    else fail "$label" "the file changed"; fi
fi
label="writer riley-overview: no Overview key set (o3) makes no config.xml where there is none"
if need_bin "$label"; then
    out=$RUN/w/o3-none.xml
    if ! rc write riley-overview --description "$DESC" --values "$OV/inputs/o3.values.xml" --out "$out" 2>"$RUN/err"
    then fail "$label" "write failed: $(first_line "$RUN/err")"
    elif [ -e "$out" ]; then fail "$label" "a file was made"
    else pass "$label"; fi
fi

refuse() {   # label values existing-or-empty
    local label=$1 values=$2 in=$3 out=$RUN/w/refused.xml
    need_bin "$label" || return
    rm -f "$out"
    if [ -n "$in" ]; then cp "$in" "$out"; set -- --in "$in"; else set --; fi
    rc write riley-overview --description "$DESC" --values "$values" --out "$out" "$@" >"$RUN/out" 2>"$RUN/err"
    local code=$? lines
    lines=$(wc -l <"$RUN/err")
    if [ "$code" != 1 ]; then fail "$label" "exit $code, want 1"
    elif grep -q 'no writer for target' "$RUN/err"; then fail "$label" "refused only because there is no writer"
    elif [ "$lines" != 1 ]; then fail "$label" "stderr has $lines lines, want 1: $(first_line "$RUN/err")"
    elif [ -n "$in" ] && ! cmp -s "$out" "$in"; then fail "$label" "the existing file changed"
    elif [ -z "$in" ] && [ -e "$out" ]; then fail "$label" "a file was made"
    else pass "$label"; fi
}
refuse "writer riley-overview: refuses the screen name \"#2\" (r1): exit 1, one line, no file" "$OV/inputs/r1.values.xml" ""
refuse "writer riley-overview: refuses the click key FF (r2): exit 1, one line, no file" "$OV/inputs/r2.values.xml" ""
refuse "writer riley-overview: refuses over a config.xml that is not well-formed (r3), which stays as it was" \
    "$OV/inputs/o2.values.xml" "$OV/inputs/r3.existing.xml"
refuse "writer riley-overview: refuses over a config.xml the overview's schema rejects (r4, margin 999), which stays" \
    "$OV/inputs/o2.values.xml" "$OV/inputs/r4.existing.xml"
refuse "writer riley-overview: refuses over a file whose root is not <overview> (r5), which stays as it was" \
    "$OV/inputs/o2.values.xml" "$OV/inputs/r5.existing.xml"

label="writer riley-overview: no temporary file left next to the target"
if need_bin "$label"; then
    left=$(find "$RUN/w" -mindepth 1 ! -name '*.xml' -printf '%f ')
    if [ -z "$left" ]; then pass "$label"; else fail "$label" "left: $left"; fi
fi

# ---- the page in the real panel, with the restart run through a fake systemctl ----------------------------------
cat >"$RUN/pan/fakebin/systemctl" <<'SH'
#!/bin/sh
printf '%s\n' "$*" >>"$FAKE_SYSTEMCTL_LOG"
SH
chmod +x "$RUN/pan/fakebin/systemctl"
: >"$RUN/pan/systemctl.log"
label="panel harness: build (RILEY_PLANT_FAULT_OVERVIEW=$FAULT) gives $PAN"
if cmake -S "$OV/panel" -B "$PAN_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRILEY_PLANT_FAULT_OVERVIEW="$FAULT" \
        >"$RUN/pan-build.log" 2>&1 && ninja -C "$PAN_BUILD" -j8 tst_overview >>"$RUN/pan-build.log" 2>&1 && [ -x "$PAN" ]
then
    pass "$label"
    env -i PATH="$RUN/pan/fakebin:/usr/bin:/bin" HOME="$RUN/pan/home" XDG_CONFIG_HOME="$RUN/pan/home/.config" \
        XDG_CACHE_HOME="$RUN/pan/xdg/cache" XDG_DATA_HOME="$RUN/pan/xdg/data" XDG_DATA_DIRS="$RUN/none" \
        XDG_STATE_HOME="$RUN/pan/xdg/state" XDG_RUNTIME_DIR="$RUN/pan/runtime" LANG=C.UTF-8 LC_ALL=C.UTF-8 \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QUICK_CONTROLS_STYLE=Basic QML_DISABLE_DISK_CACHE=1 \
        RILEY_SETTINGS_SHARE="$SHARE" RILEY_SETTINGS_ACTION_LOG="$RUN/pan/actions.log" \
        FAKE_SYSTEMCTL_LOG="$RUN/pan/systemctl.log" OVERVIEW_EXPECTED="$OV/expected" \
        timeout 120 "$PAN" >"$RUN/pan.log" 2>"$RUN/pan.err"
    code=$?
    while IFS= read -r line; do
        case $line in
            "PASS "*) echo "$line"; passed=$((passed + 1)) ;;
            "FAIL "*) echo "$line"; failed=$((failed + 1)) ;;
            "     "*) echo "$line" ;;
        esac
    done <"$RUN/pan.log"
    n=$(grep -c '^\(PASS\|FAIL\) ' "$RUN/pan.log")
    if grep -qx "DONE 14" "$RUN/pan.log" && [ "$n" = 14 ]; then pass "panel harness ran its 14 checks (exit $code)"
    else fail "panel harness ran its 14 checks" "exit $code, $n checks; stderr: $(first_line "$RUN/pan.err")"; fi
    found=$(cd "$RUN/pan/home" && find . -type f | sort | tr '\n' ' ')
    want='./.config/riley-overview/config.xml ./.config/riley/settings.xml '
    if [ "$found" = "$want" ]; then pass "panel: the throwaway home holds only settings.xml and config.xml"
    else fail "panel: the throwaway home holds only settings.xml and config.xml" "found: $found"; fi
else
    fail "$label" "$(grep -m1 -iE 'error' "$RUN/pan-build.log" | cut -c1-200) (log: $RUN/pan-build.log)"
fi

# ---- nothing written outside the files named on the command line ----------------------------------------------
label="the throwaway home is still empty"
if need_bin "$label"; then
    found=$(find "$RUN/home" -mindepth 1 -printf '%P ')
    if [ -z "$found" ]; then pass "$label"; else fail "$label" "found: $found"; fi
fi
real_after=$(stamp "$REAL_OV")
if [ "$real_before" = "$real_after" ]; then pass "Marc's real riley-overview config.xml untouched ($real_after)"
else fail "Marc's real riley-overview config.xml untouched" "before: $real_before, after: $real_after"; fi

echo "$passed passed, $failed failed"
[ "$FAULT" = ON ] && echo "(planted-fault build: the o2 and r4 writer checks and panel checks 2, 3 and 4 (systemctl) must fail; the gate is proven when they do)"
[ "$failed" = 0 ]
