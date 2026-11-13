#!/usr/bin/env bash
# Gate for M4, the switch. One command, one PASS/FAIL line per check, exit 0 only if all pass. The contract it
# checks: docs/switch-contract.md.
#
#   tests/run-switch.sh                 build tests/switch/ (the harness plus the repo) into
#                                       /srv/devops/forge/build/riley-settings/switch and check
#   tests/run-switch.sh --plant-fault   build with -DRILEY_PLANT_FAULT_SWITCH=ON into .../switch-fault (the importer drops
#                                       its last key, the layout picker puts the second layout's variant on the first,
#                                       the printer options leave out the last one) and check Riley's launcher from a
#                                       copy that starts the Qt panel without --apply-actions (one sed line); every group with a fault must then have
#                                       a failing check; it exits 0 only when each does
#   BUILD_DIR=/some/dir tests/run-switch.sh   another build directory
#   RILEY_REPO=/path/to/riley tests/run-switch.sh   where Riley's launcher is read (default /home/marcm/git/riley)
#
# Everything runs with a throwaway home under $BUILD_DIR/run, `env -i`, traps first on PATH for the desktop's
# commands, and actions recorded, never run. --import and --check run with no display at all (no QT_QPA_PLATFORM,
# no WAYLAND_DISPLAY): if either opened a window it would fail. Marc's live files are read only: copied into a
# throwaway home for the import-real group, and compared with what the writers make from the imported values; their
# content is never written into the repo. Expected results: tests/switch/expected/*, worked out by hand from Riley's
# code before the code existed; their sha256 sums, and those of the seeds, fixtures and fakes, are pinned below.
set -u
ulimit -c 0   # a program that aborts (a window with no display) leaves no core dump behind

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BASE=/srv/devops/forge/build/riley-settings
FAULT=OFF
for a in "$@"; do
    case $a in
        --plant-fault) FAULT=ON ;;
        -h|--help) sed -n '2,22p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown argument: $a" >&2; exit 2 ;;
    esac
done
if [ "$FAULT" = ON ]; then BUILD_DIR=${BUILD_DIR:-$BASE/switch-fault}; else BUILD_DIR=${BUILD_DIR:-$BASE/switch}; fi
RILEY_REPO=${RILEY_REPO:-/home/marcm/git/riley}
RUN=$BUILD_DIR/run
SW=$REPO/tests/switch
EXPECTED=$SW/expected
GROUPS_ALL="gate model import-fixture refuse import-real start install launcher pages layout printers untouched"
FAULT_GROUPS="import-fixture import-real launcher layout printers"

passed=0
failed=0
declare -A GP=() GF=()
pass() { echo "PASS $1"; passed=$((passed + 1)); local g=${1%%:*}; GP[$g]=$((${GP[$g]:-0} + 1)); }
fail() {
    echo "FAIL $1"; [ -n "${2:-}" ] && printf '%s\n' "$2" | head -n 12 | sed 's/^/     /'
    failed=$((failed + 1)); local g=${1%%:*}; GF[$g]=$((${GF[$g]:-0} + 1))
}
verdict() { if [ "$2" = 0 ]; then pass "$1"; else fail "$1" "${3:-}"; fi; }   # verdict NAME STATUS [DETAIL]
same_file() {   # same_file NAME GOT WANT_TEXT_FILE [@HOME@ value]
    local want
    if [ ! -e "$2" ]; then fail "$1" "no file $2"; return; fi
    want=$(sed "s|@HOME@|${4:-@HOME@}|g" "$3"; printf x); want=${want%x}
    if [ "$(cat "$2"; printf x)" = "${want}x" ]; then pass "$1"
    else fail "$1" "$(diff <(printf '%s' "$want") "$2" | head -n 6)"; fi
}
first_line() { head -n 1 "$1" 2>/dev/null | cut -c1-200; }
listing() { (cd "$1" 2>/dev/null && find . \( -type f -o -type l \) -printf '%P %s %T@\n' | sort); }   # files, size, mtime
warnings_in() { grep -E '\.qml|qml:|QQml|TypeError|ReferenceError|Binding loop' "$1" | head -n 3; }

# ---- Marc's real files: their state now, compared at the end. Read only, never written. -------------------------
REAL_CFG=${XDG_CONFIG_HOME:-$HOME/.config}
REAL_HYPR=$HOME/.config/hypr          # where Riley's GTK panel keeps its model (core.py: Path.home() / ".config/hypr")
REAL_FILES="$REAL_HYPR/settings.json $REAL_HYPR/settings.lua $REAL_HYPR/keys-user.lua $REAL_HYPR/idle.conf $REAL_HYPR/hypridle.conf
    $REAL_HYPR/hyprpaper.conf $REAL_HYPR/float-mode.json $REAL_HYPR/current-theme $REAL_HYPR/current-background
    $REAL_HYPR/wallpapers.json $HOME/.config/riley/desktop.json $REAL_CFG/riley/settings.xml
    $REAL_CFG/riley/fonts.toml $REAL_CFG/riley/weather.toml $REAL_CFG/riley/greeter-sway.conf
    $REAL_CFG/nwg-dock-hyprland/riley.conf $REAL_CFG/riley-dock/config.xml $HOME/.cache/nwg-dock-pinned
    $HOME/.cups/lpoptions $HOME/.local/bin/riley-settings $HOME/.local/bin/riley-settings-qt"
state() { for f in $REAL_FILES; do if [ -e "$f" ] || [ -L "$f" ]; then stat -c '%n %y %s' "$f"; else echo "$f absent"; fi; done; }
REAL_BEFORE=$(state)
REPO_BEFORE=$(git -C "$REPO" status --porcelain 2>/dev/null)

case $RUN in /srv/devops/*) ;; *) echo "BUILD_DIR must be under /srv/devops" >&2; exit 2 ;; esac
mkdir -p "$BUILD_DIR"
rm -rf -- "$RUN"
mkdir -p "$RUN/home" "$RUN/share" "$RUN/trap" "$RUN/fakebin" "$RUN/fake/canned" "$RUN/fake/state" "$RUN/runtime"
chmod 700 "$RUN/runtime"
touch "$RUN/.riley-switch-run"   # the harness wipes $RUN/home only when this marker is next to it

# ---- build ----------------------------------------------------------------------------------------------------
LOG=$RUN/build.log
QT=$BUILD_DIR/riley-settings-qt
CFGTOOL=$BUILD_DIR/riley-config
TST=$BUILD_DIR/tst_switch
if cmake -S "$SW" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/nonexistent-prefix \
        -DRILEY_PLANT_FAULT=OFF -DRILEY_PLANT_FAULT_PANEL=OFF -DRILEY_PLANT_FAULT_PAGES=OFF \
        -DRILEY_PLANT_FAULT_SWITCH="$FAULT" >"$LOG" 2>&1 \
    && ninja -C "$BUILD_DIR" -j8 >>"$LOG" 2>&1 && [ -x "$TST" ] && [ -x "$QT" ] && [ -x "$CFGTOOL" ]; then
    pass "gate: build (RILEY_PLANT_FAULT_SWITCH=$FAULT) gives tst_switch, riley-settings-qt and riley-config"
    built=1
else
    fail "gate: build (RILEY_PLANT_FAULT_SWITCH=$FAULT) gives tst_switch, riley-settings-qt and riley-config" \
        "$(grep -m1 -iE 'error' "$LOG" | cut -c1-200) (log: $LOG)"
    built=0
fi
if grep -qE '^option\(RILEY_PLANT_FAULT_SWITCH ' "$REPO/CMakeLists.txt"; then
    pass "gate: CMakeLists.txt has the option RILEY_PLANT_FAULT_SWITCH"
else fail "gate: CMakeLists.txt has the option RILEY_PLANT_FAULT_SWITCH" "no option(RILEY_PLANT_FAULT_SWITCH ...) line"; fi

# ---- the expected files, seeds, fixtures and fakes have not drifted -------------------------------------------
declare -A SUM=(
    [expected/expected-m1.lua]=65cfdae2f2e597af30299fca905b66a86dbad0cbb6ee0fdbf5057723cc3906da
    [expected/expected-m3.lua]=809f90340d13a2f6c6e548412ce2e2cc38812421f16cb3efa1b488802e61520c
    [expected/expected-m4.lua]=4a8973fb45b8f7abca6551a440f3ad60fa91781ed4d4f61dc71abe5366a54364
    [expected/expected-m7-idle.conf]=1fa3768be7bb83ffe7cab5eec6670bbbb6e9b08f9859fdd116c314b2eee397b4
    [switch/expected/check-install.txt]=95c7ea66f74973746bfc80babae0d2600c10cf317e61d14ffc166fe8a3a24579
    [switch/expected/check-m1.txt]=353db2f50ee4d3976c3df17abd07e03efcbccc60c3309f5c597e1ea698ad47cc
    [switch/expected/check-m3.txt]=3a5147dc3d72448b4a8964b5564509027b5a745cd72b4cc1640719fc1023253c
    [switch/expected/check-m4.txt]=08d46e892aa041f92cf6df44f3f785441863f416684571659e195a66bd4f1817
    [switch/expected/check-real.txt]=f3af75a73c9222f85841793d2eb60b8679066ffaaf6cc0085266e687e5cccfd1
    [switch/expected/import-m1.values.xml]=a033389707bc0b45a14afefe39504975e3d2f8138d7af86e908e0388fcbb4181
    [switch/expected/import-m3.values.xml]=f4c8fbccd3b450f22badd8460dd642b6e30c424b0c9dff743046648b3b1f2a12
    [switch/expected/import-m4.values.xml]=ef2263eb2a33b1dcc790ec74f4ce43ef9f87279f1333fa4029d414dcdb23f067
    [switch/expected/layout-actions.log]=48464317252001d726a5b385c9b4b07e0de0dda3ae1a7de87012b2fae17c269e
    [switch/expected/layout-model-1.json]=e9f3c5493d84ea9222cace951ec4f08b0ca2f212055e657c1263bff23cf840a2
    [switch/expected/layout-model-2.json]=cb89abb6e56342469bf0a8a235d5578e48ba5c2d40f79114c1117874816edcf2
    [switch/expected/layout-settings-1.lua]=38131d00bca75d6f7c72ab62bb8dfe5bc104c69c547c80d981164a2a2cd2bbd6
    [switch/expected/layout-settings-2.lua]=09509161b8072466ce222500f87dbcc7de929298802c6e6bfbbab6295054949f
    [switch/expected/layout-values-1.xml]=f1aa3665b369a9fba08a6f296dea920c8744657d28b38cfcbd3b1ea5f0560fc1
    [switch/expected/layout-values-2.xml]=9b4402541d906e29b6c1703675394bd938fd16dd438504b27f3c3ef5a9f75826
    [switch/expected/layout-values-3.xml]=9ef3447d9ad1e378a6f0ba572161271c18356521c5b482d4603b3132aa9c8058
    [switch/expected/model-m1.json]=156eb0bec0374e5f046238f898208713dfaa4cfa9c557e843ae0d74cdb8611fe
    [switch/expected/model-m3.json]=25799e3dd92a4c8b1d862380bde844c8ff653c811415e1145dd5ef09ea4c3051
    [switch/expected/model-m4.json]=07afb926f8e93813226c216fe727a7dc3a8abe9325b4fadfa31ea6dd10568c61
    [switch/expected/printers-option-calls.log]=0f011b5e4f39f06d393d22c1286115f51db7ddffa1a1bfcb84431945bb4edec0
    [switch/fakes/ipptool]=3f9868f0704403fed153dbf3cfa56ebd2d4823105729307d2cbea633cb2bf1b2
    [switch/fakes/lpoptions]=6f2fa675dffdf99e5c0c212c74578ba5b4c42d3d1494044f7d311a43d871eadb
    [switch/fixtures/fixture-m1.json]=04852b3ff29bff3e548d7e8f6243fca8001cfe40cef3973a80f8c0a979761b4f
    [switch/fixtures/fixture-m3.json]=e1625e48c108924d01d72fc8ca11c40f02ada04f4f37ae985f613dc9fbd5b886
    [switch/fixtures/fixture-m4.json]=55cac20eeeef7b22b2dbd7635d089808a85c4ff76faa0660f9001c4043727ab8
    [switch/fixtures/ipptool-Office.txt]=6e4ee512a2d35643abbe81f43d91174ba1976696c2dff72ecb485a19507fa5f6
    [switch/fixtures/ipptool-RileyGate.txt]=fdec04a2c6dd1b9b1565fd8ddeb05ec3f725edb49d65620663d22ccbcbc638aa
    [switch/fixtures/m1-home/.config/hypr/hyprpaper.conf]=0e8599cbb6ab36844041bd4791e89ea7acc2fb962c86487be93fb75fbaae3ecd
    [switch/fixtures/m3-home/.cache/nwg-dock-pinned]=0d3ed81ebcf566393290ac7da681598ed2e26666f43fad8a70e4ddade6b81dcf
    [switch/fixtures/m3-home/.config/hypr/current-theme]=f77de6509df053ecc9a3ddaa14bc6096d7da5086832083f974efa9beb218dc5c
    [switch/fixtures/m3-home/.config/hypr/float-mode.json]=9ae816c7ee493d6acd03f4ef904d4c5289c291e95d9b0911c48d89ecd6365887
    [switch/fixtures/m3-home/.config/hypr/hyprpaper.conf]=98fad71dfee10f5a354c339d45f7631b48b5ec05084f659f703c3c3d134080a4
    [switch/fixtures/m3-home/.config/hypr/wallpapers.json]=c6050005a65401c6a074d676ecdc0a87cc02ae9cdec7672a73495bc6047b3a24
    [switch/fixtures/m3-home/.config/nwg-dock-hyprland/riley.conf]=efa67407dff6b090565eec2a5306ad58a41465b5b90290590e8170a7f1ef188f
    [switch/fixtures/m3-home/.config/riley/desktop.json]=6168c349570afb8f87d3a3bf30cc8df690fd584a64421a5aea38554a6fd7905f
    [switch/fixtures/m3-home/.config/riley/fonts.toml]=51b6d39d24e303d346e90d300de9e55645cb584c5e8c12543cffdb78dc71a271
    [switch/fixtures/m3-home/.config/riley/weather.toml]=674924d970e516b27f5f5afef650d6013822460386c2e39451eda527d9bc2209
    [switch/fixtures/m4-home/.config/hypr/hyprpaper.conf]=68d9d247584cedd08b1007729aad0ce0bcc61fe2b3763fac7fb5b95d45be0256
    [switch/seeds/layout-values.xml]=d2fda31d58a7095aa56c250c8e308b436dc9bce7e8efcb6c96b4743135f3a126
)
for f in $(printf '%s\n' "${!SUM[@]}" | sort); do
    got=$(sha256sum "$REPO/tests/$f" 2>/dev/null | cut -d' ' -f1)
    if [ "$got" = "${SUM[$f]}" ]; then pass "gate: tests/$f unchanged (sha256)"
    else fail "gate: tests/$f unchanged (sha256)" "got ${got:-no file}, want ${SUM[$f]}"; fi
done
extra=$(cd "$SW" && find expected seeds fixtures fakes -type f | sort | while read -r f; do [ -n "${SUM[switch/$f]:-}" ] || printf '%s ' "$f"; done)
if [ -z "$extra" ]; then pass "gate: tests/switch/{expected,seeds,fixtures,fakes} hold only the pinned files"
else fail "gate: tests/switch/{expected,seeds,fixtures,fakes} hold only the pinned files" "not pinned: $extra"; fi

# ---- the description: riley-model replaces greeter-sway on the options of Riley's model ---------------------------
DESC=$REPO/share/riley-settings/settings.xml
if xmllint --noout --schema "$REPO/share/riley-settings/schema/settings.xsd" "$DESC" >"$RUN/xsd.err" 2>&1
then pass "gate: share/riley-settings/settings.xml validates against its schema"
else fail "gate: share/riley-settings/settings.xml validates against its schema" "$(grep -m1 -v 'fails to validate' "$RUN/xsd.err" | cut -c1-200)"; fi
MODEL_KEYS="display.monitors appearance.gaps_in appearance.gaps_out appearance.border_size appearance.rounding
    appearance.active_opacity appearance.inactive_opacity appearance.blur appearance.blur_size appearance.blur_passes
    appearance.shadow appearance.animations input.kb_layout input.kb_variant input.kb_options input.repeat_rate
    input.repeat_delay input.sensitivity input.accel_profile input.natural_scroll input.follow_mouse startup.apps
    power.screensaver_after power.lock_after power.screens_off_after power.sleep_after"
model_check=$(python3 -I - "$DESC" $MODEL_KEYS <<'EOF'
import sys, xml.etree.ElementTree as ET
want = set(sys.argv[2:])
bad = []
for o in ET.parse(sys.argv[1]).iter("option"):
    k, t = o.get("key"), (o.get("target") or "").split()
    if "greeter-sway" in t:
        bad.append(f"{k} still targets greeter-sway")
    if (k in want) != ("riley-model" in t):
        bad.append(f"{k}: target \"{' '.join(t)}\"" + (" lacks riley-model" if k in want else " has riley-model"))
    want.discard(k)
bad += [f"{k} is not in the description" for k in sorted(want)]
print("\n".join(bad))
EOF
)
if [ -z "$model_check" ]; then pass "gate: settings.xml: the 26 options of Riley's model target riley-model, no option targets greeter-sway"
else fail "gate: settings.xml: the 26 options of Riley's model target riley-model, no option targets greeter-sway" "$model_check"; fi

# ---- the throwaway world: share, traps, fakes, keyboard database ---------------------------------------------------
cp "$DESC" "$RUN/share/settings.xml"
cp -r "$REPO/share/riley-settings/schema" "$RUN/share/schema"
for c in hyprctl Hyprland hypr-settings system-config-printer gnome-control-center systemctl loginctl hypr-dock \
         hypr-theme hypr-theme.py hyprpaper hypridle hyprlock hypr-idle hypr-desktop-menu hypr-reload hypr-float-mode \
         riley-dock riley-dock-qt nwg-dock-hyprland riley-greeter-sync riley-desktop pkill killall sudo pkexec lpadmin \
         cupsenable cupsaccept ippfind ipptool lp lpstat lpoptions cancel notify-send quickshell qs waybar kitty \
         google-chrome xdg-open gio swaymsg sway python3 riley-settings; do
    printf '#!/bin/sh\necho "%s $*" >>"%s"\nexit 1\n' "$c" "$RUN/trap.log" >"$RUN/trap/$c"
    chmod +x "$RUN/trap/$c"
done
cp "$REPO"/tests/pages/fakes/* "$RUN/fakebin/"
cp "$SW"/fakes/* "$RUN/fakebin/"      # the M4 lpoptions replaces the M3 one; ipptool is new
chmod +x "$RUN"/fakebin/*
cp "$REPO"/tests/pages/fixtures/*.json "$REPO"/tests/pages/fixtures/*.txt "$SW"/fixtures/ipptool-*.txt "$RUN/fake/canned/"
EVDEV=/usr/share/X11/xkb/rules/evdev.lst
# Riley's keyboard_mouse.py layouts(): the "! layout" section, sorted by name ignoring case; and the contract's
# variant rule: the "! variant" lines "  <variant>  <layout>: <name>", in file order.
python3 -I - "$EVDEV" "$RUN" <<'EOF'
import re, sys
from pathlib import Path
f, out = Path(sys.argv[1]), Path(sys.argv[2])
layouts, variants, section = [], {}, None
if f.is_file():
    for line in f.read_text().splitlines():
        if line.startswith("!"):
            section = line[1:].strip()
            continue
        m = re.match(r"^\s+(\S+)\s+(.+)$", line)
        if section == "layout" and m:
            layouts.append((m.group(1), m.group(2).strip()))
        if section == "variant" and m:
            v = re.match(r"^(\S+):", m.group(2))
            if v:
                variants.setdefault(v.group(1), []).append(m.group(1))
codes = sorted(layouts, key=lambda x: x[1].lower()) or [("us", "English (US)")]
(out / "layouts.txt").write_text("".join(c + "\n" for c, _ in codes))
for code in ("ro", "de"):
    (out / f"variants-{code}.txt").write_text("".join(v + "\n" for v in variants.get(code, [])))
EOF

# Runs one of the programs with nothing of the session: HOME and XDG under $1, traps first on PATH, no display.
#   offline HOME TAG CMD...    (TAG names the action log: $RUN/actions-TAG.log)
offline() {
    local h=$1 tag=$2; shift 2
    env -i PATH="$RUN/trap:/usr/bin:/bin" HOME="$h" XDG_CONFIG_HOME="$h/.config" XDG_CACHE_HOME="$h/.cache" \
        XDG_DATA_HOME="$h/.local/share" XDG_RUNTIME_DIR="$RUN/runtime" TMPDIR="$RUN/tmp" LANG=C.UTF-8 LC_ALL=C.UTF-8 \
        RILEY_SETTINGS_SHARE="$RUN/share" RILEY_SETTINGS_ACTION_LOG="$RUN/actions-$tag.log" "$@"
}
mkdir -p "$RUN/tmp"

# A fixture home: Riley's fixture-mN.json as the old model, Riley's expected-mN.lua as settings.lua (m1: also Riley's
# expected-m7-idle.conf as idle.conf), then tests/switch/fixtures/mN-home with @HOME@ filled in.
make_home() {   # make_home DIR CASE
    local h=$1 n=$2
    rm -rf -- "$h"; mkdir -p "$h/.config/hypr" "$h/.cache"
    cp "$SW/fixtures/fixture-$n.json" "$h/.config/hypr/settings.json"
    cp "$REPO/tests/expected/expected-$n.lua" "$h/.config/hypr/settings.lua"
    [ "$n" = m1 ] && cp "$REPO/tests/expected/expected-m7-idle.conf" "$h/.config/hypr/idle.conf"
    cp -a "$SW/fixtures/$n-home/." "$h/"
    [ -e "$h/.config/riley/desktop.json" ] && sed -i "s|@HOME@|$h|g" "$h/.config/riley/desktop.json"
    [ "$n" = m3 ] && mkdir -p "$h/Desktop/Photos"
    return 0
}
declare -A KEYS=([m1]=5 [m3]=39 [m4]=3)

# ---- model: the riley-model writer, and the other writers, from the hand-worked import results -------------------
if [ "$built" = 1 ]; then
    for n in m1 m3 m4; do
        V=$RUN/model/import-$n.values.xml; mkdir -p "$RUN/model"
        sed "s|@HOME@|$RUN/model/home|g" "$EXPECTED/import-$n.values.xml" >"$V"
        for t in "riley-model:model-$n.json:$EXPECTED/model-$n.json" "hyprland-lua:$n.lua:$REPO/tests/expected/expected-$n.lua"; do
            IFS=: read -r target out want <<<"$t"
            offline "$RUN/model/home" model "$CFGTOOL" write "$target" --description "$DESC" --values "$V" \
                --out "$RUN/model/$out" >"$RUN/model/$out.err" 2>&1
            same_file "model: riley-config write $target from expected/import-$n.values.xml is ${want#"$REPO"/tests/}" \
                "$RUN/model/$out" "$want"
        done
    done
    offline "$RUN/model/home" model "$CFGTOOL" write hypridle --description "$DESC" --values "$RUN/model/import-m1.values.xml" \
        --out "$RUN/model/m1-idle.conf" >/dev/null 2>&1
    same_file "model: hypridle from expected/import-m1.values.xml is Riley's expected-m7-idle.conf" "$RUN/model/m1-idle.conf" \
        "$REPO/tests/expected/expected-m7-idle.conf"
    for n in m3 m4; do
        offline "$RUN/model/home" model "$CFGTOOL" write hyprpaper --description "$DESC" --values "$RUN/model/import-$n.values.xml" \
            --out "$RUN/model/$n-hyprpaper.conf" >/dev/null 2>&1
        same_file "model: hyprpaper from expected/import-$n.values.xml is fixtures/$n-home's hyprpaper.conf (hypr-theme.py set_bg)" \
            "$RUN/model/$n-hyprpaper.conf" "$SW/fixtures/$n-home/.config/hypr/hyprpaper.conf"
    done
fi

# ---- import-fixture: Riley's fixture models (and the files its pages read) imported, then --check -------------------
for n in m1 m3 m4; do
    [ "$built" = 1 ] || { fail "import-fixture: $n: not built"; continue; }
    H=$RUN/fx-$n/home
    make_home "$H" "$n"
    before=$(listing "$H")
    offline "$H" "import-$n" "$QT" --import >"$RUN/fx-$n/import.out" 2>"$RUN/fx-$n/import.err"
    code=$?
    verdict "import-fixture: $n: --import exits 0 with no display" "$code" "exit $code: $(first_line "$RUN/fx-$n/import.err")"
    same_file "import-fixture: $n: --import's values file is expected/import-$n.values.xml" "$H/.config/riley/settings.xml" \
        "$EXPECTED/import-$n.values.xml" "$H"
    want_out="imported ${KEYS[$n]} keys into $H/.config/riley/settings.xml"
    if [ "$(cat "$RUN/fx-$n/import.out")" = "$want_out" ]; then pass "import-fixture: $n: --import says \"imported ${KEYS[$n]} keys into ...\""
    else fail "import-fixture: $n: --import says \"imported ${KEYS[$n]} keys into ...\"" "got: $(first_line "$RUN/fx-$n/import.out")"; fi
    if [ ! -s "$RUN/fx-$n/import.err" ]; then pass "import-fixture: $n: --import prints nothing on stderr (no value skipped)"
    else fail "import-fixture: $n: --import prints nothing on stderr (no value skipped)" "$(head -n 3 "$RUN/fx-$n/import.err")"; fi
    after=$(listing "$H" | grep -v '^\.config/riley/settings\.xml ')
    if [ "$before" = "$after" ]; then pass "import-fixture: $n: --import wrote the values file and nothing else (no target)"
    else fail "import-fixture: $n: --import wrote the values file and nothing else (no target)" "$(diff <(echo "$before") <(echo "$after") | head -n 4)"; fi

    before=$(listing "$H")
    offline "$H" "check-$n" "$QT" --check >"$RUN/fx-$n/check.out" 2>"$RUN/fx-$n/check.err"
    code=$?
    if [ "$code" = 1 ]; then pass "import-fixture: $n: --check exits 1 (a target differs) with no display"
    else fail "import-fixture: $n: --check exits 1 (a target differs) with no display" "exit $code: $(first_line "$RUN/fx-$n/check.err")"; fi
    same_file "import-fixture: $n: --check prints expected/check-$n.txt" "$RUN/fx-$n/check.out" "$EXPECTED/check-$n.txt" "$H"
    if [ "$(listing "$H")" = "$before" ]; then pass "import-fixture: $n: --check wrote nothing in the home"
    else fail "import-fixture: $n: --check wrote nothing in the home" "$(diff <(echo "$before") <(listing "$H") | head -n 4)"; fi
done

# ---- refuse: --import never overwrites a values file ------------------------------------------------------------
H=$RUN/fx-m1/home
if [ "$built" = 1 ] && [ -e "$H/.config/riley/settings.xml" ]; then
    before=$(listing "$H"); sum=$(sha256sum <"$H/.config/riley/settings.xml")
    offline "$H" refuse "$QT" --import >"$RUN/refuse.out" 2>"$RUN/refuse.err"
    code=$?
    if [ "$code" = 1 ]; then pass "refuse: --import with a values file present exits 1"
    else fail "refuse: --import with a values file present exits 1" "exit $code"; fi
    if [ "$(grep -c . "$RUN/refuse.err")" = 1 ]; then pass "refuse: it says why in one line on stderr"
    else fail "refuse: it says why in one line on stderr" "$(head -n 3 "$RUN/refuse.err")"; fi
    if [ "$(listing "$H")" = "$before" ] && [ "$(sha256sum <"$H/.config/riley/settings.xml")" = "$sum" ]; then
        pass "refuse: the values file and the home are untouched (bytes, mtime)"
    else fail "refuse: the values file and the home are untouched (bytes, mtime)" "$(diff <(echo "$before") <(listing "$H") | head -n 4)"; fi
else
    fail "refuse: --import with a values file present exits 1" "no values file from the m1 import to refuse over"
fi

# ---- import-real: Marc's live GTK model and the files its pages read, copied (read only) into a throwaway home ------
H=$RUN/real/home
REAL_SOURCES="$REAL_HYPR/settings.json:.config/hypr/settings.json $REAL_HYPR/settings.lua:.config/hypr/settings.lua
    $REAL_HYPR/keys-user.lua:.config/hypr/keys-user.lua
    $REAL_HYPR/idle.conf:.config/hypr/idle.conf $REAL_HYPR/hypridle.conf:.config/hypr/hypridle.conf
    $REAL_HYPR/hyprpaper.conf:.config/hypr/hyprpaper.conf $REAL_HYPR/float-mode.json:.config/hypr/float-mode.json
    $REAL_HYPR/current-theme:.config/hypr/current-theme $REAL_HYPR/current-background:.config/hypr/current-background
    $REAL_HYPR/wallpapers.json:.config/hypr/wallpapers.json $HOME/.config/riley/desktop.json:.config/riley/desktop.json
    $REAL_CFG/riley/fonts.toml:.config/riley/fonts.toml $REAL_CFG/riley/weather.toml:.config/riley/weather.toml
    $REAL_CFG/nwg-dock-hyprland/riley.conf:.config/nwg-dock-hyprland/riley.conf $HOME/.cache/nwg-dock-pinned:.cache/nwg-dock-pinned"
declare -A LIVE=()    # path in the throwaway home (relative) -> Marc's live file
if [ "$built" != 1 ]; then
    fail "import-real: not built"
elif [ ! -f "$REAL_HYPR/settings.json" ]; then
    fail "import-real: Marc's GTK model $REAL_HYPR/settings.json exists" "nothing to import"
else
    mkdir -p "$H/.config" "$H/.cache"
    for s in $REAL_SOURCES; do
        src=${s%%:*} rel=${s#*:}
        LIVE[$rel]=$src
        if [ -e "$src" ]; then mkdir -p "$(dirname "$H/$rel")"; cp -L "$src" "$H/$rel"; fi
    done
    if [ -d "$HOME/Desktop" ]; then     # the folder names only: the desktop's boxes come and go with them
        mkdir -p "$H/Desktop"
        for d in "$HOME"/Desktop/*/; do [ -d "$d" ] && mkdir -p "$H/Desktop/$(basename "$d")"; done
    fi
    offline "$H" import-real "$QT" --import >"$RUN/real/import.out" 2>"$RUN/real/import.err"
    code=$?
    verdict "import-real: --import of Marc's live model exits 0" "$code" "exit $code: $(first_line "$RUN/real/import.err")"
    [ -s "$RUN/real/import.err" ] && sed 's/^/     note: /' "$RUN/real/import.err" | head -n 10
    if offline "$H" import-real "$CFGTOOL" check-values "$DESC" "$H/.config/riley/settings.xml" >"$RUN/real/cv.err" 2>&1 \
        && [ ! -s "$RUN/real/cv.err" ]; then pass "import-real: the values file is valid and has no unknown key"
    else fail "import-real: the values file is valid and has no unknown key" "$(head -n 2 "$RUN/real/cv.err")"; fi
    before=$(listing "$H")
    rm -rf -- "$RUN/real/out"
    offline "$H" check-real "$QT" --check --out "$RUN/real/out" >"$RUN/real/check.out" 2>"$RUN/real/check.err"
    code=$?
    verdict "import-real: --check --out exits 0 (every target the same)" "$code" "exit $code: $(first_line "$RUN/real/check.err")"
    same_file "import-real: --check prints expected/check-real.txt (every target \"same\")" "$RUN/real/check.out" \
        "$EXPECTED/check-real.txt" "$H"
    if [ "$(listing "$H")" = "$before" ]; then pass "import-real: --check wrote nothing in the home"
    else fail "import-real: --check wrote nothing in the home" "$(diff <(echo "$before") <(listing "$H") | head -n 4)"; fi
    # The independent source: what the writers made from the imported values, byte for byte against Marc's live files.
    while IFS=$'\t' read -r target word path; do
        rel=${path#@HOME@/}
        live=${LIVE[$rel]:-}
        [ -n "$live" ] || { fail "import-real: $target: $rel is one of the copied files" "the gate does not know $rel"; continue; }
        if [ -e "$live" ]; then
            if cmp -s "$RUN/real/out/$rel" "$live"; then pass "import-real: $target: the writers' $rel is Marc's live file, byte for byte"
            else fail "import-real: $target: the writers' $rel is Marc's live file, byte for byte" \
                "$( { [ -e "$RUN/real/out/$rel" ] || echo "no $RUN/real/out/$rel"; diff -u "$live" "$RUN/real/out/$rel"; } 2>&1 | head -n 12)"; fi
        elif [ ! -e "$RUN/real/out/$rel" ]; then pass "import-real: $target: no $rel, as in Marc's home"
        else fail "import-real: $target: no $rel, as in Marc's home" "the writers made one"; fi
    done <"$EXPECTED/check-real.txt"
    n_out=$(cd "$RUN/real/out" 2>/dev/null && find . -type f | wc -l)
    n_live=$(while IFS=$'\t' read -r _ _ path; do l=${LIVE[${path#@HOME@/}]:-}; [ -n "$l" ] && [ -e "$l" ] && echo; done <"$EXPECTED/check-real.txt" | wc -l)
    if [ "$n_out" = "$n_live" ]; then pass "import-real: --out holds one file per live target file ($n_live)"
    else fail "import-real: --out holds one file per live target file ($n_live)" "it holds $n_out"; fi
fi

# ---- start: the panel opens offscreen with no QML warning; the first start imports the old model -----------------
start() {   # start HOME TAG STYLE  (STYLE "" = the program's own, Fusion)
    local h=$1 tag=$2 style=$3
    # The pages query the screens and the queues when they open: the fakes answer, before the traps.
    offline "$h" "$tag" env PATH="$RUN/fakebin:$RUN/trap:/usr/bin:/bin" RILEY_FAKE_DIR="$RUN/fake" XDG_DATA_DIRS="$RUN/no-data-dirs" \
        QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 ${style:+QT_QUICK_CONTROLS_STYLE=$style} \
        timeout -k 2 4 "$QT" >"$RUN/$tag.out" 2>"$RUN/$tag.err"
}
if [ "$built" = 1 ]; then
    for style in Basic ""; do
        tag=start-${style:-Fusion}
        H=$RUN/$tag/home; rm -rf -- "$H"; mkdir -p "$H"
        start "$H" "$tag" "$style"; code=$?
        if [ "$code" = 124 ]; then pass "start: ${style:-Fusion}: the panel ran offscreen until stopped (empty home)"
        else fail "start: ${style:-Fusion}: the panel ran offscreen until stopped (empty home)" "exit $code: $(first_line "$RUN/$tag.err")"; fi
        w=$(warnings_in "$RUN/$tag.err")
        if [ -z "$w" ]; then pass "start: ${style:-Fusion}: no QML warning on stderr"
        else fail "start: ${style:-Fusion}: no QML warning on stderr" "$w"; fi
        if [ -z "$(listing "$H")" ]; then pass "start: ${style:-Fusion}: opening wrote nothing (no old model, no import)"
        else fail "start: ${style:-Fusion}: opening wrote nothing (no old model, no import)" "$(listing "$H" | head -n 3)"; fi
    done
    H=$RUN/start-first/home
    make_home "$H" m1
    before=$(listing "$H")
    start "$H" start-first Basic; code=$?
    if [ "$code" = 124 ]; then pass "start: first start with the old model and no values file: the panel ran until stopped"
    else fail "start: first start with the old model and no values file: the panel ran until stopped" "exit $code: $(first_line "$RUN/start-first.err")"; fi
    same_file "start: first start imported the old model: values file is expected/import-m1.values.xml" \
        "$H/.config/riley/settings.xml" "$EXPECTED/import-m1.values.xml" "$H"
    after=$(listing "$H" | grep -v '^\.config/riley/settings\.xml ')
    if [ "$before" = "$after" ]; then pass "start: first start wrote the values file and nothing else"
    else fail "start: first start wrote the values file and nothing else" "$(diff <(echo "$before") <(echo "$after") | head -n 4)"; fi
    w=$(warnings_in "$RUN/start-first.err")
    if [ -z "$w" ]; then pass "start: first start: no QML warning on stderr"; else fail "start: first start: no QML warning on stderr" "$w"; fi
else
    fail "start: not built"
fi

# ---- install: cmake --install into a throwaway prefix; the installed binary finds ../share by itself ---------------
P=$RUN/prefix
if [ "$built" = 1 ]; then
    cmake --install "$BUILD_DIR" --prefix "$P" >"$RUN/install.log" 2>&1
    verdict "install: cmake --install --prefix exits 0" $? "$(tail -n 2 "$RUN/install.log")"
    got=$(cd "$P" 2>/dev/null && find . \( -type f -o -type l \) -printf '%P\n' | sort | tr '\n' ' ')
    want="bin/riley-config bin/riley-settings-qt share/riley-settings/schema/settings.xsd share/riley-settings/schema/values.xsd share/riley-settings/settings.xml "
    if [ "$got" = "$want" ]; then pass "install: the prefix holds exactly the 5 listed files"
    else fail "install: the prefix holds exactly the 5 listed files" "found: $got"; fi
    if cmp -s "$P/share/riley-settings/settings.xml" "$DESC"; then pass "install: share/riley-settings/settings.xml is the repo's"
    else fail "install: share/riley-settings/settings.xml is the repo's"; fi
    H=$RUN/install/home; rm -rf -- "$H"; mkdir -p "$H/.config/riley"
    cp "$EXPECTED/import-m1.values.xml" "$H/.config/riley/settings.xml"
    inst() {   # the installed binary, RILEY_SETTINGS_SHARE unset
        env -i PATH="$RUN/trap:/usr/bin:/bin" HOME="$H" XDG_CONFIG_HOME="$H/.config" XDG_CACHE_HOME="$H/.cache" \
            XDG_RUNTIME_DIR="$RUN/runtime" TMPDIR="$RUN/tmp" LANG=C.UTF-8 LC_ALL=C.UTF-8 "$@"
    }
    before=$(listing "$H")
    inst "$P/bin/riley-settings-qt" --check >"$RUN/install/check.out" 2>"$RUN/install/check.err"
    code=$?
    if [ "$code" = 1 ]; then pass "install: the installed riley-settings-qt --check, RILEY_SETTINGS_SHARE unset, exits 1"
    else fail "install: the installed riley-settings-qt --check, RILEY_SETTINGS_SHARE unset, exits 1" "exit $code: $(first_line "$RUN/install/check.err")"; fi
    same_file "install: it prints expected/check-install.txt" "$RUN/install/check.out" "$EXPECTED/check-install.txt" "$H"
    if [ "$(listing "$H")" = "$before" ]; then pass "install: --check wrote nothing in the home"
    else fail "install: --check wrote nothing in the home"; fi
    rm -rf -- "$RUN/prefix-moved"; cp -a "$P" "$RUN/prefix-moved"; rm -rf -- "$RUN/prefix-moved/share"
    inst "$RUN/prefix-moved/bin/riley-settings-qt" --check >"$RUN/install/moved.out" 2>"$RUN/install/moved.err"
    code=$?
    if [ "$code" = 2 ] && [ ! -s "$RUN/install/moved.out" ]; then
        pass "install: a copy of the prefix without share/ fails (exit 2): the share dir comes from the binary's place"
    else fail "install: a copy of the prefix without share/ fails (exit 2): the share dir comes from the binary's place" \
        "exit $code, stdout: $(first_line "$RUN/install/moved.out")"; fi
else
    fail "install: not built"
fi

# ---- launcher: Riley's bin/riley-settings, from a copy, with a fake riley-settings-qt -----------------------------
# Riley's GTK panel is gone (riley 7f0c901): the launcher only starts the Qt panel.
L=$RUN/launcher
SRC=$RILEY_REPO/bin/riley-settings
QT_LINE='^exec riley-settings-qt --apply-actions "\$@"$'
ready=1
if [ -f "$SRC" ] && [ "$(head -n 1 "$SRC")" = '#!/bin/sh' ] && ! grep -qiE 'gtk|import' "$SRC"; then
    pass "launcher: Riley's bin/riley-settings is a POSIX sh launcher that names no GTK panel"
else fail "launcher: Riley's bin/riley-settings is a POSIX sh launcher that names no GTK panel" "$(first_line "$SRC")"; ready=0; fi
others=$(cd "$RILEY_REPO" && ls -d bin/riley-settings?* settings/*/app.py 2>/dev/null | tr '\n' ' ')
if [ -z "$others" ]; then pass "launcher: Riley's bin/ has no other riley-settings* program, and Riley has no GTK app entry"
else fail "launcher: Riley's bin/ has no other riley-settings* program, and Riley has no GTK app entry" "still there: $others"; fi
n_qt=$(grep -cE "$QT_LINE" "$SRC" 2>/dev/null)
if [ "${n_qt:-0}" = 1 ]; then pass "launcher: it has exactly one line \"exec riley-settings-qt --apply-actions \"\$@\"\" (where the planted copy cuts)"
else fail "launcher: it has exactly one line \"exec riley-settings-qt --apply-actions \"\$@\"\" (where the planted copy cuts)" "${n_qt:-0} lines"; ready=0; fi
if [ "$ready" = 1 ]; then
    rm -rf -- "$L"; mkdir -p "$L/riley/bin" "$L/localbin" "$L/qtbin" "$L/home"
    if [ "$FAULT" = ON ]; then   # the planted copy: the Qt panel is started without --apply-actions
        sed -E 's/^exec riley-settings-qt --apply-actions /exec riley-settings-qt /' "$SRC" >"$L/riley/bin/riley-settings"
        if cmp -s "$SRC" "$L/riley/bin/riley-settings"; then echo "     note: the planted sed line changed nothing"; fi
    else cp "$SRC" "$L/riley/bin/riley-settings"; fi
    printf '#!/bin/sh\n{ printf qt; for a in "$@"; do printf " %%s" "$a"; done; printf "\\n"; } >>"$LAUNCH_LOG"\nexit "${FAKE_QT_EXIT:-0}"\n' >"$L/qtbin/riley-settings-qt"
    chmod +x "$L"/riley/bin/* "$L/qtbin/riley-settings-qt"
    ln -s ../riley/bin/riley-settings "$L/localbin/riley-settings"         # as ~/.local/bin/riley-settings is
    WITHQT="$L/qtbin:$L/localbin:$RUN/trap:/usr/bin:/bin"
    NOQT="$L/localbin:$RUN/trap:/usr/bin:/bin"
    lrun() {   # lrun PATH QT_EXIT PROGRAM ARGS...: run, then $L/got holds what was started, $L/code the exit code
        local path=$1 qexit=$2 prog=$3; shift 3
        : >"$L/calls.log"
        env -i PATH="$path" HOME="$L/home" XDG_CONFIG_HOME="$L/home/.config" LAUNCH_LOG="$L/calls.log" FAKE_QT_EXIT="$qexit" \
            LANG=C.UTF-8 "$L/localbin/$prog" "$@" >"$L/out" 2>"$L/err"
        echo $? >"$L/code"
        cat "$L/calls.log" >"$L/got"
    }
    expect() {   # expect NAME WANT_STARTED WANT_CODE
        local got code; got=$(cat "$L/got"); code=$(cat "$L/code")
        if [ "$got" = "$2" ] && [ "$code" = "$3" ]; then pass "launcher: $1"
        else fail "launcher: $1" "started: [$(tr '\n' ';' <<<"$got")], exit $code; want [$(tr '\n' ';' <<<"$2")], exit $3"; fi
    }
    lrun "$WITHQT" 0 riley-settings;              expect "riley-settings-qt --apply-actions" "qt --apply-actions" 0
    lrun "$WITHQT" 0 riley-settings --check;      expect "arguments pass through: riley-settings-qt --apply-actions --check" "qt --apply-actions --check" 0
    lrun "$WITHQT" 3 riley-settings;              expect "riley-settings-qt exits 3: the launcher exits 3" "qt --apply-actions" 3
    lrun "$NOQT" 0 riley-settings;                expect "riley-settings-qt not installed: nothing started, exit 127" "" 127
    if [ "$(grep -c '' "$L/err")" = 1 ] && grep -q 'riley-settings-qt is not installed' "$L/err"; then
        pass "launcher: not installed: one line on stderr says so"
    else fail "launcher: not installed: one line on stderr says so" "$(head -n 2 "$L/err")"; fi
else
    fail "launcher: the launcher cases ran" "Riley's launcher is not the Qt-only launcher ($SRC), so it was not run"
fi

# ---- pages, layout, printers: the harness ---------------------------------------------------------------------------
RESULTS=$RUN/results
: >"$RESULTS"
if [ "$built" = 1 ]; then
    env -i PATH="$RUN/fakebin:$RUN/trap:/usr/bin:/bin" HOME="$RUN/home" XDG_CONFIG_HOME="$RUN/home/.config" \
        XDG_CACHE_HOME="$RUN/home/.cache" XDG_DATA_HOME="$RUN/home/.local/share" XDG_DATA_DIRS="$RUN/no-data-dirs" \
        XDG_RUNTIME_DIR="$RUN/runtime" LANG=C.UTF-8 LC_ALL=C.UTF-8 \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QUICK_CONTROLS_STYLE=Basic QML_DISABLE_DISK_CACHE=1 \
        RILEY_SETTINGS_SHARE="$RUN/share" RILEY_SETTINGS_ACTION_LOG="$RUN/actions.log" RILEY_FAKE_DIR="$RUN/fake" \
        RILEY_SWITCH_RUN="$RUN" RILEY_SWITCH_EXPECTED="$EXPECTED" RILEY_SWITCH_SEEDS="$SW/seeds" RILEY_SWITCH_RESULTS="$RESULTS" \
        timeout 300 "$TST" >"$RUN/harness.log" 2>&1
    code=$?
    while IFS= read -r line; do
        case $line in
            "PASS "*) pass "${line#PASS }" ;;
            "FAIL "*) fail "${line#FAIL }" ;;
            "     "*) printf '%s\n' "$line" ;;
        esac
    done < <(grep -v '^DONE ' "$RESULTS")
    done_line=$(grep '^DONE ' "$RESULTS" | tail -n 1)
    if [ -n "$done_line" ]; then pass "gate: harness ran to the end (${done_line#DONE } checks)"
    else fail "gate: harness ran to the end" "exit $code, no DONE line: $(grep -m1 -E 'FAIL!|QFATAL|Segmentation|error' "$RUN/harness.log" | cut -c1-200) (log: $RUN/harness.log)"; fi
    hf=$(grep -c '^FAIL ' "$RESULTS")
    if { [ "$code" = 0 ] && [ "$hf" = 0 ]; } || { [ "$code" != 0 ] && [ "$hf" != 0 ]; }; then
        pass "gate: harness exit code ($code) agrees with its checks ($hf failed)"
    else fail "gate: harness exit code ($code) agrees with its checks ($hf failed)" "log: $RUN/harness.log"; fi
else
    fail "pages: not built"; fail "layout: not built"; fail "printers: not built"
fi

# ---- untouched: nothing run, nothing of Marc's written -----------------------------------------------------------
if [ ! -e "$RUN/trap.log" ]; then pass "untouched: no desktop command was run (traps on PATH untouched)"
else fail "untouched: no desktop command was run (traps on PATH untouched)" "$(first_line "$RUN/trap.log")"; fi
if ! grep -q '^UNEXPECTED ' "$RUN/fake/calls.log" 2>/dev/null; then pass "untouched: the fakes saw no call outside the contract"
else fail "untouched: the fakes saw no call outside the contract" "$(grep -m1 '^UNEXPECTED ' "$RUN/fake/calls.log")"; fi
if [ "$(git -C "$REPO" status --porcelain 2>/dev/null)" = "$REPO_BEFORE" ]; then pass "untouched: the repo's working tree is as before"
else fail "untouched: the repo's working tree is as before" "$(diff <(echo "$REPO_BEFORE") <(git -C "$REPO" status --porcelain) | grep '^>' | head -n 1)"; fi
REAL_AFTER=$(state)
if [ "$REAL_BEFORE" = "$REAL_AFTER" ]; then pass "untouched: Marc's model, targets, values, dock pins, lpoptions and launchers unchanged (mtime, size)"
else fail "untouched: Marc's model, targets, values, dock pins, lpoptions and launchers unchanged (mtime, size)" \
    "$(diff <(echo "$REAL_BEFORE") <(echo "$REAL_AFTER") | grep '^>' | head -n 1)"; fi

echo
echo "Per group:"
for g in $GROUPS_ALL; do printf '  %-15s %3d passed, %3d failed\n' "$g" "${GP[$g]:-0}" "${GF[$g]:-0}"; done
if [ "$FAULT" = ON ]; then
    missing=""
    for g in $FAULT_GROUPS; do [ "${GF[$g]:-0}" -gt 0 ] || missing="$missing $g"; done
    if [ -z "$missing" ]; then echo "PASS planted faults: every group with a fault has a failing check (the gate is proven)"
    else echo "FAIL planted faults: no failing check in:$missing (the gate is not proven)"; fi
fi
echo "$passed passed, $failed failed"
[ "$FAULT" = ON ] && exit $([ -z "$missing" ] && echo 0 || echo 1)
[ "$failed" = 0 ]
