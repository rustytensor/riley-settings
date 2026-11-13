# Configuration format

Three kinds of file, all XML. The first two are checked against an XML schema (XSD 1.0, in
`share/riley-settings/schema/`) every time they are read.

| File | Where | Schema |
|---|---|---|
| The description: every page, section and option | `share/riley-settings/settings.xml` | `settings.xsd` |
| Your values: only what differs from the defaults | `~/.config/riley/settings.xml` | `values.xsd` |
| The targets: each program's own file | where that program reads it | none (the writer's format) |

## The description

```xml
<description version="1">
  <page id="power" title="Power" order="70">
    <section title="When idle">
      <option key="power.lock_after" label="Lock the screen after" help="Minutes without input before the lock screen."
              type="choice" default="1200" target="hypridle" action="hypridle-restart">
        <choice value="0" label="Never"/>
        <choice value="600" label="10 minutes"/>
        <choice value="1200" label="20 minutes"/>
      </option>
      <option key="appearance.gaps_in" label="Gap between windows" type="slider" default="5" min="0" max="40" step="1"
              target="hyprland-lua" action="hyprland-reload"/>
    </section>
  </page>
  <page id="displays" title="Displays" order="10" widget="displays">
    <option key="display.monitors" label="Screens" type="list" target="hyprland-lua greeter-sway hyprpaper"
            action="hyprland-reload greeter-sync">
      <field name="desc" type="text" label="Screen"/>
      <field name="enabled" type="switch" default="true" label="On"/>
      ...
    </option>
  </page>
</description>
```

- `page`: `id` (unique), `title`, `order` (integer, menu order). A page holds `section`s, each with a `title` and
  its `option`s.
- A page that cannot be drawn from options names its custom QML component: `<page widget="displays" .../>`. It
  holds no sections, only the `option`s it stores (so their keys, types and defaults are still described here).
- `option` attributes:
  - `key`: required, unique in the file, lower case, dot-separated (`page.name`). Never reused for another meaning.
  - `label`, `help`: what the panel shows.
  - `type`: one of `switch`, `choice`, `slider`, `number`, `text`, `colour`, `font`, `file`, `folder`,
    `keybinding`, `list`.
  - `default`: the value when your values file does not set the key. Required except for `list` (default: empty).
  - `min`, `max`, `step`: for `slider` and `number`. The default must lie within `min`..`max` (checked by the
    loader, since XSD 1.0 cannot compare two attributes).
  - `choice` children (`value`, `label`): for `choice`. The default must be one of them.
  - `field` children (`name`, `type`, `label`, optional `default`): for `list`; the columns of one item.
  - `target`: the writers fed by this option, space-separated (table below).
  - `action`: what runs after a change, space-separated, from this closed list: `hyprland-reload`,
    `hypridle-restart`, `hyprpaper-reload`, `dock-reload`, `bar-reload`, `greeter-sync`, `none`. Never a shell line.

What XSD checks: element structure, the type list, required `key`, unique `key` (`xs:unique`), the action list,
integer `order`. What the loader checks after XSD: a default within `min`..`max`, a choice default among its
choices.

## Your values

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Riley settings: only what differs from the defaults. -->
<settings version="1">
  <value key="power.lock_after">300</value>
  <value key="input.kb_layout">us,ro</value>
  <value key="startup.apps">
    <item name="Slack" exec="flatpak run com.slack.Slack" enabled="true"/>
  </value>
</settings>
```

- One `value` per key; its text is the value. A key at its default may be present; it is used like any other.
- A list value is a sequence of `item`s, one attribute per field. A field left out takes the field's default.
  `<value key="startup.apps"/>` is an empty list. A list value replaces the default list whole.
- Value text by type: `switch` is `true` or `false`; `number` and `slider` a decimal number (`1.0`, `-0.2`);
  `choice` one of the choice values; `colour` `#rrggbb` or `#rrggbbaa`; the other types plain text.
- Comments and layout are kept: `riley-config set` changes the text of one value and leaves every other byte alone.
- A key the description does not know is kept and reported, never dropped.
- A new version of the description never rewrites this file.

## Writers

Each writer writes one target, whole, or only its own block of an existing file, always atomically (temporary file,
then rename). Numbers are written as C's `%g` (`1.0` becomes `1`, `-0.2` stays `-0.2`), as today.

| Target | File | Whole or block | Keys it reads |
|---|---|---|---|
| `hyprland-lua` | `~/.config/hypr/settings.lua` | whole | `display.monitors`, `appearance.*`, `input.*`, `startup.apps` |
| `hypridle` | `~/.config/hypr/idle.conf` | whole | `power.screensaver_after`, `power.lock_after`, `power.screens_off_after`, `power.sleep_after` |
| `hyprpaper` | `~/.config/hypr/hyprpaper.conf` | whole | `wallpaper.picture`, `wallpaper.screens`, `display.monitors` |
| `greeter-sway` | the login screen's sway file (installed by `riley-greeter-sync`) | whole | `display.monitors`, `input.kb_layout` |

- `hyprland-lua` writes only the keys your values file sets; everything else falls back to `hyprland.lua`. The
  Displays part is always written. Appearance and Keyboard-and-mouse appear only when one of their keys is set.
  Startup apps appear only when an enabled app has a command. A `text`, `choice` or `font` value is a quoted Lua
  string, except a `choice` whose choice values are all integers, which is a number.
- The others use your value, or the description's default when it is not set.
- `hyprpaper`: one block per enabled screen of `display.monitors`, in list order, each with that screen's picture
  from `wallpaper.screens` or else `wallpaper.picture`. With no per-screen picture, one catch-all block.

## Keys used by the M1 gate

| Key | Type | Notes |
|---|---|---|
| `display.monitors` | list | fields `desc` (text), `enabled` (switch, default true), `mode` (text, `2560x1440@59.95`), `x`, `y` (number), `scale` (number), `transform` (choice 0-3, Hyprland's numbering) |
| `appearance.gaps_in`, `appearance.gaps_out`, `appearance.border_size`, `appearance.rounding` | slider | integers |
| `appearance.active_opacity`, `appearance.inactive_opacity` | slider | 0 to 1 |
| `appearance.blur`, `appearance.shadow`, `appearance.animations` | switch | |
| `appearance.blur_size`, `appearance.blur_passes` | slider | integers |
| `input.kb_layout`, `input.kb_variant`, `input.kb_options` | text | `input.kb_layout` default `us` (the greeter uses it) |
| `input.repeat_rate`, `input.repeat_delay` | slider | integers |
| `input.sensitivity` | slider | -1 to 1 |
| `input.accel_profile` | choice | `adaptive`, `flat`, `custom` |
| `input.natural_scroll` | switch | |
| `input.follow_mouse` | choice | `0`-`3`, written as a number |
| `startup.apps` | list | fields `name`, `exec` (text), `enabled` (switch, default true) |
| `power.screensaver_after`, `power.lock_after`, `power.screens_off_after`, `power.sleep_after` | choice | seconds, `0` = never |
| `wallpaper.picture` | file | the picture on every screen without its own |
| `wallpaper.screens` | list | fields `screen` (a screen's `desc`), `path` (file) |

## The command line

```
riley-config validate <description.xml>
riley-config check-values <description.xml> <values.xml>
riley-config set <values.xml> <key> <value>
riley-config write <target> --description D --values V --out FILE [--in EXISTING]
```

Exit 0 on success, 1 with a one-line reason on stderr otherwise. `check-values` reports each unknown key on stderr
(`unknown key: <key>`) and still exits 0. The schemas are found in `$RILEY_SETTINGS_SHARE/schema/` (default: the
installed share directory). The tool never reads or writes `~/.config` on its own; every file is named on the
command line.
