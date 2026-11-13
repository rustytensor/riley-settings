// The desktop boxes' layout: a port of Riley's desktop/riley_desktop/layout.py (docs/pages-contract.md, "Desktop").
// Boxes of 440 x 320 (the ~/Desktop box) and 340 x 240, first at (48, 72), down the column with 16 px gaps, a new
// column when a box would reach the bottom 90 px of the main screen. Used by the riley-desktop writer and the
// Desktop page's backend.
// M3 scaffolding: not ready yet.
#pragma once

#include <QJsonObject>
#include <QString>

namespace riley::config::desktop {

// desktop.json as Riley's resolve() leaves it: the file `current` with boxes for new folders of `desktopDir` added.
// False with a one-line *error when it cannot be done.
bool resolve(const QJsonObject& current, const QString& desktopDir, QJsonObject* out, QString* error);

}  // namespace riley::config::desktop
