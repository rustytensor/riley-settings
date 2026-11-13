// Dock: pins by drag and drop, the app list, the dock's screens (docs/pages-contract.md, "Dock").
// M3 scaffolding: a stub. Every function refuses with "<page title>: not ready".
#pragma once

#include "common.h"

#include <QString>
#include <QStringList>
#include <QVariantList>

namespace riley::panel::pages {

class DockBackend : public PageBackend {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList pins READ pins NOTIFY changed)
    Q_PROPERTY(QStringList appIds READ appIds NOTIFY changed)
    Q_PROPERTY(QVariantList apps READ apps NOTIFY changed)
    Q_PROPERTY(QVariantList screens READ screens NOTIFY changed)

public:
    explicit DockBackend(QObject* parent = nullptr) : PageBackend(parent) {}

    QStringList pins() const { return m_pins; }
    QStringList appIds() const { return m_appIds; }
    QVariantList apps() const { return m_apps; }
    QVariantList screens() const { return m_screens; }

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool dropOnDock(const QString& value, double x);
    Q_INVOKABLE bool dropOnRemove(const QString& value);
    Q_INVOKABLE bool setScreens(const QStringList& descs);

Q_SIGNALS:
    void changed();

private:
    QStringList m_pins;
    QStringList m_appIds;
    QVariantList m_apps;
    QVariantList m_screens;
};

}  // namespace riley::panel::pages
