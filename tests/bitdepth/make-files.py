#!/usr/bin/env python3
"""Makes the bit-depth gate's EDID fixtures and the expected files that Riley's own code can make.

    python3 -I tests/bitdepth/make-files.py RILEY_REPO

Run once, by hand, when the gate was written; the gate never runs it (tests/run-bitdepth.sh pins every file it made
by sha256).

- edid/*.bin: made-up EDIDs (no real screen's serial), one per case, each with the bits per colour worked out by
  hand below. The gate also asks edid-decode (an independent decoder) what each one says.
- expected/*.settings.lua: core.render(model) of Riley's settings/riley_settings/core.py (riley 0027087 writes
  bitdepth = N last, only when N is not 8).
- expected/*.model.json: Riley's model format, json.dumps(model, indent=2) + "\\n" as core.py writes settings.json,
  with the M4 contract's number rule (1.0 is written 1).
"""
import importlib.util
import json
import sys
from pathlib import Path

here = Path(__file__).resolve().parent
riley = Path(sys.argv[1])

# ---- EDIDs ---------------------------------------------------------------------------------------------------------
def base(version_minor, input_byte, extensions):
    b = bytearray(128)
    b[0:8] = bytes([0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00])
    b[8:10] = bytes([0x49, 0x99])          # manufacturer "RLY": R=18, L=12, Y=25 -> 0b0_10010_01100_11001
    b[10:12] = bytes([0x01, 0x00])         # product code 1
    b[16], b[17] = 1, 30                   # week 1 of 2020
    b[18], b[19] = 1, version_minor        # EDID 1.<minor>
    b[20] = input_byte                     # video input definition
    b[21], b[22] = 60, 34                  # 60 x 34 cm
    b[126] = extensions
    b[127] = (-sum(b[:127])) & 0xFF
    return bytes(b)

def cta_hdmi(flags):
    """A CTA-861 block holding one HDMI vendor block (OUI 00-0C-03, little-endian 03 0C 00), byte 6 = flags."""
    e = bytearray(128)
    vsdb = bytes([(3 << 5) | 6, 0x03, 0x0C, 0x00, 0x10, 0x00, flags])
    e[0], e[1], e[2], e[3] = 0x02, 0x03, 4 + len(vsdb), 0x00
    e[4:4 + len(vsdb)] = vsdb
    e[127] = (-sum(e[:127])) & 0xFF
    return bytes(e)

EDIDS = {
    # name: (bytes, bits per colour worked out by hand)
    "dp-10": (base(4, 0x80 | (3 << 4) | 5, 0), 10),            # 1.4 digital, bits 6-4 = 3: 10 bits, DisplayPort
    "dp-8": (base(4, 0x80 | (2 << 4) | 5, 0), 8),              # 1.4 digital, bits 6-4 = 2: 8 bits, DisplayPort
    "hdmi-dc30": (base(3, 0x80, 1) + cta_hdmi(0x10 | 0x08), 10),  # 1.3 (no depth in byte 20), DC_30bit + DC_Y444
    "hdmi-8": (base(3, 0x80, 1) + cta_hdmi(0x00), 8),          # 1.3, HDMI block with no deep colour: 8
    "analog": (base(3, 0x0E, 0), 0),                           # 1.3 analog: says nothing
}
for name, (data, _) in EDIDS.items():
    (here / "edid" / (name + ".bin")).write_bytes(data)

# ---- Riley's output ------------------------------------------------------------------------------------------------
spec = importlib.util.spec_from_file_location("riley_core", riley / "settings/riley_settings/core.py")
core = importlib.util.module_from_spec(spec)
spec.loader.exec_module(core)

def screen(desc, mode, x, y, scale, transform, enabled=True, bitdepth=None):
    m = {"desc": desc, "enabled": enabled, "mode": mode, "x": x, "y": y, "scale": scale, "transform": transform}
    if bitdepth is not None:
        m["bitdepth"] = bitdepth
    return m

DELL = "Dell Inc. DELL U2515H YKFWP4CC09VL"
SAM1 = "Samsung Electric Company S34C65xU HCNYB00237"
SAM2 = "Samsung Electric Company S34C65xU HCNYB00019"
ARZ1 = "GWD ARZOPA 0x37330206"
ARZ2 = "GWD ARZOPA 0000001504150"

def model(sam2_bits=None, arz2_scale=1.25):
    """Riley's fixture-m1 screens; SAM1 and ARZ1 at 10 bits (as in b1.values.xml and as running in the fake hyprctl);
    SAM2 at sam2_bits, the switched-off ARZ2 at arz2_scale."""
    return {"version": 1, "monitors": [
        screen(DELL, "2560x1440@59.95", 0, 0, 1, 3),
        screen(SAM1, "3440x1440@59.97", 1440, 407, 1, 0, bitdepth=10),
        screen(SAM2, "3440x1440@59.97", 4880, 407, 1, 0, bitdepth=sam2_bits),
        screen(ARZ1, "2560x1600@60.00", 1440, 1847, 1.5, 0, bitdepth=10),
        screen(ARZ2, "2560x1600@60.00", 4495, 1847, arz2_scale, 0, enabled=False),
    ]}

# b1-edit: b1 after the page sets the switched-off ARZOPA's scale to 1.5 (settings.lua shows it only as disabled).
for name, m in {"b1": model(), "b1-edit": model(arz2_scale=1.5), "b3": model(sam2_bits=10)}.items():
    if name != "b1-edit":   # its settings.lua is b1's: a switched-off screen is written only as disabled
        (here / "expected" / (name + ".settings.lua")).write_text(core.render(m))
    (here / "expected" / (name + ".model.json")).write_text(json.dumps(m, indent=2) + "\n")
print("made", len(EDIDS), "EDIDs and 5 expected files")
