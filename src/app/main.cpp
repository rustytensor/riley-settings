// riley-settings-qt: the settings panel. A thin main over the riley-settings-panel library.
//
//   riley-settings-qt [--apply-actions] [--description FILE] [--values FILE]
//
// Without --apply-actions, the actions a change would run are only recorded ($RILEY_SETTINGS_ACTION_LOG) or
// printed on stderr; M4's launcher passes it.
#include "panel/panel.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include <cstdio>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("riley-settings"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Riley Settings"));
    QGuiApplication::setDesktopFileName(QStringLiteral("riley-settings"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Riley's settings panel"));
    parser.addHelpOption();
    const QCommandLineOption apply(QStringLiteral("apply-actions"),
                                   QStringLiteral("Run each change's actions (reloads); otherwise they are only recorded."));
    const QCommandLineOption description(QStringLiteral("description"),
                                         QStringLiteral("The description to draw (default: the installed settings.xml)."),
                                         QStringLiteral("file"));
    const QCommandLineOption values(QStringLiteral("values"),
                                    QStringLiteral("The values file (default: ~/.config/riley/settings.xml)."),
                                    QStringLiteral("file"));
    parser.addOptions({apply, description, values});
    parser.process(app);

    // Fusion follows the system's palette, light or dark; an explicit QT_QUICK_CONTROLS_STYLE wins.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        QQuickStyle::setStyle(QStringLiteral("Fusion"));

    riley::panel::LoadOptions options;
    options.applyActions = parser.isSet(apply);
    options.descriptionPath = parser.value(description);
    options.valuesPath = parser.value(values);

    QQmlApplicationEngine engine;
    QString error;
    if (!riley::panel::load(engine, options, &error)) {
        std::fprintf(stderr, "riley-settings-qt: %s\n", error.toUtf8().constData());
        return 1;
    }
    return app.exec();
}
