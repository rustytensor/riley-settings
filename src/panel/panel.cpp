#include "panel.h"

#include "models.h"

#include <QCoreApplication>
#include <QDir>
#include <QQuickWindow>
#include <QtQml/qqmlextensionplugin.h>

// The QML module is a static plugin: bring it in here, so a program that links the library needs nothing else.
Q_IMPORT_QML_PLUGIN(Riley_SettingsPlugin)

namespace riley::panel {
namespace {

QString envOr(const char* name, const QString& fallback)
{
    const QString v = qEnvironmentVariable(name);
    return v.isEmpty() ? fallback : v;
}

}  // namespace

bool load(QQmlApplicationEngine& engine, const LoadOptions& options, QString* error)
{
    // The share directory: $RILEY_SETTINGS_SHARE, else ../share/riley-settings next to the program (an installed
    // prefix carries it along); no compiled-in path.
    const QString share = envOr("RILEY_SETTINGS_SHARE",
                                QDir::cleanPath(QCoreApplication::applicationDirPath()
                                                + QStringLiteral("/../share/riley-settings")));
    const QString configHome = envOr("XDG_CONFIG_HOME", QDir::homePath() + QStringLiteral("/.config"));

    Paths paths;
    paths.description = options.descriptionPath.isEmpty() ? share + QStringLiteral("/settings.xml")
                                                          : options.descriptionPath;
    paths.schemaDir = share + QStringLiteral("/schema");
    paths.values = options.valuesPath.isEmpty() ? configHome + QStringLiteral("/riley/settings.xml")
                                                : options.valuesPath;
    paths.configHome = configHome;
    paths.actionLog = qEnvironmentVariable("RILEY_SETTINGS_ACTION_LOG");
    paths.home = QDir::homePath();
    paths.cacheHome = envOr("XDG_CACHE_HOME", paths.home + QStringLiteral("/.cache"));
    paths.dataHome = envOr("XDG_DATA_HOME", paths.home + QStringLiteral("/.local/share"));
    paths.dataDirs = envOr("XDG_DATA_DIRS", QStringLiteral("/usr/local/share:/usr/share"))
                         .split(QLatin1Char(':'), Qt::SkipEmptyParts);

    QString why;
    Controller* controller = Controller::create(paths, options.applyActions, &why);
    if (!controller) {
        *error = why;
        return false;
    }
    controller->setParent(&engine);

    QList<QQmlError> warnings;
    const auto onWarnings = QObject::connect(&engine, &QQmlEngine::warnings,
                                             [&warnings](const QList<QQmlError>& w) { warnings += w; });
    engine.setInitialProperties({{QStringLiteral("panel"), QVariant::fromValue(static_cast<QObject*>(controller))}});
    engine.loadFromModule("Riley.Settings", "Main");
    QObject::disconnect(onWarnings);

    if (engine.rootObjects().isEmpty()) {
        *error = warnings.isEmpty() ? QStringLiteral("Main.qml did not load") : warnings.first().toString();
        return false;
    }
    if (!qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst())) {
        *error = QStringLiteral("Main.qml's root object is not a window");
        return false;
    }
    return true;
}

}  // namespace riley::panel
