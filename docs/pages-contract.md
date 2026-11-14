# Custom pages contract (M3)

How the M3 gate (`tests/run-pages.sh`, harness `tests/pages/tst_pages.cpp`) finds and drives the custom pages. The
pages must meet every point here; the gate checks them against results worked out before the pages existed, in
`tests/pages/expected/`. Everything in `docs/panel-contract.md` (M2) still holds, except where this file says
otherwise.

## What the build provides

- Everything M2 provides (`riley-settings-panel`, `riley-settings-qt`, `panel/panel.h`).
- One QML component per widget of `share/riley-settings/settings.xml`, compiled into the panel's QML module. A page
  with `widget="<name>"` is drawn by the component for `<name>`; its menu entry, title, help and order still come from
  the file. The root item of the component has objectName **`widget:<name>`**.

  | widget | page | component |
  |---|---|---|
  | `displays` | Displays | `qml/pages/Displays.qml` |
  | `printers` | Printers | `qml/pages/Printers.qml` |
  | `keyboard-mouse` | Keyboard and Mouse | `qml/pages/KeyboardMouse.qml` |
  | `theme-wallpaper` | Theme and Wallpaper (the wallpaper part) | `qml/pages/ThemeWallpaper.qml` |
  | `dock` | Dock | `qml/pages/Dock.qml` |
  | `desktop` | Desktop | `qml/pages/Desktop.qml` |
  | `system-launcher` | Network, Bluetooth, Sound, Users... | no page: the entry launches (see below) |

  `webapps` is not part of M3: it keeps the M2 placeholder. A widget the build has no component for shows
  `placeholder:<widget>` as in M2 (the M2 test description now names its widget page `m2-placeholder` for that
  reason).
- The CMake option **`RILEY_PLANT_FAULT_PAGES`** (default OFF), see "The planted faults".
- Writers for the targets `riley-dock` and `riley-desktop` (below). Writers are code and may name keys; the panel's
  code may not.

The gate builds `tests/pages/CMakeLists.txt` (like `tests/panel/`), which links `tst_pages` with
`riley-settings-panel`, `Qt6::Test` and `Qt6::Quick`.

## Rules for every page

- **No option key in the panel's code.** The M2 grep check stays: no file under `src/panel/`, `src/app/` or `qml/`
  (pages included) contains an option key of `share/riley-settings/settings.xml` or `tests/panel/settings.xml`. A
  page finds the options it works on through the description and values API (its `PageItem`'s options). The
  suggested way: a `role` attribute on each option a page treats specially (`role="pins"` on `dock.pins`), added to
  `settings.xsd`; a role is not a key.
- **Scalar options of a widget page are M2 controls.** On the keyboard-mouse, dock and desktop pages every option
  that is not a list is drawn as the M2 control of its type: objectName `option:<key>`, `controlType`, `valueText`,
  driven exactly as `docs/panel-contract.md` says (click, `Key_Down`, `Key_Right`, typing). A change of one runs the
  M2 change sequence (values file, writers, actions).
- **A page change is a change.** Whatever a page saves goes through the M2 change sequence: the values file through
  the M1 set path (list format, new key before `</settings>`, a list equal to its default removes the key), then the
  writers of the option's targets, then its actions, recorded in `$RILEY_SETTINGS_ACTION_LOG`. One user step that
  sets several keys saves them together, runs each writer once and records each action once. A step that leaves every
  value as it was writes nothing and records nothing.
- **Number fields of list items** are written as `%g` (`1`, `1.5`, `1.25`, `407`).
- **A refusal** writes no file, runs no writer or action and puts a one-line reason in the panel's message: the root
  window's `panel` property (the controller) has a non-empty `message`.
- **Commands** are run with a fixed argv (never a shell line), found on `PATH`, with `LC_ALL=C`. Two kinds:
  - *queries and your own settings* (the tables below): run always. The gate puts fakes first on `PATH` that record
    argv and answer from canned files; any argv not listed here is an unexpected call and fails the gate.
  - *launches* of another program (`hypr-settings`, `system-config-printer`): treated like actions. Each appends
    `launch <program>` to the action log; the program runs only with `--apply-actions`.
- **Functions the gate calls** are QML functions on the named item, with untyped parameters (the harness passes
  `QVariant`s: strings, numbers, booleans, lists of strings). They are the same functions the page's mouse, drag and
  drop handlers and dialogs call; a page may not change anything by another path. Their effects are done before they
  return, except where the page runs a command: then the gate waits up to 5 s for the result to show.
- Opening the panel or a page writes nothing.

## Files and commands

Paths under the throwaway home of the gate. `$XDG_CONFIG_HOME` and `$XDG_CACHE_HOME` as set by the environment.

| What | Where |
|---|---|
| Values, targets of M1/M2 | as in `docs/panel-contract.md` (`hypr/settings.lua`, `hypr/hyprpaper.conf`, `riley/greeter-sway.conf`, ...) |
| Target `riley-dock`: pins | `$XDG_CACHE_HOME/nwg-dock-pinned`, whole: one app id per line, in dock order (the file riley-dock-qt and the Rust dock read) |
| Target `riley-dock`: settings | `$XDG_CONFIG_HOME/nwg-dock-hyprland/riley.conf`, one `KEY=value` line per option (below), every other byte kept |
| Target `riley-desktop` | `$XDG_CONFIG_HOME/riley/desktop.json` (Riley's `desktop/riley_desktop/layout.py` format) |
| The desktop's folders | `$HOME/Desktop` and each folder in it |
| Apps for the dock | `.desktop` files in `$XDG_DATA_HOME/applications`, then in each `$XDG_DATA_DIRS/*/applications` |

| Page | Command (argv) | When |
|---|---|---|
| displays, desktop | `hyprctl monitors all -j` | the screens Hyprland knows, connected or off |
| wallpapers, dock | `hyprctl monitors -j` | the screens in use |
| displays | `Hyprland --verify-config -c <candidate settings.lua>` | before Apply writes anything; refused unless the last output line is `config ok` and the exit code 0 |
| printers | `lpstat -l -p`, `lpstat -a`, `lpstat -d`, `lpstat -l -o` or `lpstat -l -o <queue>`, `lpoptions -p <queue>` | reading queues, accepting, your default, jobs, your options |
| printers | `lp -d <queue> -t "Test page from Riley Settings" /usr/share/cups/data/testprint` | test page |
| printers | `cancel <request id>` | cancel a job |
| printers | `lpoptions -p <queue> -o <option>=<value>` | one of your own options |
| printers | `lpoptions -d <queue>` | your default printer |

The `lpoptions` argv are Riley's (`printers_core.py` `set_options`, `set_default`). Riley read queues, jobs and
printed through pycups (libcups); the `lpstat`, `lp` and `cancel` argv are the CUPS tools that make the same requests.
What their output looks like (C locale): `tests/pages/fixtures/lpstat-*.txt`. A job is `held` when its `Alerts:` line
holds `job-hold-until-specified`.

## Displays (`widget:displays`)

The screens: `display.monitors` from the values file when it is set, else the list Hyprland gives (`hyprctl monitors
all -j`, in its order: `desc` = `description`, `enabled` = not `disabled`, `mode` = `<width>x<height>@<refreshRate
%.2f>`, `x`, `y`, `scale` rounded to 2 places, `transform`, `bitdepth` 10 when `currentFormat` holds `2101010`).
Live-only fields (port name, modes, depths) are never saved.

- **Colour depth:** `bitdepth` is the values file's, except that a screen running 10-bit now (`currentFormat` holds
  `2101010`) while the values file says 8 or nothing shows 10, so Apply does not undo a change made in settings.lua.
  `depths` offers "8", plus "10" when the EDID in `$RILEY_DRM_DIR` (default `/sys/class/drm`) `/card*-<port>/edid`
  gives 10 or more bits per colour (EDID 1.4 byte 20, or the HDMI block's DC_30bit) or the screen runs 10-bit now.
  Hyprland takes only 8 and 10. Gate: `tests/run-bitdepth.sh`.

- One item **`screen:<desc>`** per enabled screen, drawn to scale; properties **`screenX`**, **`screenY`**: its place
  in the arrangement being edited, in logical pixels.
- Properties on `widget:displays`: **`dirty`** (the screens being edited differ from `display.monitors` in the values
  file; true when that key is not set), **`pending`** (applied, waiting for Keep), **`keepSeconds`** (15),
  **`secondsLeft`**.
- Functions on `widget:displays`:
  - **`dragMove(desc, dx, dy)`**: the drag so far, `dx`, `dy` in logical pixels from where the screen was when the
    drag began (the drag handler divides the pointer's movement by the drawing's scale). Riley's rule
    (`displays.py` `on_update`): the new place is snapped, separately in x and in y, to the nearest of each other
    enabled screen's edge candidates (`o.x - w`, `o.x + o.w`, `o.x`, `o.x + o.w - w`, the same in y) when it is
    nearer than 80; else it is the unsnapped place, rounded.
  - **`dragEnd(desc)`**: returns false and moves the screen back to where the drag began, with a message, when it
    now overlaps another enabled screen (touching is fine); else returns true and shifts every screen (enabled or
    not) so the smallest x and y of the enabled screens are 0.
  - **`setScreenField(desc, field, text)`**: what each screen's controls call (`enabled`, `mode`, `scale`,
    `transform`).
  - **`reset()`**: back to what the values file says (Riley's Reset).
- Buttons **`displays:apply`**, and while `pending`, **`displays:keep`** and **`displays:revert`** (in Riley's
  dialog: Keep, Go back).
- **Apply**: checks the edited screens against the description (a field that is not valid, such as scale `abc`,
  is a refusal, before any command); renders the `hyprland-lua` target to a temporary file and runs `Hyprland
  --verify-config -c` on it (a refusal when it fails). Then it remembers the bytes (or absence) of the values file
  and of every target file of `display.monitors`, saves `display.monitors` as one change (targets `hyprland-lua
  greeter-sway hyprpaper`, actions `hyprland-reload greeter-sync`), and sets `pending`, `secondsLeft` 15.
- **Keep**: ends `pending`; nothing else is written.
- **Revert**, and the timer when `secondsLeft` reaches 0: writes back exactly the remembered bytes (a file that did
  not exist is removed), without checking, then records the same actions again (Riley's `restore` reloads).

## Printers (`widget:printers`)

- One item **`printer:<queue>`** per queue of `lpstat -l -p`, properties **`printerInfo`** (`Description:`),
  **`printerLocation`** (`Location:`), **`printerState`** (`idle`, `printing`, `stopped`), **`accepting`** (bool,
  from `lpstat -a`), **`isDefault`** (bool, from `lpstat -d`).
- One item **`job:<request id>`** per job not completed, property **`jobState`** (`waiting`, `held`, `printing`,
  `stopped`).
- Functions on `widget:printers`: **`refresh()`**, **`printTestPage(queue)`**, **`cancelJob(requestId)`**,
  **`setOption(queue, option, value)`**, **`userOption(queue, option)`** (returns your value from `lpoptions -p
  <queue>`, or ""), **`setDefault(queue)`**, **`openSetupTool()`** (launch `system-config-printer`). After each
  command that changes something the page reads the state again.
- Nothing is written in the home (the real `lpoptions` writes `~/.cups/lpoptions`; the fake does not).

## Keyboard and Mouse (`widget:keyboard-mouse`)

Every option of the page (`input.*`) is an M2 control `option:<key>` (see "Rules"). The page may add helpers (a
layout picker, a box to try key repeat) that end in the same controls' commit functions.

## Wallpapers (`widget:theme-wallpaper`)

The screens: `hyprctl monitors -j` (enabled screens, in its order).

- Functions on `widget:theme-wallpaper`:
  - **`screenAt(x, y, width, height)`**: the `desc` of the screen under the point (x, y) of a drawing of the
    screens `width` x `height` big, or "". Riley's rule (`core.py` `layout_rects`, `screen_at`): logical sizes
    (rotated by `transform` odd, divided by `scale`, rounded), scale `s = min((width - 40) / spanX, (height - 40) /
    spanY)`, centred; the last screen in the list wins where they overlap. The drop area calls
    `screenAt(drop.x, drop.y, width, height)`.
  - **`dropPicture(path, desc)`**: what a drop calls. Refused when `desc` is "" or the path does not end in `.jpg`,
    `.jpeg`, `.png`, `.webp`, `.avif` or `.jxl` (any case). Else `wallpaper.screens` gets `{screen: desc, path}`:
    the item of that screen replaced in place, or a new item at the end.
  - **`useEverywhere(path)`**: a click on a picture: `wallpaper.picture` = path and `wallpaper.screens` emptied, in
    one change (Riley's `hypr-theme --bg FILE` without `--screen`).
- The theme cards (`theme.name`, `hypr-theme NAME`) are not part of M3.

## Dock (`widget:dock`)

- The dock strip **`dock:strip`**: one item **`pin:<app id>`** per pin, in order, each 48 px wide, side by side from
  x = 0 (strip coordinates), so their centres are at 24, 72, 120, ... as in Riley's gate.
- Property **`pins`** (list of app ids, strip order) and **`appIds`** (the app list's ids in shown order: every
  `.desktop` file without `NoDisplay=true` or `Hidden=true`, sorted by `Name` ignoring case, the id being the file name
  without `.desktop`). One item **`app:<app id>`** per app.
- Functions on `widget:dock`:
  - **`dropOnDock(value, x)`**: a drop on the strip at x (strip coordinates). `value` is `pin:<id>` (dragged from the
    strip) or `app:<id>` (from the app list). Riley's rule (`dock_core.py` `insert_index`, `move_pin`): the place is
    the number of pin centres left of x; a pinned app moves there (one place less when it moves right), any other is
    added there.
  - **`dropOnRemove(value)`**: a drop on Remove. `pin:<id>` takes the pin off; anything else is refused.
  - **`setScreens(descs)`**: what the screen check boxes call; `dock.screens` = those descriptions.
- `dock.pins` saves the pins file. `riley.conf` is written one line per change, for the option changed only, with
  Riley's rule (`dock_core.py` `_set_config`): the first line whose part before `=` is the key (trimmed) is replaced by
  `KEY=value` (keeping a `\r\n` end), later lines of that key are dropped, and when there is none the line is added at
  the end (after a newline if the file does not end with one); every other byte is kept and the file is replaced by a
  rename, never rewritten in place. Keys: `dock.icon_size` `ICON_SIZE=<n>`, `dock.screens` `SCREENS=<desc,desc>`,
  `dock.reflections` `REFLECTIONS=1|0`, `dock.launcher` `LAUNCHER=<choice>`, `dock.autohide` `AUTOHIDE=1|0`. (A
  `config.xml` of riley-dock-qt, if present, is not written in M3.)

## Desktop (`widget:desktop`)

The rules of Riley's `desktop/riley_desktop/layout.py`, ported: boxes of 440 x 320 (the Desktop box) and 340 x 240,
first at (48, 72), then down the column with 16 px gaps, a new column when a box would reach the bottom 90 px; the
main screen is the one whose description holds `HCNYB00237` (from `hyprctl monitors all -j`, logical size).

- Property **`shown`**: the boxes the desktop shows now, in `desktop.json` order, each `"<title> <x> <y> <w> <h>"`
  (Riley's `resolve`: hidden boxes and every box while `desktop.show` is false are left out).
- Functions on `widget:desktop`: **`addFolder(path)`** (returns false when the folder has a box), **`setHidden(folder,
  hidden)`**, **`removeBox(folder)`** (returns false for the box of a folder inside `~/Desktop`), **`reload()`** (what
  showing the page does: reads `desktop.json` and `~/Desktop` again; writes nothing).
- Each change reads `desktop.json` fresh, adds boxes for new folders of `~/Desktop` (Riley's `resolve`), makes the
  change and writes it. `desktop.json` holds `boxes` as Riley writes them (`folder`, `screen`, `x`, `y`, `w`, `h`,
  `auto: true` for a `~/Desktop` folder's box, `hidden: true` when hidden) and `show` and `icon_size` (Riley reads them
  with the defaults true and 48; absent is the same as the default). The gate compares it as JSON, not as bytes.
- `desktop.boxes` in the values file: one item per box you added and one per hidden `~/Desktop` box, in
  `desktop.json` order, fields `folder`, `screen`, `hidden`.

## The system launcher

A click on the menu entry `page:system` records `launch hypr-settings` (Riley's `app.py`: `hypr-settings`) and leaves
the page that was shown on screen.

## The planted faults

With `-DRILEY_PLANT_FAULT_PAGES=ON` (one build plants all of them):

| Page | Fault | Breaks |
|---|---|---|
| displays | Revert writes back every remembered file but `settings.lua` | Riley gate_m1 check 4: the live file is back |
| printers | `printTestPage` prints on your default queue (`lp -d Office ...`) | Riley gate_m2 check 2 and the argv |
| keyboard-mouse | the page's second option (`input.kb_variant`) saves to its third option's key | fixture-m3's values, expected-m3.lua |
| wallpapers | `screenAt` answers the screen after the one under the point (in list order; "" after the last) | Riley gate_m5 check 1 and 2 |
| dock | a drop on the strip lands one place too far right | Riley gate_m6 steps 2, 4, 5 |
| desktop | a new box goes 24 px below the box above it, not 16 | Riley gate_d4 checks 1 and 2 |
| system launcher | the entry launches `gnome-control-center` | Riley app.py's launch |

The gate is proven when every page group has at least one failing check in the fault build.

## The gate's sequence

Each group starts the panel afresh (window 1280 x 1600, offscreen) on a wiped throwaway home with its seed files, an
empty action log and an empty fake call log, and ends with a check that the home holds only the files listed.

| Group | Seed | Steps and results |
|---|---|---|
| displays | `seeds/displays-values.xml`, `seeds/displays-settings.lua` as `hypr/settings.lua` | 4 `screen:` items at fixture-m1's places; `dirty`; scale `abc` then Apply: refused, nothing written, no command; `reset()`; fake checker refuses: refused, nothing written; Apply: candidate and `settings.lua` = `expected-m1.lua`, settings.json = `displays-model.json`, hyprpaper = `displays-hyprpaper.conf`, values = `displays-values-applied.xml`; Revert: every file back; Apply then 13 s: still applied, 17 s: back by itself; Apply, Keep, 16 s: still applied, `dirty` false; drags below; action log = `displays-actions.log` |
| printers | fake default Office | RileyGate idle, accepting, "Riley gate queue", "Riley gate"; test page: `job:RileyGate-12` held; cancel: gone; `print-color-mode=monochrome` read back; default RileyGate then Office again; calls that change something = `printers-calls.log`; `launch system-config-printer` |
| keyboard-mouse | `seeds/keyboard-values.xml` | type `us,ro`, `,std`, `grp:alt_shift_toggle`; `Key_Right` x5 on repeat_rate, `Key_Left` x20 on repeat_delay, `Key_Left` x4 on sensitivity, `Key_Down` on accel_profile: values = `keyboard-values.xml`, `settings.lua` = `expected-m3.lua`, settings.json = `keyboard-model.json`, log = `keyboard-actions.log` |
| wallpapers | `seeds/wallpapers-values.xml` | the 6 probes of Riley gate_m5 check 1 on 852 x 331; a `.txt` refused; a.jpg on (328, 133), b.jpg on (90, 148): `wallpapers-values-1/2.xml`, `wallpapers-hyprpaper-1/2.conf`; c.jpg everywhere: `-3` |
| dock | `seeds/dock-values.xml`; then each `seeds/dock-conf-*.conf` as `riley.conf` | centres 24, 72, 120, 168; the app list; Riley gate_m6's 6 steps: values and pins file after each; reopened, the pins read back; icon size 44 to 46 to 48, screens one then none, reflections off, launcher right, auto-hide on and off: `riley.conf` = the `dock-conf-*` files; log = `dock-actions.log` |
| desktop | none; `~/Desktop/Projects`, `~/Downloads` | Riley gate_d4 checks 1 to 3, then icon size 56 and the switch off: `shown`, `desktop.json` = `desktop-json-N.json`, values = `desktop-values-N.xml` after each; log = `desktop-actions.log` |
| system-launcher | none | `page:power`, then `page:system`: `launch hypr-settings`, `option:power.lock_after` still shown |

Display drags (from Keep's state; worked out by hand with the rule above): the ARZOPA 0x37330206 screen by
(+1680, 0) snaps to (3173, 1847), against both ultrawides' edges; then by (-1733, -500) it would overlap the left
ultrawide, so it goes back to (3173, 1847) and `dragEnd` returns false. `reset()` puts it at (1440, 1847). The Dell by
(-500, 0) stays unsnapped and the end shifts every screen right by 500: the left ultrawide at (1940, 407).

## Where the expected results come from

| File | Source |
|---|---|
| `expected-m1.lua`, `expected-m3.lua` | copied byte for byte from Riley `settings/tests/` (commit ea5f243, unchanged since df8c270) |
| `displays-model.json`, `keyboard-model.json` | M4 (docs/switch-contract.md, "Changes to the description"): the greeter file became Riley's model `hypr/settings.json`, the file `riley-greeter-sync.path` watches (Riley `sddm/install-sddm.sh`). Riley's model format (`json.dumps(model, indent=2)`, %g numbers) of `displays-values-applied.xml` and `keyboard-values.xml`; the second is byte for byte `tests/switch/expected/model-m3.json` (Riley's `fixture-m3.json`), the first's screens are `fixture-m1.json`'s |
| `displays-*`, `wallpapers-*`, `keyboard-values.xml`, `dock-values-*`, `desktop-values-*` | by hand, from Riley's hand-worked values (fixture-m1, fixture-m3, gate_m5, gate_m6, gate_d4) and the M2 values-file rules |
| `wallpapers-hyprpaper-1/2.conf` | by hand from fixture-m1's enabled screens in list order: the new m5 case of Marc's answer Y1 (report 1) |
| `wallpapers-hyprpaper-3.conf` | Riley gate_m4 check 4 (`monitor =` with the chosen path) |
| `dock-pinned-*` | Riley gate_m6's `WANT` lists and gate_m4 check 3's format |
| `dock-conf-*` | the literal strings of Riley's gate_dock_screens/reflections/launcher/autohide (`ICON_SIZE=46` is the one step between, by hand) |
| `desktop-json-*` | Riley desktop gate_d4's boxes and places |
| `printers-calls.log` | Riley gate_m2's steps as the argv above |
| `*-actions.log` | the actions of the options in `settings.xml`, step by step |
| `fixtures/monitors*.json` | Riley's fixture-m1.json as `hyprctl` reports it (port names and modes made up) |

## What is not checked here (left for Marc)

Riley's checks that need the live desktop, sudo, a real printer queue, a running Hyprland, hyprpaper or the dock:
gate_m1 check 2 and gate_m3 check 2 (the real `Hyprland --verify-config`), gate_m2 with a real CUPS queue (sudo
`lpadmin`), gate_m3 check 4 (values live in Hyprland, then undone), gate_m5 check 3 (hyprpaper shows the drop),
gate_d4 check 4 (the desktop follows `desktop.json` within 1 s), and once: the canned `lpstat` output against the real
tools.
