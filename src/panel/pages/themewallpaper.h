// Wallpapers per screen: drop a picture on a screen, or use one everywhere (docs/pages-contract.md, "Wallpapers").
// M3 scaffolding: a stub. Every function refuses with "<page title>: not ready".
#pragma once

#include "common.h"

#include <QString>
#include <QStringList>
#include <QVariantList>

namespace riley::panel::pages {

class ThemeWallpaperBackend : public PageBackend {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList screens READ screens NOTIFY changed)

public:
    explicit ThemeWallpaperBackend(QObject* parent = nullptr) : PageBackend(parent) {}

    QVariantList screens() const { return m_screens; }

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE QString screenAt(double x, double y, double width, double height);
    Q_INVOKABLE bool dropPicture(const QString& path, const QString& desc);
    Q_INVOKABLE bool useEverywhere(const QString& path);

Q_SIGNALS:
    void changed();

private:
    QVariantList m_screens;
};

}  // namespace riley::panel::pages
