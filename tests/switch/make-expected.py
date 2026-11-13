#!/usr/bin/env python3
"""Makes the M4 gate's expected files that Riley's own code can make, so they do not come from the code under test.

    python3 -I tests/switch/make-expected.py RILEY_REPO OUT_DIR

Run once, by hand, when the gate was written; the gate never runs it (tests/run-switch.sh pins the files it made by
sha256). The values are the ones worked out by hand in docs/switch-contract.md ("Import", "riley-model", "Layout
picker"); this script only lets Riley's code format them:

- model-*.json and layout-model-*.json: Riley's model format, json.dumps(model, indent=2) + "\\n" exactly as
  settings/riley_settings/core.py writes ~/.config/hypr/settings.json, with the contract's number rule (a number with
  no fraction is written without one, as C's %g does: 1.0 becomes 1).
- layout-settings-*.lua: core.render(model) of Riley's settings/riley_settings/core.py.

It also checks the hand-worked models against Riley's fixtures: model-m3.json and model-m4.json must hold the same
data as fixture-m3.json and fixture-m4.json (1 == 1.0), and model-m1.json fixture-m1.json's screens plus the power
times that core.parse_idle reads from expected-m7-idle.conf.
"""
import importlib.util
import json
import sys
from pathlib import Path

riley, out = Path(sys.argv[1]), Path(sys.argv[2])
spec = importlib.util.spec_from_file_location("riley_core", riley / "settings/riley_settings/core.py")
core = importlib.util.module_from_spec(spec)
spec.loader.exec_module(core)


def g(v):
    """The contract's number rule: %g. Booleans and strings stay as they are."""
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        return v
    return int(v) if float(v).is_integer() else v


def norm(x):
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm(v) for v in x]
    return g(x)


def dump(model, name):
    text = json.dumps(norm(model), indent=2) + "\n"
    (out / name).write_text(text)
    return text


fixtures = Path(__file__).resolve().parent / "fixtures"
f1, f3, f4 = (json.loads((fixtures / f"fixture-{m}.json").read_text()) for m in ("m1", "m3", "m4"))
MON = f1["monitors"]   # the same five screens in all three fixtures
assert f3["monitors"] == MON and f4["monitors"] == MON

INPUT_DEFAULTS = {"kb_layout": "us", "kb_variant": "", "kb_options": "", "repeat_rate": 25, "repeat_delay": 600,
                  "sensitivity": 0, "accel_profile": "", "natural_scroll": False, "follow_mouse": 1}

# ---- import cases: the riley-model writer's output from the hand-worked values files -----------------------------
# Riley's expected-m7-idle.conf, byte for byte the copy this repo pins in tests/expected/ (Riley deleted its own in 7f0c901).
idle = core.parse_idle((Path(__file__).resolve().parents[2] / "tests/expected/expected-m7-idle.conf").read_text())
m1 = {"version": 1, "monitors": MON,
      "power": {"screensaver": 0, "lock": 600, "screens_off": 900, "sleep": 3600}}
assert {k: m1["power"][k] for k in ("lock", "screens_off", "sleep")} == idle and "screensaver" not in idle
dump(m1, "model-m1.json")

m3 = {"version": 1, "monitors": MON,
      "appearance": {"gaps_in": 8, "gaps_out": 20, "border_size": 3, "rounding": 14, "active_opacity": 1,
                     "inactive_opacity": 0.9, "blur": True, "blur_size": 8, "blur_passes": 3, "shadow": True,
                     "animations": False},
      "input": {"kb_layout": "us,ro", "kb_variant": ",std", "kb_options": "grp:alt_shift_toggle", "repeat_rate": 30,
                "repeat_delay": 400, "sensitivity": -0.2, "accel_profile": "flat", "natural_scroll": False,
                "follow_mouse": 1}}
assert norm(m3) == norm(f3) and list(m3["appearance"]) == list(f3["appearance"]) and list(m3["input"]) == list(f3["input"])
dump(m3, "model-m3.json")

m4 = {"version": 1, "monitors": MON, "startup": f4["startup"]}
assert norm(m4) == norm(f4)
dump(m4, "model-m4.json")

# ---- layout picker: steps 1 and 2 of the layout group (seed: tests/switch/seeds/layout-values.xml) -------------------
step1 = {"kb_layout": "us,ro", "kb_variant": ",std", "kb_options": "caps:escape,grp:alt_shift_toggle"}
step2 = {"kb_options": "caps:escape"}
for n, keys in ((1, step1), (2, step2)):
    # settings.lua: core.render with only the keys the values file sets (hyprland-lua writes only those).
    (out / f"layout-settings-{n}.lua").write_text(core.render({"version": 1, "monitors": MON, "input": dict(keys)}))
    # settings.json: the whole input section, unset keys at their defaults (riley-model's rule).
    dump({"version": 1, "monitors": MON, "input": {**INPUT_DEFAULTS, **keys}}, f"layout-model-{n}.json")
print("written to", out)
