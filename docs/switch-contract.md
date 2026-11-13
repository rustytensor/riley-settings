# Switch contract (M4)

How the M4 gate (`tests/run-switch.sh`, harness `tests/switch/tst_switch.cpp`) checks the switch from Riley's GTK
panel to this one: the import of what the GTK panel saved, the launcher behind Super+Ctrl+S, the install, `--check`,
and the two features the switch must not lose (the keyboard layout picker and the printer options). Everything in
`docs/panel-contract.md` (M2) and `docs/pages-contract.md` (M3) still holds, except where this file says otherwise.
The gate checks these points against results worked out by hand from Riley's code before the code existed, in
`tests/switch/expected/`.

## Where Riley's GTK panel keeps things (found in Riley, commit ea5f243)

| What | Path | Riley's code |
|---|---|---|
| **The GTK panel's own model** | `~/.config/hypr/settings.json` (`Path.home()`, not `$XDG_CONFIG_HOME`) | `settings/riley_settings/core.py`: `load_model`, `model_for_save`, `_write_atomic(json.dumps(model, indent=2) + "\n")` |
| **The file the greeter follows** | the same `~/.config/hypr/settings.json` | `sddm/install-sddm.sh`: `riley-greeter-sync.path` has `PathChanged=$HOME/.config/hypr/settings.json`; the root service runs `riley-greeter-sync --settings $HOME/.config/hypr/settings.json --out /etc/sway/sddm-greeter-riley.config`, which renders it with a root-owned copy of `core.render_greeter` |
| Power times when the model has none | `~/.config/hypr/idle.conf`, else `~/.config/hypr/hypridle.conf` | `power.py` `current_times`, `core.parse_idle` |
| Tiled or floating workspaces | `~/.config/hypr/float-mode.json`, field `default` | `appearance.py` `float_default` |
| Fonts | `$XDG_CONFIG_HOME/riley/fonts.toml` (`sans`, `mono`, `terminal_size`) | `fonts_core.py` `load_user` |
| Weather | `$XDG_CONFIG_HOME/riley/weather.toml` (`location`, `units`) | `weather_core.py` `load_config` |
| Theme | `~/.config/hypr/current-theme` | `theme_wallpaper.py` `current_theme` |
| Wallpaper | `~/.config/hypr/hyprpaper.conf` (its first line says who wrote it), `~/.config/hypr/wallpapers.json` | `bin/hypr-theme.py` `set_wallpapers` (the theme's), `set_bg` (picked in the panel) |
| Dock pins | `~/.cache/nwg-dock-pinned` | `dock_core.py` `load_pins` |
| Dock settings | `$XDG_CONFIG_HOME/nwg-dock-hyprland/riley.conf` | `dock_core.py` `icon_size`, `screens`, `reflections`, `launcher`, `autohide` |
| Desktop boxes | `~/.config/riley/desktop.json` | `desktop_page.py`, `desktop/riley_desktop/layout.py` |

So the greeter writer of this panel writes `~/.config/hypr/settings.json` in Riley's model format (the target
`riley-model`, below). No system unit changes; the GTK panel, kept as the way back, keeps reading a current model.

## What the build provides

- Everything M3 provides.
- **`riley-settings-qt`**, besides M2's `--apply-actions`, `--description FILE` and `--values FILE`:
  - **`--import`**: see "Import". No window, no display needed.
  - **`--check [--out DIR]`**: see "--check". No window, no display needed.
  - Neither may construct a `QGuiApplication` (the gate runs them with no `QT_QPA_PLATFORM`, `WAYLAND_DISPLAY` or
    `DISPLAY`: a GUI application would abort).
  - When `RILEY_SETTINGS_SHARE` is unset, the share directory is **`<the binary's directory>/../share/riley-settings`**
    (`QCoreApplication::applicationDirPath()`), and nothing else: no compiled-in fallback.
- A writer for the new target **`riley-model`** (below). The writer `greeter-sway` stays (riley-config and the M1 gate
  use it), but no option of `settings.xml` names it any more.
- The CMake option **`RILEY_PLANT_FAULT_SWITCH`** (default OFF), see "The planted faults".
- **`cmake --install <build> --prefix P`** installs exactly:

  ```
  P/bin/riley-config
  P/bin/riley-settings-qt
  P/share/riley-settings/settings.xml
  P/share/riley-settings/schema/settings.xsd
  P/share/riley-settings/schema/values.xsd
  ```

The gate builds `tests/switch/CMakeLists.txt` (like `tests/pages/`), which adds the repo as a subdirectory, links
`tst_switch` with `riley-settings-panel`, `Qt6::Test` and `Qt6::Quick`, and gives `riley-settings-qt` and
`riley-config` in the build directory's top.

## Changes to the description

- On the 26 options whose keys are in Riley's model, `target` names **`riley-model`**: `display.monitors`, the 11
  `appearance.*` sliders and switches of Riley's `core.APPEARANCE` (not `appearance.workspaces`), the 9 `input.*`,
  `startup.apps`, the 4 `power.*`. Where `greeter-sway` stood (`display.monitors`, `input.kb_layout`), `riley-model`
  takes its place; elsewhere it is added at the end. No other option names it; no option names `greeter-sway`.
- Actions are unchanged (`greeter-sync` stays recorded; `riley-greeter-sync.path` does the work).
- **This changes M3's gate**: in its displays and keyboard-mouse groups the greeter file
  `riley/greeter-sway.conf` is replaced by `hypr/settings.json`. That update goes in its own commit, with Riley's
  `sddm/install-sddm.sh` lines above as the evidence, and its new expected files are made by the rules of
  `riley-model` below (with `tests/switch/make-expected.py`'s method: Riley's own `json.dumps`).

## Writers: what M4 adds or changes

### `riley-model`: `$XDG_CONFIG_HOME/hypr/settings.json`, whole file

Riley's model, as `core.py` writes it: `json.dumps(model, indent=2) + "\n"` (Python's defaults: `ensure_ascii`, so
every character above U+007F is written `\uXXXX`, surrogate pairs above U+FFFF; `"` and `\` escaped; `", "` never
appears, items are one per line). The keys, in this order:

1. `"version": 1`
2. `"monitors"`: always, a list (maybe `[]`); one object per item of `display.monitors`, keys `desc`, `enabled`
   (bool), `mode`, `x`, `y`, `scale`, `transform` (numbers).
3. `"appearance"`: only when the values file sets at least one of its 11 keys; then all 11 in Riley's order
   (`gaps_in`, `gaps_out`, `border_size`, `rounding`, `active_opacity`, `inactive_opacity`, `blur`, `blur_size`,
   `blur_passes`, `shadow`, `animations`), each the value or else the description's default.
4. `"input"`: likewise, all 9 in Riley's order (`kb_layout`, `kb_variant`, `kb_options`, `repeat_rate`,
   `repeat_delay`, `sensitivity`, `accel_profile`, `natural_scroll`, `follow_mouse`). `accel_profile` `adaptive` is
   written `""` (Riley's "on"); `follow_mouse` a number.
5. `"startup"`: only when `startup.apps` has items: objects `name`, `exec`, `enabled` (bool).
6. `"power"`: only when the values file sets at least one `power.*` key; then `screensaver`, `lock`, `screens_off`,
   `sleep`, numbers.

Numbers are written as C's `%g`, like every writer (`1.0` becomes `1`, `0.94` and `-0.2` stay). Strings and booleans
as JSON.

### `hyprland-lua`

As in M1, plus: `input.accel_profile` `adaptive` is written `""`, Riley's value for it (Hyprland's own default).

### `hyprpaper`: only when the wallpaper was picked in this panel

When `wallpaper.picture` is empty and `wallpaper.screens` has no item (the real description's defaults: a theme
decides the wallpapers, `hypr-theme.py set_wallpapers`), the writer leaves `hyprpaper.conf` as it is: an existing file
is not touched, a missing one is not made. Otherwise as in M1. (M2's test description gives `wallpaper.picture` a
default, so M2's steps are unchanged.)

## Import

`riley-settings-qt --import [--values FILE]` reads the files of the table above and writes the values file
(`$XDG_CONFIG_HOME/riley/settings.xml`, or FILE).

- **It never overwrites a values file.** When the file exists: exit 1, one line on stderr, nothing written.
- **It never writes a target** and runs no command. It writes the values file and nothing else.
- The file is made like M2's first change: the M2 header (`<?xml ...?>`, the comment
  `<!-- Riley settings: only what differs from the defaults. -->`, `<settings version="1">`, `</settings>`), then each
  key set through the M1 set path in **description order** (file order of `settings.xml`), so each new key goes on its
  own line before `</settings>`, two spaces in; lists in M2's list format (every field, the description's field
  order, attributes escaped).
- **A value the old files state is imported, even when it equals the description's default**; a value they leave
  out is not. (This keeps `settings.lua`, `idle.conf` and `settings.json` byte for byte: Riley wrote whole sections.)
  An empty list is not written.
- Numbers as `%g`; booleans `true`/`false`.
- A value that cannot be represented (a power time not among the choices, a dock icon size outside 28..64 or off
  its step, a desktop icon size outside 32..96 or off its step, a font size outside 6..48, an unknown launcher
  position) is left out, with one line on stderr: `riley-settings-qt: skipped <file>: <key> <value>: <reason>`.
- On success: exit 0 and one line on stdout: `imported <n> keys into <path>`.

### The mapping

| Old file, field | Key | Rule |
|---|---|---|
| model `monitors[]` | `display.monitors` | one item per entry: `desc`, `enabled` (missing: true), `mode`, `x`, `y`, `scale`, `transform` (missing: 0). Screens Hyprland reports but the model lacks are not added (no command runs) |
| model `appearance.<k>` | `appearance.<k>` | each of Riley's 11 keys present |
| `float-mode.json` `default` | `appearance.workspaces` | `tiled` or `floating` |
| `fonts.toml` `sans`, `mono`, `terminal_size` | `fonts.sans`, `fonts.mono`, `fonts.terminal_size` | each present; the family as it is |
| `weather.toml` `location`, `units` | `weather.location`, `weather.units` | each present; location trimmed (Riley's `_validate`) |
| model `input.<k>` | `input.<k>` | each present; `accel_profile` `""` becomes `adaptive`; `follow_mouse` as its number; `natural_scroll` bool |
| `current-theme` | `theme.name` | the file's text, trimmed, when the file exists |
| `hyprpaper.conf` + `wallpapers.json` | `wallpaper.picture`, `wallpaper.screens` | only when `hyprpaper.conf`'s first line is `# Written by hypr-theme.py (wallpaper picked in Riley Settings).` Then from `wallpapers.json` when it exists: `"*"` is the picture, every other entry a `{screen, path}` item in file order; else the path of the block whose `monitor =` is empty is the picture. With the theme's first line, or no file: nothing |
| `nwg-dock-pinned` | `dock.pins` | each line trimmed, empty lines left out (`load_pins`) |
| `riley.conf` `SCREENS=` | `dock.screens` | Riley's `screen_filters`: the last `SCREENS` line (key trimmed), split at `,`, each trimmed, empties and repeats left out |
| `riley.conf` `ICON_SIZE=` | `dock.icon_size` | the first line that starts with `ICON_SIZE=` whose rest is an integer (`icon_size`) |
| `riley.conf` `REFLECTIONS=` | `dock.reflections` | the last one, trimmed: `0` false, anything else true (`reflections`) |
| `riley.conf` `LAUNCHER=` | `dock.launcher` | the last one, trimmed, when it is `left`, `right`, `center` or `off` |
| `riley.conf` `AUTOHIDE=` | `dock.autohide` | the last one, trimmed: `1` true, anything else false (`autohide`) |
| `desktop.json` `show` | `desktop.show` | when present |
| `desktop.json` `boxes[]` | `desktop.boxes` | one item per box without `auto`, and one per `auto` box with `hidden: true`, in file order: `folder`, `screen`, `hidden` (missing: false). Places and sizes stay in `desktop.json` |
| `desktop.json` `icon_size` | `desktop.icon_size` | when present |
| model `startup[]` | `startup.apps` | `name`, `exec`, `enabled` (missing: true) |
| model `power` | `power.screensaver_after`, `power.lock_after`, `power.screens_off_after`, `power.sleep_after` | `screensaver`, `lock`, `screens_off`, `sleep`, when the model has a non-empty `power` |
| `idle.conf`, else `hypridle.conf` | the 4 `power.*` | only when the model has no `power`: `core.parse_idle`; all four are written (a step the file lacks is 0), as `current_times` shows them |

Riley's `accel_profile` and the panel's default differ (report 1): `""` means Hyprland's own, which is adaptive.

### The first start

When `riley-settings-qt` starts without `--import`/`--check`, finds no values file and finds the old model
(`~/.config/hypr/settings.json`), it runs the import first (the same function, the same result), then opens. With no
old model it opens as before and writes nothing.

## `--check`

`riley-settings-qt --check [--out DIR] [--values FILE] [--description FILE]` loads the description and the values,
runs every writer of the description's targets into a temporary directory (under `$TMPDIR`), compares each result
with the file at the target's real path, prints one line per target file, and exits. It writes nothing in the home
and runs no command.

- Order: the targets in the order they first appear in `settings.xml` (options in file order, then their `target`
  attribute's order), only targets with a writer; a target with several files gives one line per file. With today's
  description: `hyprland-lua`, `riley-model`, `hyprpaper`, `riley-dock` (pins, then `riley.conf`), `riley-desktop`,
  `hypridle`.
- Each line: `<target>\t<same|differs>\t<absolute path>`.
  `same`: the writer would leave exactly the bytes that are there (also: it would leave no file, and there is none).
  `differs`: anything else.
- Each writer is given the target file's present content where it uses one (M1's `--in`), and the writers that find
  their own files (`riley-dock`, `riley-desktop`) run with `changed` = the keys of their options that the values file
  sets; with none set they leave their files as they are. They may read the real home (`~/Desktop`'s folders) but
  write only into the temporary directory. Where `riley-desktop` would need the screens to place a new box, it takes
  the enabled screens of `display.monitors` (no command).
- **`--out DIR`**: also leaves in DIR every file the writers would leave at the targets (the unchanged ones too), at
  its path relative to `$HOME` (`DIR/.config/hypr/settings.lua`). Nothing for a target that would have no file.
- Exit 0: every line `same`. 1: at least one `differs`. 2: the description or the values cannot be loaded (one line
  on stderr, nothing on stdout).

## The launcher (Riley's `bin/riley-settings`)

The keybinding stays as it is: Super+Ctrl+S runs `riley-settings`, which is `~/.local/bin/riley-settings ->
~/git/riley/bin/riley-settings`. That file becomes a small launcher; the GTK panel's Python entry moves, unchanged, to
**`bin/riley-settings-gtk`** (today's `bin/riley-settings`: `from riley_settings.app import main`).

- `bin/riley-settings` is a POSIX `sh` script (first line exactly `#!/bin/sh`) that does not name `riley_settings`.
- It finds `riley-settings-gtk` next to itself after resolving its own symlinks (`readlink -f "$0"`), never on PATH.
- The panel chosen: `gtk` when an argument is `--gtk` (removed from the arguments; one run only); else the first word
  of `${XDG_CONFIG_HOME:-$HOME/.config}/riley/settings-panel` when that is `gtk`; else `qt` (missing file, `qt`,
  anything else).
- `qt`: when `riley-settings-qt` is on PATH, it runs `riley-settings-qt --apply-actions "$@"` as a child; when that
  exits 127 (it could not start), it runs the GTK panel; otherwise it exits with its code. When `riley-settings-qt` is
  not on PATH: the GTK panel.
- `gtk`: runs `riley-settings-gtk "$@"`, and exits with its code.
- Its argument loop holds exactly one line matching `^[[:space:]]*--gtk\) panel=gtk ;;$` (the gate's planted copy
  replaces it with `--gtk) ;;`).
- **`bin/riley-settings-use gtk|qt`** (POSIX `sh`, first line `#!/bin/sh`) writes that word and a newline into
  `${XDG_CONFIG_HOME:-$HOME/.config}/riley/settings-panel` (making the folder, by a temporary file and a rename),
  starts nothing, exits 0. Any other argument, or none: exit 2, a usage line on stderr, the file unchanged.
- Ways back: `riley-settings --gtk` for one run; `riley-settings-use gtk` for good; `riley-settings-use qt` to return.

## Keyboard layout picker (`widget:keyboard-mouse`)

Riley's `keyboard_mouse.py` picker (first layout, optional second layout, the key that switches), with one addition:
a variant per layout (Riley always wrote empty variants; with empty variants the result is Riley's exactly).

- Property **`layoutCodes`**: the codes of `/usr/share/X11/xkb/rules/evdev.lst`'s `! layout` section (lines
  `  <code>  <name>`), sorted by name ignoring case (Riley's `layouts()`, Python's stable sort); `["us"]` when the file
  is missing.
- Property **`switchKeys`**: `["grp:alt_shift_toggle", "grp:ctrl_shift_toggle", "grp:caps_toggle"]` (Riley's
  `SWITCH_KEYS`: Alt+Shift, Ctrl+Shift, Caps Lock).
- Function **`variantsFor(code)`**: `""` (none) followed by the variants of the `! variant` section whose description
  starts with `<code>:` (lines `  <variant>  <code>: <name>`), in file order.
- Function **`chooseLayouts(first, firstVariant, second, secondVariant, switchKey)`**: what the picker's controls call.
  `second` `""` means one layout.
  - Refused (false, a reason in the panel's message, nothing written or recorded) when `first` is not in
    `layoutCodes`, `second` is neither `""` nor in it, a variant is neither `""` nor in `variantsFor` of its layout,
    or `second` is set and `switchKey` is not in `switchKeys`.
  - Else (Riley's `on_layout`): `input.kb_layout` = `first` or `first,second`; `input.kb_variant` = `firstVariant`, or
    `firstVariant,secondVariant` when there is a second layout; `input.kb_options` = the options of the present value
    that are not empty and do not start with `grp:`, in order, then `switchKey` when there is a second layout, joined
    by `,`.
  - The three keys are saved as **one change**, in the order `kb_layout`, `kb_variant`, `kb_options` (each through the
    M1 set path; a value equal to the default removes its key); then each writer of the changed keys' targets runs
    once; then the actions of the changed keys, each once, in key order then attribute order. Returns true. When no
    value changes: nothing written or recorded, true.
- The M2 controls `option:input.kb_layout`, `option:input.kb_variant`, `option:input.kb_options` stay and show the
  new values.

## Printer options (`widget:printers`)

Riley's `printers_core.py` `OPTIONS` (`media` Paper size, `print-color-mode` Colour, `print-quality` Quality, `sides`
Two-sided) and `option_choices`, read through the CUPS command-line tools like the rest of the page:

| Command (argv) | When |
|---|---|
| `ipptool -tv ipp://localhost/printers/<queue> get-printer-attributes.test` | the queue's supported and default values (the IPP request pycups' `getPrinterAttributes` makes) |

- An option is offered when its `<option>-supported` line has type `1setOf ...` and more than one value (Riley:
  `isinstance(sup, list) and len(sup) > 1`). Its choices: those values, in order, without the ones starting with
  `custom_`. `print-quality` is an enum: ipptool's `draft`, `normal`, `high` are the choices `3`, `4`, `5` (pycups
  gives the numbers).
- Its value (Riley's selected row: `cur = mine.get(opt, default)`, then index 0 when `cur` is not a choice): your
  own value (`lpoptions -p <queue>`) when you have one, else the `<option>-default` (mapped the same way); and when
  that is not one of the choices, the first choice (so your Office colour `auto` shows as `monochrome`, not as the
  default `color`).
- Functions on `widget:printers`: **`optionNames(queue)`** (the offered options, in `OPTIONS` order),
  **`optionChoices(queue, option)`**, **`optionLabels(queue, option)`** (Riley's `NICE` names, else the value),
  **`optionValue(queue, option)`**. M3's `setOption(queue, option, value)` (`lpoptions -p <queue> -o
  <option>=<value>`) is what each option's control calls; the page then reads the queue's options again.
- One control **`printer-option:<queue>:<option>`** per offered option of each queue.
- Toner levels (Riley's `toner`, through `ipptool` on the printer itself) are not part of M4.

## No QML warning

`FontControl.qml` must not warn "Cannot read property 'family' of undefined" (report 3; it shows with the Basic style,
which the gates use, when a font option's value is empty). The gate starts `riley-settings-qt` offscreen, with
`QT_FORCE_STDERR_LOGGING=1` (else Qt sends its messages to the journal), once with `QT_QUICK_CONTROLS_STYLE=Basic` and
once with the program's own Fusion, lets it run 4 s and stops it: stderr must have no line matching
`\.qml|qml:|QQml|TypeError|ReferenceError|Binding loop`. The harness also shows every page once and fails on any
QML warning.

## The planted faults

With `-DRILEY_PLANT_FAULT_SWITCH=ON` (one build plants all of them; the launcher's is a copy the gate makes):

| Where | Fault | Breaks |
|---|---|---|
| import | the importer leaves out the last key it would write (description order) | import-fixture (m1 `power.sleep_after`, m3 `desktop.icon_size`, m4 `startup.apps`), import-real (Marc's last key) |
| layout picker | `kb_variant`'s first part is the second layout's variant (`std,std` for `us`/`ro std`) | layout steps 1 and 4 |
| printer options | `optionNames` leaves out the last option offered | printers (Office's quality, RileyGate's two-sided) |
| launcher | the gate's copy of `bin/riley-settings`, made with one `sed` line: `--gtk` is read and ignored | launcher (`--gtk` cases) |

The gate is proven when the groups import-fixture, import-real, launcher, layout and printers each have at least one
failing check in the fault run.

## The gate's sequence

Throwaway homes under `$BUILD_DIR/run`; `env -i`; traps first on PATH for the desktop's commands (and `python3`,
`riley-settings`, `riley-settings-gtk`); fakes before them for the harness. Groups:

| Group | What |
|---|---|
| gate | build; sha256 pins of `tests/switch/{expected,seeds,fixtures,fakes}` and the 4 reused M1 files; the option in CMakeLists.txt; `settings.xml` validates and has `riley-model` on exactly the 26 model options, `greeter-sway` on none |
| model | `riley-config write`: `riley-model` from `expected/import-mN.values.xml` = `expected/model-mN.json`; `hyprland-lua` from them = Riley's `expected-m1/m3/m4.lua`; `hypridle` from m1 = Riley's `expected-m7-idle.conf`; `hyprpaper` from m3, m4 = their fixture homes' `hyprpaper.conf` |
| import-fixture | homes m1, m3, m4 (below): `--import` with no display: exit 0, values = `expected/import-mN.values.xml`, `imported 5/39/3 keys into ...`, stderr empty, nothing else written; `--check`: exit 1, output = `expected/check-mN.txt`, nothing written |
| refuse | `--import` again in the m1 home: exit 1, one stderr line, home and values file untouched (bytes, mtime) |
| import-real | Marc's live files of the table above, copied read only into a home (and `~/Desktop`'s folder names); `--import` exit 0; the values valid with no unknown key; `--check --out`: exit 0, every line `same` (`expected/check-real.txt`), nothing written; every file in `--out` = Marc's live file byte for byte, and no file where Marc has none |
| start | offscreen, Basic and Fusion, the M3 fakes first on PATH for the pages' queries, empty home: runs until stopped, no QML warning, nothing written; first start in the m1 home without values: values = `expected/import-m1.values.xml`, nothing else written, no warning |
| install | `cmake --install --prefix`: exactly the 5 files; the installed binary with `RILEY_SETTINGS_SHARE` unset, `--check` in a home with `import-m1.values.xml`: exit 1, `expected/check-install.txt`, nothing written; a copy of the prefix without `share/`: exit 2 |
| launcher | Riley's `bin/riley-settings-gtk` is the GTK entry; `bin/riley-settings` is the sh launcher with the one `--gtk` line; from a copy (called through a symlink, as `~/.local/bin` does) with a fake `riley-settings-qt` and a fake `riley-settings-gtk` that record their argv: no file → `qt --apply-actions`; `--gtk` → `gtk`; qt not on PATH → `gtk`; qt exits 127 → `qt --apply-actions` then `gtk`; qt exits 3 → exit 3, no gtk; `riley-settings-use gtk` → `gtk\n` in the file, then → `gtk`; `riley-settings-use qt` → `qt\n`, then → qt; `--gtk` beats the file; `riley-settings-use maybe` → exit 2, file unchanged |
| pages | harness: every page shown once from an empty home: no QML warning, nothing recorded or written |
| layout | harness, seed `seeds/layout-values.xml` (fixture-m1's screens, `kb_layout` `de`, `kb_options` `caps:escape,grp:caps_toggle`): `layoutCodes`, `switchKeys`, `variantsFor("ro")`, `variantsFor("de")` against the gate's own reading of evdev.lst; step 1 `("us", "", "ro", "std", "grp:alt_shift_toggle")`: values `layout-values-1.xml`, `settings.lua` `layout-settings-1.lua`, `settings.json` `layout-model-1.json`, log 2 lines; step 2 `("us", "", "", "", "grp:alt_shift_toggle")`: `-2` files, log 4; step 3 `("us", "", "", "", "grp:caps_toggle")`: nothing; step 4 `("de", "nodeadkeys", "fr", "", "grp:ctrl_shift_toggle")`: `layout-values-3.xml`, log 6; 3 refusals; the home holds the values file, `settings.lua`, `settings.json` |
| printers | harness, the M3 fake queues plus a fake `ipptool` (`fixtures/ipptool-*.txt`) and your Office colour set to `auto` (not a choice): `optionNames` Office `[media, print-color-mode, print-quality]`, RileyGate `[media, print-color-mode, sides]`; each option's choices, labels and value; 6 `printer-option:` items; 3 changes read back; their argv = `expected/printers-option-calls.log` |
| untouched | no trap called; no unexpected fake call; the repo's working tree as before; Marc's files (model, targets, values, `settings-panel`, pins, lpoptions, `~/.local/bin` launchers) unchanged (mtime, size) |

The fixture homes: Riley's `fixture-mN.json` as `~/.config/hypr/settings.json`, Riley's `expected-mN.lua` as
`settings.lua`, then `tests/switch/fixtures/mN-home/` (m1: also Riley's `expected-m7-idle.conf` as `idle.conf`).

| Home | Its other files | Import result |
|---|---|---|
| m1 | `hyprpaper.conf` written by the theme | the 5 screens; the 4 power times of `idle.conf` (0, 600, 900, 3600); no wallpaper |
| m3 | `float-mode.json` (floating), `fonts.toml`, `weather.toml`, `current-theme`, `hyprpaper.conf` picked per screen with `wallpapers.json`, a messy `nwg-dock-pinned` and `riley.conf` (blank line, spaces, a repeated screen, two `ICON_SIZE` lines), `desktop.json` with an auto box, a hidden auto box and an added box | 39 keys |
| m4 | `hyprpaper.conf` picked, one picture everywhere, no `wallpapers.json` | the screens, `wallpaper.picture`, `startup.apps` |

## Where the expected results come from

| File | Source |
|---|---|
| `fixtures/fixture-m1/m3/m4.json` | copied byte for byte from Riley `settings/tests/` (commit ea5f243) |
| `expected/import-*.values.xml` | by hand, from the fixtures and the mapping above; the dock and font readings checked with Riley's own `dock_core`, `fonts_core`, `weather_core` on the m3 home |
| `expected/model-*.json`, `layout-model-*.json` | `tests/switch/make-expected.py`: the hand-worked values in Riley's model, formatted by Python's `json.dumps(indent=2)` as `core.py` does, with the `%g` number rule; checked there to hold the same data as Riley's fixtures |
| `expected/layout-settings-*.lua` | `make-expected.py`: Riley's `core.render` of the same model |
| `expected/layout-values-*.xml` | by hand, from Riley's `on_layout` and the M1/M2 set and removal rules |
| `expected/check-*.txt` | by hand, from the `--check` rules and what each fixture home holds |
| `fixtures/*-home/.config/hypr/hyprpaper.conf` | by hand, in `hypr-theme.py`'s `set_wallpapers` and `set_bg` formats |
| `fixtures/ipptool-*.txt` | the format of `ipptool -tv ... get-printer-attributes.test` on this machine's CUPS (C locale), with made-up queues |
| `expected/printers-option-calls.log` | Riley's `set_options` argv for the three changes |
| the layout codes and variants | read at run time from the system's `evdev.lst` by the gate, with Riley's `layouts()` rule |
| Marc's live files | the independent source of the import-real group, compared at run time, never copied into the repo |

## What is not checked here (left for Marc)

The switch on the live desktop: Super+Ctrl+S opening the Qt panel, `--apply-actions` reloading for real, the greeter
following `settings.json` (`riley-greeter-sync.path`, needs the live system), the real `ipptool` answer of Marc's own
queues, and using the GTK panel through `riley-settings --gtk` afterwards.
