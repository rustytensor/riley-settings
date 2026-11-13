// Desktop: the folder boxes of riley-desktop, a port of Riley's layout.py and desktop_page.py
// (docs/pages-contract.md, "Desktop"). The layout rules are shared with the riley-desktop writer
// (config/writers/desktop-layout.h); the options are found by role (show, boxes, icon-size), never by key.
#pragma once

#include "common.h"
#include "config/writers/desktop-layout.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include <functional>

namespace riley::panel::pages {

class DesktopBackend : public PageBackend {
    Q_OBJECT
    QML_ELEMENT
    // The boxes the desktop shows now, in desktop.json order: "<title> <x> <y> <w> <h>".
    Q_PROPERTY(QStringList shown READ shown NOTIFY changed)
    // Every box of desktop.json, for the page's list: {folder, title, hidden, auto, missing, onScreen}.
    Q_PROPERTY(QVariantList boxes READ boxes NOTIFY changed)

public:
    explicit DesktopBackend(QObject* parent = nullptr) : PageBackend(parent) {}

    bool ready() const override { return true; }
    QStringList shown() const { return m_shown; }
    QVariantList boxes() const { return m_boxes; }

    // What showing the page does: the screens from Hyprland, desktop.json and ~/Desktop read again. Writes nothing.
    Q_INVOKABLE bool reload();
    Q_INVOKABLE bool addFolder(const QString& path);
    Q_INVOKABLE bool setHidden(const QString& folder, bool hidden);
    Q_INVOKABLE bool removeBox(const QString& folder);
    // A file:// URL from the folder dialog as a local path.
    Q_INVOKABLE QString localPath(const QUrl& url) const;

Q_SIGNALS:
    void changed();

private:
    QString jsonPath() const;
    QString desktopDir() const;
    bool readScreens();
    void recompute();
    // One user step: desktop.json read fresh and resolved, `change` made on it (false: refused, nothing written),
    // then the boxes option (role "boxes") saved as one change, which writes desktop.json through the riley-desktop
    // writer.
    bool step(const std::function<bool(QJsonObject*)>& change, const QString& refusal);
    void watchOptions();

    config::desktop::Screens m_screens;
    QStringList m_shown;
    QVariantList m_boxes;
    bool m_watching = false;
};

}  // namespace riley::panel::pages
