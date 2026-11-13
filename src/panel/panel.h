// The settings panel, drawn from the description (docs/panel-contract.md). Linking riley-settings-panel and calling
// load() is all a program needs to show it.
#pragma once

#include <QQmlApplicationEngine>
#include <QString>

namespace riley::panel {

struct LoadOptions {
    bool applyActions = false;   // true only from --apply-actions (M4's launcher)
    QString descriptionPath;     // empty: $RILEY_SETTINGS_SHARE/settings.xml (default: <program dir>/../share/riley-settings)
    QString valuesPath;          // empty: $XDG_CONFIG_HOME/riley/settings.xml (default: ~/.config/riley/settings.xml)
};

// Reads the description and the values, exposes the models to QML and loads Main.qml into engine.
// Its root object is a QQuickWindow. Returns false with a one-line reason in *error when anything fails.
bool load(QQmlApplicationEngine& engine, const LoadOptions& options, QString* error);

}  // namespace riley::panel
