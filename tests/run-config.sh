#!/usr/bin/env bash
# Gate for M1, the configuration system. One command, one PASS/FAIL line per check, exit 0 only if all pass.
#
#   tests/run-config.sh                 build into /srv/devops/forge/build/riley-settings/gate and check
#   tests/run-config.sh --plant-fault   build with -DRILEY_PLANT_FAULT=ON into .../gate-fault: the shared atomic
#                                       write drops the second line of what it writes, so the writer checks must fail
#   BUILD_DIR=/some/dir tests/run-config.sh   another build directory
#
# The build must produce the binary at $BUILD_DIR/riley-config.
# Expected results: tests/expected/* (the seven expected-* files are Riley's settings/tests/ files, copied byte for
# byte from Riley commit b575a9a; roundtrip.values.xml was written by hand). Their sha256 sums are pinned below.
# All scratch files go under $BUILD_DIR/run; the tool runs with HOME pointed at an empty throwaway directory there.
set -u

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BASE=/srv/devops/forge/build/riley-settings
FAULT=OFF
for a in "$@"; do
    case $a in
        --plant-fault) FAULT=ON ;;
        -h|--help) sed -n '2,12p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown argument: $a" >&2; exit 2 ;;
    esac
done
if [ "$FAULT" = ON ]; then BUILD_DIR=${BUILD_DIR:-$BASE/gate-fault}; else BUILD_DIR=${BUILD_DIR:-$BASE/gate}; fi
RUN=$BUILD_DIR/run
BIN=$BUILD_DIR/riley-config
SHARE=$REPO/share/riley-settings
DESC=$SHARE/settings.xml

passed=0
failed=0
pass() { echo "PASS $1"; passed=$((passed + 1)); }
fail() { echo "FAIL $1"; [ -n "${2:-}" ] && printf '     %s\n' "$2"; failed=$((failed + 1)); }
first_line() { head -n 1 "$1" 2>/dev/null | cut -c1-200; }

mkdir -p "$BUILD_DIR"
rm -rf -- "$RUN"
mkdir -p "$RUN/home" "$RUN/broken" "$RUN/rt" "$RUN/w"

# Run the tool with an empty throwaway home, the repo's share directory, and nothing else from this session.
rc() {
    env -i PATH=/usr/bin:/bin HOME="$RUN/home" XDG_CONFIG_HOME="$RUN/home/.config" LANG=C.UTF-8 \
        RILEY_SETTINGS_SHARE="$SHARE" "$BIN" "$@"
}

# ---- build ----------------------------------------------------------------------------------------------------
LOG=$RUN/build.log
if cmake -S "$REPO" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRILEY_PLANT_FAULT="$FAULT" >"$LOG" 2>&1 \
    && ninja -C "$BUILD_DIR" -j8 >>"$LOG" 2>&1 && [ -x "$BIN" ]; then
    pass "build (RILEY_PLANT_FAULT=$FAULT) gives $BIN"
    built=1
else
    fail "build (RILEY_PLANT_FAULT=$FAULT) gives $BIN" "$(grep -m1 -iE 'error' "$LOG" | cut -c1-200) (log: $LOG)"
    built=0
fi

# ---- the expected files have not drifted ----------------------------------------------------------------------
declare -A SUM=(
    [expected-greeter.conf]=045efb5349b18a16f3c8b18dbf2b200703036a537d0fee3d3ffeddd68e973f34
    [expected-m1.lua]=65cfdae2f2e597af30299fca905b66a86dbad0cbb6ee0fdbf5057723cc3906da
    [expected-m3.lua]=809f90340d13a2f6c6e548412ce2e2cc38812421f16cb3efa1b488802e61520c
    [expected-m4.lua]=4a8973fb45b8f7abca6551a440f3ad60fa91781ed4d4f61dc71abe5366a54364
    [expected-m5-hyprpaper.conf]=8091fd32470f712521eee35c5a0bed564b1dff58ee11f304db8f2c4e4fe76dd3
    [expected-m7-idle.conf]=1fa3768be7bb83ffe7cab5eec6670bbbb6e9b08f9859fdd116c314b2eee397b4
    [expected-m8-idle.conf]=735604a1e3c1cb8a9545b98fc06117ba5fb9e2b675ec2d295db257ad45ed20e5
    [roundtrip.values.xml]=132e048f8a3e5f8db82dd47ff83e7e14c0200700298c331d7c4e153ee31216c7
)
for f in $(printf '%s\n' "${!SUM[@]}" | sort); do
    got=$(sha256sum "$REPO/tests/expected/$f" 2>/dev/null | cut -d' ' -f1)
    if [ "$got" = "${SUM[$f]}" ]; then pass "expected/$f unchanged (sha256)"
    else fail "expected/$f unchanged (sha256)" "got ${got:-no file}, want ${SUM[$f]}"; fi
done
extra=$(cd "$REPO/tests/expected" && for f in *; do [ -n "${SUM[$f]:-}" ] || printf '%s ' "$f"; done)
if [ -z "$extra" ]; then pass "tests/expected holds only the pinned files"
else fail "tests/expected holds only the pinned files" "not pinned: $extra"; fi

need_bin() { [ "$built" = 1 ] || { fail "$1" "riley-config not built"; return 1; }; }

# ---- schema: the real description is accepted, five broken copies are refused ----------------------------------
if need_bin "schema: validate accepts share/riley-settings/settings.xml"; then
    if rc validate "$DESC" >/dev/null 2>"$RUN/err"; then pass "schema: validate accepts share/riley-settings/settings.xml"
    else fail "schema: validate accepts share/riley-settings/settings.xml" "$(first_line "$RUN/err")"; fi
fi

# Make each broken copy by a text edit of one attribute in one <option> start tag of the real file, so the copy keeps
# every other byte; then parse both and confirm that they differ in exactly that one attribute.
cat >"$RUN/break.py" <<'PY'
import re, sys, xml.etree.ElementTree as ET
src, out = sys.argv[1], sys.argv[2]
raw = open(src, encoding="utf-8").read()
root = ET.fromstring(raw)
opts = list(root.iter("option"))
blank = lambda m: re.sub(r"[^\n]", " ", m.group())
masked = re.sub(r"<!--.*?-->|<!\[CDATA\[.*?\]\]>|<\?.*?\?>", blank, raw, flags=re.S)   # same offsets as raw
tags = list(re.finditer(r"<option\b(?:[^>\"']|\"[^\"]*\"|'[^']*')*>", masked))
if len(tags) != len(opts) or len(opts) < 2:
    sys.exit(f"found {len(opts)} options and {len(tags)} option tags; need two or more, equal")

def edit(i, name, new):
    m = tags[i]
    tag = raw[m.start():m.end()]
    attrs = re.compile(r"(\s+)([^\s=/>]+)(\s*=\s*)(\"[^\"]*\"|'[^']*')").finditer(tag, len("<option"))
    a = next((x for x in attrs if x.group(2) == name), None)   # attribute by attribute, never inside a value
    if not a:
        sys.exit(f"option {i} has no {name} attribute")
    tag = tag[:a.start()] + ("" if new is None else a.group(1) + name + a.group(3) + '"' + new + '"') + tag[a.end():]
    return raw[:m.start()] + tag + raw[m.end():]

def diffs(a, b, path="/"):
    d = []
    if a.tag != b.tag or (a.text or "").strip() != (b.text or "").strip() or (a.tail or "").strip() != (b.tail or "").strip():
        d.append((path, a.tag, None))
    for k in set(a.attrib) | set(b.attrib):
        if a.attrib.get(k) != b.attrib.get(k):
            d.append((path, a.tag, k))
    if len(a) != len(b):
        d.append((path, a.tag, "children"))
    for n, (x, y) in enumerate(zip(a, b)):
        d += diffs(x, y, f"{path}{n}/")
    return d

def first(pred, what):
    i = next((n for n, o in enumerate(opts) if pred(o)), None)
    if i is None:
        sys.exit(f"no option {what}")
    return i

def bigger(v):
    return str(int(v) + 1) if re.fullmatch(r"-?\d+", v) else repr(float(v) + 1)

cases = []
cases.append(("unknown-type", 0, "type", "dial"))
cases.append(("missing-key", 0, "key", None))
cases.append(("duplicate-key", 1, "key", opts[0].get("key", "")))
i = first(lambda o: o.get("type") in ("slider", "number") and o.get("max") and o.get("default"),
          "of type slider or number with max and default")
cases.append(("default-out-of-range", i, "default", bigger(opts[i].get("max"))))
i = first(lambda o: o.get("action"), "with an action")
cases.append(("unknown-action", i, "action", "reboot-machine"))

where = {id(o): n for n, o in enumerate(opts)}
for name, i, attr, new in cases:
    text = edit(i, attr, new)
    d = diffs(root, ET.fromstring(text))
    if len(d) != 1 or d[0][2] != attr or d[0][1] != "option":
        sys.exit(f"{name}: the copy differs in {d}, want only {attr} of option {i}")
    open(f"{out}/{name}.xml", "w", encoding="utf-8").write(text)
PY
made=0
if python3 -I "$RUN/break.py" "$DESC" "$RUN/broken" 2>"$RUN/break.err"; then made=1; fi
for name in unknown-type missing-key duplicate-key default-out-of-range unknown-action; do
    label="schema: validate refuses a copy with one $name"
    need_bin "$label" || continue
    if [ "$made" != 1 ]; then fail "$label" "broken copies not made: $(first_line "$RUN/break.err")"; continue; fi
    rc validate "$RUN/broken/$name.xml" >/dev/null 2>"$RUN/err"; code=$?
    lines=$(grep -c '' "$RUN/err")
    if [ "$code" = 1 ] && [ "$lines" = 1 ]; then pass "$label"
    else fail "$label" "exit $code, $lines stderr lines (want exit 1, one line): $(first_line "$RUN/err")"; fi
done

# ---- values: round-trip and unknown keys ----------------------------------------------------------------------
label="values: set changes one value, every other byte and comment kept"
if need_bin "$label"; then
    cp "$REPO/tests/inputs/roundtrip.values.xml" "$RUN/rt/values.xml"
    if ! rc set "$RUN/rt/values.xml" power.lock_after 900 2>"$RUN/err"; then
        fail "$label" "set failed: $(first_line "$RUN/err")"
    elif cmp "$RUN/rt/values.xml" "$REPO/tests/expected/roundtrip.values.xml" >"$RUN/cmp" 2>&1; then pass "$label"
    else fail "$label" "$(first_line "$RUN/cmp")"; fi
    left=$(find "$RUN/rt" -mindepth 1 ! -name values.xml -printf '%f ')
    if [ -z "$left" ]; then pass "values: set leaves no temporary file behind"
    else fail "values: set leaves no temporary file behind" "left: $left"; fi
fi

label="values: check-values reports the unknown key old.removed_key and still exits 0"
if need_bin "$label"; then
    rc check-values "$DESC" "$REPO/tests/inputs/roundtrip.values.xml" >/dev/null 2>"$RUN/err"; code=$?
    if [ "$code" = 0 ] && grep -q 'unknown key: old.removed_key' "$RUN/err"; then pass "$label"
    else fail "$label" "exit $code: $(first_line "$RUN/err")"; fi
fi

# ---- writers: each case reproduces Riley's expected file byte for byte ----------------------------------------
# case            target         expected file
CASES="m1-displays    hyprland-lua   expected-m1.lua
m3-appearance  hyprland-lua   expected-m3.lua
m4-startup     hyprland-lua   expected-m4.lua
m5-hyprpaper   hyprpaper      expected-m5-hyprpaper.conf
m7-idle        hypridle       expected-m7-idle.conf
m8-idle        hypridle       expected-m8-idle.conf
greeter        greeter-sway   expected-greeter.conf"
while read -r case target want; do
    vals=$REPO/tests/inputs/$case.values.xml
    label="values: check-values accepts inputs/$case.values.xml with no unknown key"
    if need_bin "$label"; then
        rc check-values "$DESC" "$vals" >/dev/null 2>"$RUN/err"; code=$?
        if [ "$code" = 0 ] && ! grep -q 'unknown key' "$RUN/err"; then pass "$label"
        else fail "$label" "exit $code: $(first_line "$RUN/err")"; fi
    fi
    label="writer $target: $case gives expected/$want byte for byte"
    need_bin "$label" || continue
    in=()
    if [ -f "$REPO/tests/inputs/$case.in" ]; then
        cp "$REPO/tests/inputs/$case.in" "$RUN/w/$case.in"
        in=(--in "$RUN/w/$case.in")
    fi
    out=$RUN/w/$case.out
    if ! rc write "$target" --description "$DESC" --values "$vals" --out "$out" "${in[@]}" 2>"$RUN/err"; then
        fail "$label" "write failed: $(first_line "$RUN/err")"
    elif cmp "$out" "$REPO/tests/expected/$want" >"$RUN/cmp" 2>&1; then pass "$label"
    else fail "$label" "$(first_line "$RUN/cmp")"; fi
done <<<"$CASES"

# ---- nothing written outside the files named on the command line ----------------------------------------------
label="the throwaway home is still empty (no ~/.config read or written)"
if need_bin "$label"; then
    found=$(find "$RUN/home" -mindepth 1 -printf '%P ')
    if [ -z "$found" ]; then pass "$label"
    else fail "$label" "found: $found"; fi
fi

echo "$passed passed, $failed failed"
[ "$FAULT" = ON ] && echo "(planted-fault build: the writer checks must fail; the gate is proven when they do)"
[ "$failed" = 0 ]
