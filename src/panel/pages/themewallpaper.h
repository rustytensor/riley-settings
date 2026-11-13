// Wallpapers per screen: drop a picture on a screen, or use one everywhere (docs/pages-contract.md, "Wallpapers").
// A port of the wallpaper part of Riley's GTK panel's theme_wallpaper.py (riley 2364a47) (ScreenDrop) and core.py's
// layout_rects and screen_at. The pictures are the page's options with role "picture" (every screen) and
// "screen-pictures" (one screen's own); the hyprpaper writer turns them into hyprpaper.conf.
#pragma once

#include "common.h"

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

namespace riley::panel::pages {

class ThemeWallpaperBackend : public PageBackend {
    Q_OBJECT
    QML_ELEMENT
    // [{desc, model, picture}]: the screens in use (hyprctl monitors -j), in its order, each with its picture
    Q_PROPERTY(QVariantList screens READ screens NOTIFY changed)
    // The pictures the values name (every screen's first), for dragging onto a screen
    Q_PROPERTY(QStringList pictures READ pictures NOTIFY changed)

public:
    explicit ThemeWallpaperBackend(QObject* parent = nullptr);

    bool ready() const override { return true; }

    QVariantList screens() const { return m_screens; }
    QStringList pictures() const { return m_pictures; }

    // Reads the screens in use from hyprctl and the pictures from the values. Writes nothing.
    Q_INVOKABLE bool refresh();
    // The screens drawn to fit width x height: [{desc, x, y, w, h, picture}] (Riley's layout_rects, margin 20).
    Q_INVOKABLE QVariantList rects(double width, double height);
    // The desc of the screen under (x, y) of that drawing, or "" (Riley's screen_at: the last one wins).
    Q_INVOKABLE QString screenAt(double x, double y, double width, double height);
    // A picture dropped on a screen: that screen's own picture.
    Q_INVOKABLE bool dropPicture(const QString& path, const QString& desc);
    // A picture clicked: on every screen, and no screen keeps its own.
    Q_INVOKABLE bool useEverywhere(const QString& path);
    // A file:// URL (or a plain path) as a local path, for drops from a file manager.
    Q_INVOKABLE QString localPath(const QVariant& urlOrPath) const;
    // A picture's file:// URL for drawing it, or an empty URL when there is no such file.
    Q_INVOKABLE QUrl pictureUrl(const QString& path) const;

Q_SIGNALS:
    void changed();

private:
    struct Rect {
        QString desc;
        double x, y, w, h;
    };
    OptionItem* option(const QString& role) const;
    bool ensureScreens();
    QList<Rect> layout(double width, double height) const;
    void readValues();
    // The fields of the per-screen list: the screen's and the picture's.
    bool screenFields(QString* screenField, QString* pathField) const;

    bool m_haveScreens = false;
    QList<Monitor> m_monitors;
    QVariantList m_screens;
    QStringList m_pictures;
    QList<QMetaObject::Connection> m_watch;
};

}  // namespace riley::panel::pages
