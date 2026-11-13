// The desktop boxes' layout: a port of Riley's desktop/riley_desktop/layout.py (docs/pages-contract.md, "Desktop").
// Boxes of 440 x 320 (the ~/Desktop box) and 340 x 240, first at (48, 72), down the column with 16 px gaps, a new
// column when a box would reach the bottom 90 px of the main screen. Used by the riley-desktop writer and the
// Desktop page's backend.
//
// `cfg` is desktop.json as an object: "boxes" (a list of {folder, screen, x, y, w, h, auto?, hidden?}), and "show" and
// "icon_size" when set. Other members are kept as they are.
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace riley::config::desktop {

inline constexpr const char* MAIN_SERIAL = "HCNYB00237";   // the main screen (left ultrawide), by serial

// The connected screens: description -> logical size, and the main one's description.
struct Screens {
    QHash<QString, QSize> sizes;
    QString main;
    bool known() const { return !main.isEmpty() && sizes.contains(main); }

    // As the Desktop page hands them to the riley-desktop writer (WriteContext::pageData), and back.
    QVariantMap toPageData() const;
    static Screens fromPageData(const QVariantMap& data);
};

// One box as resolve() shows it: placed and clamped on the screen it is shown on.
struct Shown {
    QString title;
    QString folder;
    QString shownOn;
    int x = 0, y = 0, w = 0, h = 0;
};

// The main screen of a list of descriptions: the one holding MAIN_SERIAL, else the first, else "".
QString mainOf(const QStringList& descs);

// desktop.json at `path`, or {"boxes": []} when it is missing or not a desktop file (Riley's load).
QJsonObject load(const QString& path);
// The file's text as Riley's save writes it (indent 2, a newline at the end).
QByteArray toText(const QJsonObject& cfg);

// Top-left corner for a new w x h box among `taken` (x, y, w, h) on a screen of `screen` size (Riley's free_spot).
QPoint freeSpot(int w, int h, const QList<QRect>& taken, QSize screen);

// Riley's resolve: drops auto boxes whose folder is gone, adds boxes for ~/Desktop and each folder in it that has
// none, and returns the boxes to show now (hidden ones and every box while "show" is false left out). Needs the
// screens; with none known it changes nothing and shows nothing.
QList<Shown> resolve(QJsonObject* cfg, const QString& desktopDir, const Screens& screens);

// Riley's add_folder: a box for any folder in the next free spot of the main screen. False when it has one.
bool addFolder(QJsonObject* cfg, const QString& folder, const Screens& screens);
// Riley's set_hidden. False when no box has that folder.
bool setHidden(QJsonObject* cfg, const QString& folder, bool hidden);
// Riley's remove: a box you added goes; a ~/Desktop folder's box (auto) cannot be removed. False when nothing went.
bool remove(QJsonObject* cfg, const QString& folder);

// What the values file holds of the boxes (desktop.boxes): one item per box you added and one per hidden auto box,
// in desktop.json order.
struct ValueBox {
    QString folder;
    QString screen;
    bool hidden = false;
};
QList<ValueBox> valueBoxes(const QJsonObject& cfg);

// The other way: desktop.json brought in line with the values file's boxes. Boxes you added that the values no
// longer hold go; boxes they hold that desktop.json lacks are added in the next free spot (when the screens are
// known); each box's hidden mark follows its item (an auto box without an item is shown).
void applyValueBoxes(QJsonObject* cfg, const QList<ValueBox>& boxes, const Screens& screens);

// The folder given as a clean absolute path (Riley's str(Path(folder))).
QString cleanFolder(const QString& folder);

}  // namespace riley::config::desktop
