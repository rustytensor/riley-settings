# Panel contract (M2)

How the M2 gate (`tests/run-panel.sh`, harness `tests/panel/tst_panel.cpp`) finds and drives the panel. The panel
must meet every point here; the gate checks them with results worked out by hand in `tests/panel/expected/`.

## What the build provides

- A library target **`riley-settings-panel`**: the C++ models and the QML module (`qml/`, compiled in with
  `qt_add_qml_module`, `Main.qml` its window). Linking it is all a program needs to show the panel.
- The program **`riley-settings-qt`** (`src/app/`), built to `${CMAKE_BINARY_DIR}`, a thin `main()` over the library.
- A header **`panel/panel.h`** (under `src/`, so `#include "panel/panel.h"`) declaring:

```cpp
namespace riley::panel {
struct LoadOptions {
    bool applyActions = false;   // true only from --apply-actions (M4's launcher)
};
// Reads the description and the values (see "Files"), exposes the models to QML and loads Main.qml into engine.
// Its root object is a QQuickWindow. Returns false with a one-line reason in *error when anything fails.
bool load(QQmlApplicationEngine& engine, const LoadOptions& options, QString* error);
}
```

  If the QML module of a static library needs an explicit import, `load()` (or the library) does it; the harness
  includes only `panel/panel.h`.
- The CMake option **`RILEY_PLANT_FAULT_PANEL`** (default OFF), see "The planted fault".

The gate builds `tests/panel/CMakeLists.txt`, which adds the repo as a subdirectory and links `tst_panel` with
`riley-settings-panel`, `Qt6::Test` and `Qt6::Quick`.

## Files

| What | Where |
|---|---|
| The description | `$RILEY_SETTINGS_SHARE/settings.xml`, schemas in `$RILEY_SETTINGS_SHARE/schema/` (default: the installed share directory) |
| Your values | `$XDG_CONFIG_HOME/riley/settings.xml` (default `$HOME/.config/riley/settings.xml`) |
| Target `hypridle` | `$XDG_CONFIG_HOME/hypr/idle.conf` |
| Target `hyprpaper` | `$XDG_CONFIG_HOME/hypr/hyprpaper.conf` |
| Target `hyprland-lua` | `$XDG_CONFIG_HOME/hypr/settings.lua` |
| Target `greeter-sway` | a file under `$XDG_CONFIG_HOME/riley/` (the build chooses its name; `greeter-sync` installs it) |
| Targets with no writer yet | not written; one line on stderr |

Opening the panel writes nothing. Nothing else in the home is read or written.

## Pages

- The menu holds one entry per page of the description, in `order`, top to bottom. Entry objectName:
  **`page:<id>`**. A mouse click on it shows that page.
- A drawn page shows its sections, in file order, and in each section one control per option, in file order.
- A widget page (`widget="..."`) shows, in M2, only a plain placeholder with objectName
  **`placeholder:<widget>`**. Its options get no control: no item anywhere is named `option:<key>` for them.
- At the gate's window size (1280 x 1600) every control of the test description's pages is inside the window
  without scrolling.

## Controls

Each option of a drawn page is one control, chosen by the option's type (one QML component per type):

- objectName **`option:<key>`**. While its page is shown exactly one *visible* item has that name.
- property **`controlType`** (string): the option's type name as in the XSD (`switch`, `choice`, `slider`, `number`,
  `text`, `colour`, `font`, `file`, `folder`, `keybinding`, `list`).
- property **`valueText`** (string), every type but `list`: the current value as the values file writes it (the
  default when the key is not set): `true`/`false`, the choice's value (not its label), `%g` for `slider` and
  `number`, the text itself for the others.
- property **`itemCount`** (int), `list` only: the number of items.

### How the gate changes each type

Mouse and key events go through `QTest` on the window, at the control's centre (`mapToScene`). "Focus" means the
gate calls `forceActiveFocus()` on the `option:<key>` item and waits until the window's active focus item is that
item or inside it.

| Type | The gate does | One change is |
|---|---|---|
| `switch` | one left click at the centre | the toggle |
| `choice` | focus, then one `Key_Down` | the next choice; `Key_Down`/`Key_Up` with the list closed move one choice and commit at once (as Qt Quick Controls' `ComboBox` does) |
| `slider` | focus, then one `Key_Right` | value + `step`, committed at once (`Key_Left` the other way) |
| `number` | focus, `Ctrl+A`, type the number as text (`42.25`), `Key_Return` | the typed number, committed on Return. Decimals must work (not an integer-only `SpinBox`) |
| `text` | focus, `Ctrl+A`, type the text, `Key_Return` | the typed text, committed on Return (and on focus loss) |
| `colour` | `commitValue("#1a2b3c")` | that colour |
| `font` | `commitValue("Noto Sans 12")` | that font |
| `file` | `commitValue("/path/to/file")` | that path |
| `folder` | `commitValue("/path/to/folder")` | that path |
| `keybinding` | `commitValue("SUPER+SHIFT+K")` | that binding: modifiers in the order `SUPER`, `CTRL`, `ALT`, `SHIFT`, then the key, joined by `+` |
| `list` | `commitItems([{screen: "DP-1", path: "/a.png"}])` | the whole list replaced by these items |

`commitValue(value)` and `commitItems(items)` are QML functions on the `option:<key>` item with an untyped
parameter (the harness calls them with `QMetaObject::invokeMethod` and a `QVariant`: a string, or a list of maps
from field name to string). They are **the same functions the control's dialog or item editor calls on OK**; the
gate replaces only the dialog step. A control may not save by any other path.

## A change

One change (one row above) does, in this order, before control returns to the event loop:

1. **Save the values file** through the M1 set path (comments and every other byte kept):
   - value equal to the option's default: the key is removed (see below); else the key is set.
   - no values file yet: it is first created as exactly
     ```
     <?xml version="1.0" encoding="UTF-8"?>
     <!-- Riley settings: only what differs from the defaults. -->
     <settings version="1">
     </settings>
     ```
     and then the value is set as M1's `set` does (a new key goes on a new line before `</settings>`, indented like
     the last value, two spaces when there is none).
   - a list is written as its own element, every field of every item in the description's field order, a field
     missing from an item taking the field's default (else empty), attribute values escaped (`&amp; &lt; &gt; &quot;`):
     ```
       <value key="wallpaper.screens">
         <item screen="DP-1" path="/a.png"/>
       </value>
     ```
     A new list key goes where a new scalar key goes; an existing one is replaced from its start tag to its end
     tag. An empty list is the default: the key is removed.
   - removing a key deletes its element; when only white space is before it on its first line and after it on its
     last line, those whole lines go, newline included. Removing a key that is not set changes nothing.
   - a value the set path refuses (out of range, not a choice): no file, writer or action is touched; the control
     shows the old value again and the panel shows the reason.
2. **Run the writers** of the option's `target`s, in attribute order, from the saved values, into the files above.
3. **Run the actions** of the option's `action`, in attribute order. `none` runs and records nothing.

### Actions are recorded, not run

- For each action it would run, the panel appends one line `<action>\n` to the file named by
  `$RILEY_SETTINGS_ACTION_LOG` when that is set.
- It executes an action only when started with `--apply-actions` (`LoadOptions::applyActions`). Then each action
  name maps to one fixed argv, never a shell line.
- With neither, the action is only printed on stderr.
- Nobody runs the panel with `--apply-actions` in M2. The gate also starts it with a `PATH` whose first directory
  holds traps named like the desktop's commands (`hyprctl`, `systemctl`, `pkill`, ...); any call fails the gate.

## The panel names no option

Pages, sections and controls come from `settings.xml` through the C++ models. No file under `src/panel/`,
`src/app/` or `qml/` contains an option key of `share/riley-settings/settings.xml` or `tests/panel/settings.xml`,
not even in a comment (the gate greps for each key as a whole word).

## The planted fault

With `-DRILEY_PLANT_FAULT_PANEL=ON` the control of the **second option in description order** (file order over all
pages, sections and options) is bound to the **key of the third option**: it shows that key's value and saves to
that key. Everything else is unchanged. In the test description that is `power.lock_after`'s choice control bound
to `test.slider`, so its `valueText` is wrong at load and its change saves `300` to `test.slider` (refused: above
max 10), so `values-02.xml` and every later step must fail. The gate is proven when they do.

## The gate's sequence

From no values file, window 1280 x 1600, offscreen. Before the steps: every page entry, its order, each drawn
option's `controlType` and default `valueText`, the placeholder, and no control for the widget page's options.
Then one change per drawn option in description order, and one back to the default:

| Step | Page | Key | Input | After it |
|---|---|---|---|---|
| 1 | basic | `test.switch` | click | `true`; `idle.conf` = `idle-01.conf` |
| 2 | basic | `power.lock_after` | `Key_Down` | `300`; `idle.conf` = `idle-02.conf` |
| 3 | basic | `test.slider` | `Key_Right` | `6` |
| 4 | basic | `test.number` | `42.25` Return | `42.25`; `hyprpaper.conf` = `hyprpaper-04.conf` |
| 5 | basic | `test.text` | `a & b` Return | `a & b` (saved as `a &amp; b`) |
| 6 | looks | `test.colour` | `commitValue("#1a2b3c")` | |
| 7 | looks | `test.font` | `commitValue("Noto Sans 12")` | |
| 8 | looks | `wallpaper.picture` | `commitValue("/usr/share/backgrounds/riley/sea.png")` | `hyprpaper.conf` = `hyprpaper-08.conf` |
| 9 | looks | `test.folder` | `commitValue("/srv/test/photos")` | |
| 10 | looks | `test.keybinding` | `commitValue("SUPER+SHIFT+K")` | |
| 11 | looks | `wallpaper.screens` | `commitItems([{screen: "DP-1", path: "/usr/share/backgrounds/riley/dp1.png"}])` | 1 item; `hyprpaper.conf` = `hyprpaper-11.conf` |
| 12 | basic | `test.switch` | click | `false`, the key removed; `idle.conf` = `idle-02.conf` |

After step N the values file equals `expected/values-NN.xml` byte for byte, and the action log equals the first
lines of `expected/actions.log` (15 lines after step 12). The home then holds only `.config/riley/settings.xml`,
`.config/hypr/idle.conf` and `.config/hypr/hyprpaper.conf`.

Where the expected files come from: `idle-*.conf` follow Riley's `expected-m7-idle.conf` (its header and lock
listener); `hyprpaper-*.conf` follow Riley's `bin/hypr-theme.py` (the "picked in Riley Settings" head, the
catch-all block, and one block per screen, of which the test has none, once a per-screen picture exists). Steps 1
to 10 of the values files were also reproduced with M1's `riley-config set`, and all five target files with
`riley-config write`; steps 11 and 12 follow the list and removal rules above, by hand.
