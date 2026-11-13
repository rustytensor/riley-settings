#!/usr/bin/env bash
# Gate for M3, the custom pages. One command, one PASS/FAIL line per check, exit 0 only if all pass. The contract it
# checks: docs/pages-contract.md.
#
#   tests/run-pages.sh                 build tests/pages/ (the harness plus the repo) into
#                                      /srv/devops/forge/build/riley-settings/pages and check
#   tests/run-pages.sh --plant-fault   build with -DRILEY_PLANT_FAULT_PAGES=ON into .../pages-fault: one fault per page
#                                      (docs/pages-contract.md, "The planted faults"); every page group must then have
#                                      at least one failing check; it exits 0 only when every group does
#   BUILD_DIR=/some/dir tests/run-pages.sh   another build directory
#
# The harness runs offscreen with a throwaway home under $BUILD_DIR/run (wiped and seeded for each page), the
# repo's share/riley-settings/settings.xml, and fakes first on PATH for hyprctl, Hyprland and the CUPS tools: they
# record their argv and answer from tests/pages/fixtures. Every other desktop command is a trap. Actions and launches
# are recorded in $BUILD_DIR/run/actions.log, never run. Expected results: tests/pages/expected/*, worked out by hand
# from Riley's gates before the pages existed; their sha256 sums, and those of the seeds, fixtures and fakes, are
# pinned below.
set -u

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BASE=/srv/devops/forge/build/riley-settings
FAULT=OFF
for a in "$@"; do
    case $a in
        --plant-fault) FAULT=ON ;;
        -h|--help) sed -n '2,18p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown argument: $a" >&2; exit 2 ;;
    esac
done
if [ "$FAULT" = ON ]; then BUILD_DIR=${BUILD_DIR:-$BASE/pages-fault}; else BUILD_DIR=${BUILD_DIR:-$BASE/pages}; fi
RUN=$BUILD_DIR/run
PAGES=$REPO/tests/pages
EXPECTED=$PAGES/expected
GROUPS_ALL="displays printers keyboard-mouse wallpapers dock desktop system-launcher"

passed=0
failed=0
declare -A GP=() GF=()
pass() { echo "PASS $1"; passed=$((passed + 1)); }
fail() { echo "FAIL $1"; [ -n "${2:-}" ] && printf '     %s\n' "$2"; failed=$((failed + 1)); }
first_line() { head -n 1 "$1" 2>/dev/null | cut -c1-200; }

# Marc's real files: their state now, compared at the end. Read only, never written.
REAL_CFG=${XDG_CONFIG_HOME:-$HOME/.config}
REAL_CACHE=${XDG_CACHE_HOME:-$HOME/.cache}
REAL_FILES="$REAL_CFG/riley/settings.xml $REAL_CFG/riley/desktop.json $REAL_CFG/riley/greeter-sway.conf
    $REAL_CFG/hypr/settings.json $REAL_CFG/hypr/idle.conf $REAL_CFG/hypr/hyprpaper.conf $REAL_CFG/hypr/settings.lua
    $REAL_CFG/nwg-dock-hyprland/riley.conf $REAL_CFG/riley-dock/config.xml $REAL_CACHE/nwg-dock-pinned
    $HOME/.cups/lpoptions"
state() { for f in $REAL_FILES; do if [ -e "$f" ]; then stat -c '%n %y %s' "$f"; else echo "$f absent"; fi; done; }
REAL_BEFORE=$(state)
REPO_BEFORE=$(git -C "$REPO" status --porcelain 2>/dev/null)

case $RUN in /srv/devops/forge/build/*|/srv/devops/*) ;; *) echo "BUILD_DIR must be under /srv/devops" >&2; exit 2 ;; esac
mkdir -p "$BUILD_DIR"
rm -rf -- "$RUN"
mkdir -p "$RUN/home" "$RUN/share" "$RUN/trap" "$RUN/fakebin" "$RUN/fake/canned" "$RUN/fake/state" \
    "$RUN/xdg/cache" "$RUN/xdg/state" "$RUN/data-dirs/applications" "$RUN/runtime"
chmod 700 "$RUN/runtime"
touch "$RUN/.riley-pages-run"   # the harness wipes $RUN/home only when this marker is next to it

# ---- build ----------------------------------------------------------------------------------------------------
LOG=$RUN/build.log
TST=$BUILD_DIR/tst_pages
if cmake -S "$PAGES" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRILEY_PLANT_FAULT=OFF \
        -DRILEY_PLANT_FAULT_PANEL=OFF -DRILEY_PLANT_FAULT_PAGES="$FAULT" >"$LOG" 2>&1 \
    && ninja -C "$BUILD_DIR" -j8 >>"$LOG" 2>&1 && [ -x "$TST" ]; then
    pass "gate: build (RILEY_PLANT_FAULT_PAGES=$FAULT) gives $TST"
    built=1
else
    fail "gate: build (RILEY_PLANT_FAULT_PAGES=$FAULT) gives $TST" "$(grep -m1 -iE 'error' "$LOG" | cut -c1-200) (log: $LOG)"
    built=0
fi

# ---- the expected files, seeds, fixtures and fakes have not drifted -------------------------------------------
declare -A SUM=(
    [expected/desktop-actions.log]=652c6907fd9ce0ccae2921648b4e84d11c1ef3c66456880efdc69dc319afe394
    [expected/desktop-json-1.json]=2adb01d3f75d4a97142c95e1cdd96aae66db692158316949e7569234083d9df4
    [expected/desktop-json-2.json]=fd157b3769e3f71d2f09aad7be7bd8b445af2549b9db096d0e26418ffd13b30a
    [expected/desktop-json-3.json]=00623ae3ce9ac0c4e7664e02b890af387049cef95d62557169c4832585df377f
    [expected/desktop-json-4.json]=4e050fc9bb06ce9c7d63126350cdd41e60063b3f4c8f0244ad9a95c094ad190e
    [expected/desktop-json-5.json]=3a1cf41620623ef2b356b3475624ee49abbc41761df6c35f9df171fd4157758e
    [expected/desktop-json-6.json]=1b64902f5ce4027df8248a63d8637dfef955719905edcf4a2f5a1871217c91e1
    [expected/desktop-values-1.xml]=33dbee4d4ac2d3c194a0d95d1a784b7eb0cfc8d06909e7e73aa5bf321d7dd5e1
    [expected/desktop-values-2.xml]=b31b82602b7422351b75d7856d91903921f2077886931770f3e9b857039b12ba
    [expected/desktop-values-4.xml]=f557f573f2a4f6bfe6209a956842f464e8c8936d11965cbf5b829b68f0f210cc
    [expected/desktop-values-5.xml]=47f4a93b203b35cde9dae8db060b92a7a2f46753e44ca952e4308fb718369ea5
    [expected/desktop-values-6.xml]=4c19fce8a1d73f8c3b6fb49fc1cb94bc565bc7a477a3734a48b672ccbdf085c0
    [expected/displays-model.json]=b027b5f4d700544462a3a600c51f03b255d78734c4e7f23e98ec577735ee696f
    [expected/displays-actions.log]=676765fbd892919e03588e26eaa8c81fd12d575732f75586aec96b1d88f7f93e
    [expected/displays-hyprpaper.conf]=5a57401247ed80ceae36483f53392e4b6b141123b78b0b191ed0cc7a5fa10eb5
    [expected/displays-values-applied.xml]=828570d2f1070443c438e3ed2950646a22c4ae5609af81ef160207fdadacc905
    [expected/dock-actions.log]=c9c4eb4cba0075b1527c077d1541acf21f73ecef99d22e23092fbda571f2ada7
    [expected/dock-conf-autohide.conf]=2c00149cd8e77990783ff9121e25113c92f13bf2162db6909485cc431a421d29
    [expected/dock-conf-empty.conf]=69808e01f194000af591d3bd14ef97d3ab6d6790db77ad9087c25800ea29c0e3
    [expected/dock-conf-launcher.conf]=76270892a6c3fb84566cd6f2173ac1335cebca795b6e428312b740a6774211f2
    [expected/dock-conf-one.conf]=fa740dd638b14c40b97c5cc6e5ac44a8bcc8b4c6e2e9392685fe62d38afe92d5
    [expected/dock-conf-reflections.conf]=c78ce270bd7e8354ae469647bf5009c0cedb54daa0ff09ce6b80135afe37c694
    [expected/dock-conf-size46.conf]=766858d15c6baaf14817719656501a70cbfa2afe600f0341924aa0fdc50e21af
    [expected/dock-conf-sized.conf]=2819d6a5e3bc68487d47cf292131f50b5fc02b5f89ebf176f2040b2f5850f31c
    [expected/dock-pinned-1]=da7bd2207e85e95b8cd9a9c14d2da2a4a414263bd1352778f8feb0f223a8ca04
    [expected/dock-pinned-2]=ea34308a7d5458da7dd86b602648d9e5f3fccda8c2425112518d399b5f75a0d4
    [expected/dock-pinned-4]=c3bbb9e3b6ad2452bbd461f2446bc74fd3a1a10618e8852cd91ca4ea77a4e5ba
    [expected/dock-pinned-5]=5caf0387b47744a4accc54c1bb6e812db38701fa6e5d4583c06ac2f11268dbbc
    [expected/dock-pinned-6]=c8fdb11dcf4392596ca18757e3943bc24d9a91edd8b201b5674b32cb171f8618
    [expected/dock-values-1.xml]=546293df19c78756643e180135eabd2280af3b175c6316b72adf236a437eb542
    [expected/dock-values-2.xml]=e43abd534b6e76e990fe73e471a822df6359d7d6bd22ef1e4d1d168f9d1c81d7
    [expected/dock-values-4.xml]=cfe7b51589749700d2e595085f673d34091d74b940bdeee65e21d6e4eba7a089
    [expected/dock-values-5.xml]=e55156018bd46ada1e20c747ade9a98faba2e249c6891577271a8bdeaae8e6ba
    [expected/dock-values-6.xml]=1c341a97f6567e3d212d86938401558bf03fb1e95da9140f00014f287616b31e
    [expected/expected-m1.lua]=65cfdae2f2e597af30299fca905b66a86dbad0cbb6ee0fdbf5057723cc3906da
    [expected/expected-m3.lua]=809f90340d13a2f6c6e548412ce2e2cc38812421f16cb3efa1b488802e61520c
    [expected/keyboard-actions.log]=0791dcd4f3a596d852e3f21f564219b971d11c7ccce2a2acd7b4f14cf2cf2145
    [expected/keyboard-model.json]=25799e3dd92a4c8b1d862380bde844c8ff653c811415e1145dd5ef09ea4c3051
    [expected/keyboard-values.xml]=568fab2899bf7683342c37d0960711578391080f7ed9c5127e0034950a849954
    [expected/printers-actions.log]=c034dd1b7a8f991e22cc9a4d09cb7d5e4f05ef84c3e780c73f198e1fab170eee
    [expected/printers-calls.log]=2c8aedd971315f09f6d741ac6e891b3dc52459f8ad683122aeb57587ff335a9a
    [expected/system-actions.log]=f54d1271bc602e8a94fc27d8a2a02fb90c90a250663c10f4f69da128ef5258ed
    [expected/wallpapers-actions.log]=6dfdf800c756815aac3b942f1a9d6fe2ff677773ca8148d37fa1c19472110419
    [expected/wallpapers-hyprpaper-1.conf]=9c3dc582028836a9d9dd0ec9c94ab7fed5bc12f037837157b893d41e481f5cdf
    [expected/wallpapers-hyprpaper-2.conf]=c1bfaa2277e57d1e17bc08cdf1a175324e118f824c0cc47c1778e55253bcb4d2
    [expected/wallpapers-hyprpaper-3.conf]=68d9d247584cedd08b1007729aad0ce0bcc61fe2b3763fac7fb5b95d45be0256
    [expected/wallpapers-values-1.xml]=1a236df37e78ef7621df886ea5c63899496f1c69d5a815c24ee54a0a2737770a
    [expected/wallpapers-values-2.xml]=eb0145224a54f28f1522f93ebc91a213e445058d73d061005f4010d1aeda5933
    [expected/wallpapers-values-3.xml]=2446ce937d985019aa3dfbc411bfef7aa9e2df7bc8f5f68599ca963aef92bc30
    [seeds/displays-settings.lua]=c5ba2fd7b76ab42212b98ba63e11c69a77ad5a076ae22b4f97abfd3aa0574315
    [seeds/displays-values.xml]=c8e9f7bb855c9fe65b1ce239079cd40b03d4b9ddd39178408bf7976307637421
    [seeds/dock-conf-autohide.conf]=92b7140e0e1de2fccaf7dac531b4e77b9f1e5836f23050c98d494dcb0be9fa49
    [seeds/dock-conf-launcher.conf]=2df30a9e69a8f75bcfb854aaf179daafb1209d6a912199af8245cb7d3f7930ce
    [seeds/dock-conf-reflections.conf]=b7785a45e56249ece2e3b6e1e7316863f7c9cf42174e95542d1a08ff64252159
    [seeds/dock-conf-screens.conf]=22bcfd6b221c86c9b5c914125eeb947c8440d546a9ace4c1dec5b7b16b42dafb
    [seeds/dock-values.xml]=6aca9564dcc39b9bf6b12416c0bcf6865adfa0353a9b395d8c2925c1ac3ad387
    [seeds/keyboard-values.xml]=d577c582ca3be40cfd17a20c118a4822dcec86208b488c76488aa9767e95e1ee
    [seeds/wallpapers-values.xml]=cde0ead9571aa5839730a08d363c6b302f495c56fc72a7da82a13dc778e86f88
    [fixtures/applications/dev.zed.Zed.desktop]=1c855a3e499aa9626eb71615025c547bb1838906bae5f6c8e0fa978301f90295
    [fixtures/applications/firefox.desktop]=4c26773be9f4778f82611eb2343459727b95e4e8cb149a4d8c6ffb9f8bafedeb
    [fixtures/applications/org.gnome.Ptyxis.desktop]=930294a58b3f88a900f76fc8cc1eddc40dbcd798f97b252f23ed95e5ad09afc2
    [fixtures/applications/org.kde.dolphin.desktop]=d92fbad893b72dbab6b33e81e1650b02c965af79c1be53d9891d0bebfeace5d3
    [fixtures/applications/riley.gate-hidden.desktop]=a7685602290d59590e928d1b47127ff6f6d51f751b2876e0d00145163876c9ed
    [fixtures/applications/riley.gate-removed.desktop]=575e35375f0e25fc6ede5fb5297d724e6d04b20d382ee731e6cfbe379d04bdff
    [fixtures/applications/riley.settings.desktop]=802be615d04f884d75e462cceb55e6b95455c3fbe201e66f905ca50d57b143c6
    [fixtures/ipptool-Office.txt]=6e4ee512a2d35643abbe81f43d91174ba1976696c2dff72ecb485a19507fa5f6
    [fixtures/ipptool-RileyGate.txt]=fdec04a2c6dd1b9b1565fd8ddeb05ec3f725edb49d65620663d22ccbcbc638aa
    [fixtures/lpoptions-p-Office.txt]=d342f86da4a8aa4fdea99aacfad42d0d9a7754902549617e2d68d0c4f7fc7b34
    [fixtures/lpoptions-p-RileyGate.txt]=bb3aa1b689eb7618ae7aa943713bc1acddeb7025e3585683dcc10c386660ad62
    [fixtures/lpstat-a.txt]=83340e3662ed4e0b554eaf71c0c59ed00523a3cef2d3e2151342999e3f77f5c0
    [fixtures/lpstat-l-o-RileyGate.txt]=43393f920741a850852b4e3db2356ce6f7f8bda5d1132e4251a038d2a8dd8446
    [fixtures/lpstat-l-p.txt]=09a15b46d255a38ffa845d0ee44a4e7c9f1e74bf8507ba9768a64df5101719f5
    [fixtures/monitors-all.json]=e51b0b22ce6b5ed5f055168dacc0164e4714b7452a71bdd3438f58cca937eb7d
    [fixtures/monitors.json]=55c8c556f0009fc153af8612feb1c00c00580b679856e6dd2dc5aa6624663487
    [fakes/cancel]=4467b6143eae76a2fe0c2fe60be076d430f9fac29e6778d16ec488ffa91d5202
    [fakes/_common.sh]=beaaf33b878aa466cfe4786f9feae87e44b18f94e1201479d9beb68997970997
    [fakes/hyprctl]=89232339b844f92c3b52bc95c20489194ca2f33f4a486669999062447af75130
    [fakes/Hyprland]=cc73bb3a55a23d4112ff070241c713c62be2e05f99238f36434e8d56e6234f13
    [fakes/ipptool]=3f9868f0704403fed153dbf3cfa56ebd2d4823105729307d2cbea633cb2bf1b2
    [fakes/lp]=2e6e9b1c9cd38f5aeeb1722f4bbf6670bdb28020b3044a073cad8c1f40f3a1c7
    [fakes/lpoptions]=fa5769c76ef4be7d30a23ec5d1b97ca9d0b6aaf3d9c6cbdaacf294be92c8290c
    [fakes/lpstat]=18ab17638735f8091667555e7f24ed426d0571f47a489fdb148dd6727968f90b
)
for f in $(printf '%s\n' "${!SUM[@]}" | sort); do
    got=$(sha256sum "$PAGES/$f" 2>/dev/null | cut -d' ' -f1)
    if [ "$got" = "${SUM[$f]}" ]; then pass "gate: pages/$f unchanged (sha256)"
    else fail "gate: pages/$f unchanged (sha256)" "got ${got:-no file}, want ${SUM[$f]}"; fi
done
extra=$(cd "$PAGES" && find expected seeds fixtures fakes -type f | sort | while read -r f; do [ -n "${SUM[$f]:-}" ] || printf '%s ' "$f"; done)
if [ -z "$extra" ]; then pass "gate: tests/pages/{expected,seeds,fixtures,fakes} hold only the pinned files"
else fail "gate: tests/pages/{expected,seeds,fixtures,fakes} hold only the pinned files" "not pinned: $extra"; fi

# ---- the description is valid; the panel's code names no option -------------------------------------------------
DESC=$REPO/share/riley-settings/settings.xml
if xmllint --noout --schema "$REPO/share/riley-settings/schema/settings.xsd" "$DESC" >"$RUN/xsd.err" 2>&1
then pass "gate: share/riley-settings/settings.xml validates against its schema"
else fail "gate: share/riley-settings/settings.xml validates against its schema" "$(grep -m1 -v 'fails to validate' "$RUN/xsd.err" | cut -c1-200)"; fi
for w in displays printers keyboard-mouse theme-wallpaper dock desktop system-launcher; do
    if grep -q "widget=\"$w\"" "$DESC"; then pass "gate: settings.xml has a page with widget=\"$w\""
    else fail "gate: settings.xml has a page with widget=\"$w\""; fi
done
label="gate: the panel's code (src/panel, src/app, qml) names no option key"
dirs=()
for d in src/panel src/app qml; do [ -d "$REPO/$d" ] && dirs+=("$REPO/$d"); done
keys=$(sed -n 's/.*<option[^>]*[[:space:]]key="\([^"]*\)".*/\1/p' "$DESC" "$REPO/tests/panel/settings.xml" | sort -u)
nkeys=$(grep -c . <<<"$keys")
hits=$(grep -rnwF -f <(printf '%s\n' "$keys") "${dirs[@]}" 2>/dev/null | head -n 3 | cut -c1-200)
if [ "$nkeys" -lt 40 ]; then fail "$label" "found only $nkeys keys in the two descriptions"
elif [ -z "$hits" ]; then pass "$label ($nkeys keys)"
else fail "$label" "$(head -n 1 <<<"$hits")"; fi

# ---- run the harness ---------------------------------------------------------------------------------------------
cp "$DESC" "$RUN/share/settings.xml"
cp -r "$REPO/share/riley-settings/schema" "$RUN/share/schema"
cp "$PAGES"/fakes/* "$RUN/fakebin/"
chmod +x "$RUN"/fakebin/*
cp "$PAGES"/fixtures/*.json "$PAGES"/fixtures/*.txt "$RUN/fake/canned/"
cp "$PAGES"/fixtures/applications/*.desktop "$RUN/data-dirs/applications/"
# Traps named like the desktop's other commands: any call is recorded and fails the gate.
for c in hypr-settings system-config-printer gnome-control-center systemctl loginctl hypr-dock hypr-theme hypr-theme.py \
         hyprpaper hypridle hyprlock hypr-idle hypr-desktop-menu hypr-reload hypr-float-mode riley-dock riley-dock-qt \
         nwg-dock-hyprland riley-greeter-sync riley-desktop pkill killall sudo pkexec lpadmin cupsenable cupsaccept \
         ippfind ipptool notify-send quickshell qs waybar kitty google-chrome xdg-open gio swaymsg sway; do
    printf '#!/bin/sh\necho "%s $*" >>"%s"\nexit 1\n' "$c" "$RUN/trap.log" >"$RUN/trap/$c"
    chmod +x "$RUN/trap/$c"
done

RESULTS=$RUN/results
: >"$RESULTS"
if [ "$built" = 1 ]; then
    env -i PATH="$RUN/fakebin:$RUN/trap:/usr/bin:/bin" HOME="$RUN/home" XDG_CONFIG_HOME="$RUN/home/.config" \
        XDG_CACHE_HOME="$RUN/xdg/cache" XDG_DATA_HOME="$RUN/home/.local/share" XDG_DATA_DIRS="$RUN/data-dirs" \
        XDG_STATE_HOME="$RUN/xdg/state" XDG_RUNTIME_DIR="$RUN/runtime" LANG=C.UTF-8 LC_ALL=C.UTF-8 \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QUICK_CONTROLS_STYLE=Basic QML_DISABLE_DISK_CACHE=1 \
        RILEY_SETTINGS_SHARE="$RUN/share" RILEY_SETTINGS_ACTION_LOG="$RUN/actions.log" RILEY_FAKE_DIR="$RUN/fake" \
        RILEY_PAGES_RUN="$RUN" RILEY_PAGES_EXPECTED="$EXPECTED" RILEY_PAGES_SEEDS="$PAGES/seeds" \
        RILEY_PAGES_RESULTS="$RESULTS" \
        timeout 600 "$TST" -maxwarnings 0 >"$RUN/harness.log" 2>&1
    code=$?
    while IFS= read -r line; do
        case $line in
            "PASS "*|"FAIL "*)
                rest=${line#* }; g=${rest%%:*}
                if [ "${line%% *}" = PASS ]; then pass "$rest"; GP[$g]=$((${GP[$g]:-0} + 1))
                else fail "$rest"; GF[$g]=$((${GF[$g]:-0} + 1)); fi ;;
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
    for g in $GROUPS_ALL; do
        n=$(( ${GP[$g]:-0} + ${GF[$g]:-0} ))
        if [ "$n" -gt 0 ]; then pass "gate: the $g group ran ($n checks)"
        else fail "gate: the $g group ran" "no check of the group in $RESULTS"; fi
    done
else
    fail "gate: harness ran to the end" "not built"
fi

# ---- nothing run, nothing written outside the throwaway home ------------------------------------------------------
if [ ! -e "$RUN/trap.log" ]; then pass "gate: no desktop command was run (traps on PATH untouched)"
else fail "gate: no desktop command was run (traps on PATH untouched)" "$(first_line "$RUN/trap.log")"; fi
if ! grep -q '^UNEXPECTED ' "$RUN/fake/calls.log" 2>/dev/null; then pass "gate: the fakes saw no call outside the contract"
else fail "gate: the fakes saw no call outside the contract" "$(grep -m1 '^UNEXPECTED ' "$RUN/fake/calls.log")"; fi
top=$(cd "$RUN" && ls -A | sort | tr '\n' ' ')
want="actions.log build.log data-dirs fake fakebin harness.log home results runtime share trap xdg xsd.err .riley-pages-run "
want=$(tr ' ' '\n' <<<"$want" | grep . | sort | tr '\n' ' ')
if [ "$built" != 1 ] || [ "$top" = "$want" ] || [ "$top" = "$(tr ' ' '\n' <<<"$want trap.log" | grep . | sort | tr '\n' ' ')" ]
then pass "gate: the run directory holds only what the gate made"
else fail "gate: the run directory holds only what the gate made" "found: $top"; fi
if [ "$(git -C "$REPO" status --porcelain 2>/dev/null)" = "$REPO_BEFORE" ]; then pass "gate: the repo's working tree is as before"
else fail "gate: the repo's working tree is as before" "$(diff <(echo "$REPO_BEFORE") <(git -C "$REPO" status --porcelain) | grep '^>' | head -n 1)"; fi
REAL_AFTER=$(state)
if [ "$REAL_BEFORE" = "$REAL_AFTER" ]; then pass "gate: Marc's real settings, targets, dock pins and lpoptions unchanged (mtime, size)"
else fail "gate: Marc's real settings, targets, dock pins and lpoptions unchanged (mtime, size)" \
    "$(diff <(echo "$REAL_BEFORE") <(echo "$REAL_AFTER") | grep '^>' | head -n 1)"; fi

echo
echo "Per page:"
for g in $GROUPS_ALL; do printf '  %-16s %3d passed, %3d failed\n' "$g" "${GP[$g]:-0}" "${GF[$g]:-0}"; done
if [ "$FAULT" = ON ]; then
    missing=""
    for g in $GROUPS_ALL; do [ "${GF[$g]:-0}" -gt 0 ] || missing="$missing $g"; done
    if [ -z "$missing" ]; then echo "PASS planted faults: every page group has a failing check (the gate is proven)"
    else echo "FAIL planted faults: no failing check in:$missing (the gate is not proven)"; fi
fi
echo "$passed passed, $failed failed"
[ "$FAULT" = ON ] && exit $([ -z "$missing" ] && echo 0 || echo 1)
[ "$failed" = 0 ]
