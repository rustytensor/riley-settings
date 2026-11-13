-- Written by Riley Settings; do not edit by hand (the app rewrites it).
-- Delete this file and settings.json to go back to the defaults in hyprland.lua.

-- Displays
hl.monitor({ output = "desc:Dell Inc. DELL U2515H YKFWP4CC09VL", mode = "2560x1440@59.95", position = "0x0", scale = 1, transform = 3 })
hl.monitor({ output = "desc:Samsung Electric Company S34C65xU HCNYB00237", mode = "3440x1440@59.97", position = "1440x407", scale = 1, transform = 0 })
hl.monitor({ output = "desc:Samsung Electric Company S34C65xU HCNYB00019", mode = "3440x1440@59.97", position = "4880x407", scale = 1, transform = 0 })
hl.monitor({ output = "desc:GWD ARZOPA 0x37330206", mode = "2560x1600@60.00", position = "1440x1847", scale = 1.5, transform = 0 })
hl.monitor({ output = "desc:GWD ARZOPA 0000001504150", disabled = true })
hl.monitor({ output = "", mode = "preferred", position = "auto", scale = "auto" })   -- any other screen

-- Appearance
hl.config({ general = { gaps_in = 8, gaps_out = 20, border_size = 3 }, decoration = { rounding = 14, active_opacity = 1, inactive_opacity = 0.9, blur = { enabled = true, size = 8, passes = 3 }, shadow = { enabled = true } }, animations = { enabled = false } })

-- Keyboard and mouse
hl.config({ input = { kb_layout = "us,ro", kb_variant = ",std", kb_options = "grp:alt_shift_toggle", repeat_rate = 30, repeat_delay = 400, sensitivity = -0.2, accel_profile = "flat", natural_scroll = false, follow_mouse = 1 } })
