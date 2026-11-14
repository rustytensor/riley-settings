// A screen's colour depth, read from its EDID (the bytes in /sys/class/drm/card*-<connector>/edid).
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace riley::config {

// The most bits per colour the screen says it takes, or 0 when the EDID does not say (or is not an EDID):
//   - EDID 1.4, digital input: base block byte 20, bits 6-4 (1 = 6, 2 = 8, 3 = 10, 4 = 12, 5 = 14, 6 = 16 bits);
//   - HDMI: the CTA-861 extension's HDMI vendor block (IEEE OUI 00-0C-03), byte 6: DC_30bit (bit 4) = 10,
//     DC_36bit (bit 5) = 12, DC_48bit (bit 6) = 16. Deep colour there is on top of 8, so the block alone means 8.
// The larger of the two wins.
int edidBitsPerColour(const QByteArray& edid);

// The bit depths Hyprland can drive this screen at: 8 always, and 10 when the EDID gives 10 or more bits per colour
// (Hyprland takes only bitdepth 8 or 10). connector is hyprctl's monitor name (DP-1); the EDID is read from
// <drmDir>/card*-<connector>/edid, drmDir being $RILEY_DRM_DIR or else /sys/class/drm.
QList<int> supportedBitdepths(const QString& connector);

}  // namespace riley::config
