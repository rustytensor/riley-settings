// Dock: pins by drag and drop, the app list, the dock's screens (docs/pages-contract.md, "Dock").
// A port of Riley's settings/riley_settings/dock.py and the parts of dock_core.py it uses (insert_index, move_pin,
// remove_pin, all_apps, connected_screens). The pins and the screens are the page's options with role "pins" and
// "screens"; the dock's other options are M2 controls (PageOptions.qml). Every change is one Controller::commitStep.
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
    // [{id, name, icon, pinned}] in shown order
    Q_PROPERTY(QVariantList apps READ apps NOTIFY changed)
    // [{desc, name, checked}]: the screens in use (hyprctl monitors -j), top to bottom, left to right
    Q_PROPERTY(QVariantList screens READ screens NOTIFY changed)
    // Width of one pin in the strip, in pixels: the drop rule's icon centres are at pinWidth * (i + 0.5).
    Q_PROPERTY(int pinWidth READ pinWidth CONSTANT)

public:
    explicit DockBackend(QObject* parent = nullptr);

    bool ready() const override { return true; }

    QStringList pins() const { return m_pins; }
    QStringList appIds() const { return m_appIds; }
    QVariantList apps() const { return m_apps; }
    QVariantList screens() const { return m_screens; }
    static constexpr int pinWidth() { return 48; }

    // Reads the pins and the screen choices from the values, the apps from their .desktop files and the screens in
    // use from hyprctl. Writes nothing.
    Q_INVOKABLE bool refresh();
    // A drop on the strip at x (strip coordinates) of "pin:<id>" or "app:<id>".
    Q_INVOKABLE bool dropOnDock(const QString& value, double x);
    // A drop on Remove: "pin:<id>" takes the pin off; anything else is refused.
    Q_INVOKABLE bool dropOnRemove(const QString& value);
    // The screens option (role "screens") = these descriptions.
    Q_INVOKABLE bool setScreens(const QStringList& descs);
    // A screen's check box: the choices for screens not in use are kept, the ones in use follow the boxes.
    Q_INVOKABLE bool setScreenChecked(const QString& desc, bool checked);
    // Where a drop at x would land (Riley's insert_index), for the drop marker.
    Q_INVOKABLE int insertIndex(double x) const;

Q_SIGNALS:
    void changed();

private:
    OptionItem* option(const QString& role) const;
    QStringList listValue(const QString& role) const;
    bool saveList(const QString& role, const QStringList& values);
    void readValues();
    void readApps();
    void readScreens();
    void rebuildApps();
    void rebuildScreens();
    void watchOptions();

    QStringList m_pins;
    QStringList m_filters;   // the screens option as saved
    QStringList m_appIds;
    QVariantList m_apps;
    QVariantList m_screens;
    struct App {
        QString id, name, icon;
    };
    QList<App> m_appList;
    QList<Monitor> m_monitors;
    QList<QMetaObject::Connection> m_watch;
};

}  // namespace riley::panel::pages
