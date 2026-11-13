// Desktop: the folder boxes of riley-desktop, a port of Riley's layout.py (docs/pages-contract.md, "Desktop").
// M3 scaffolding: a stub. Every function refuses with "<page title>: not ready".
#pragma once

#include "common.h"

#include <QString>
#include <QStringList>
#include <QVariantList>

namespace riley::panel::pages {

class DesktopBackend : public PageBackend {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList shown READ shown NOTIFY changed)

public:
    explicit DesktopBackend(QObject* parent = nullptr) : PageBackend(parent) {}

    QStringList shown() const { return m_shown; }

    Q_INVOKABLE bool reload();
    Q_INVOKABLE bool addFolder(const QString& path);
    Q_INVOKABLE bool setHidden(const QString& folder, bool hidden);
    Q_INVOKABLE bool removeBox(const QString& folder);

Q_SIGNALS:
    void changed();

private:
    QStringList m_shown;
};

}  // namespace riley::panel::pages
