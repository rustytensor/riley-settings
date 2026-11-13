// riley-settings-qt: the settings panel. A thin main over the riley-settings-panel library.
//
//   riley-settings-qt [--apply-actions] [--description FILE] [--values FILE]
//   riley-settings-qt --import [--values FILE] [--description FILE]
//   riley-settings-qt --check [--out DIR] [--values FILE] [--description FILE]
//
// Without --apply-actions, the actions a change would run are only recorded ($RILEY_SETTINGS_ACTION_LOG) or
// printed on stderr; M4's launcher passes it. --import and --check open no window and need no display
// (docs/switch-contract.md). The share directory is $RILEY_SETTINGS_SHARE, else <this binary's dir>/../share/riley-settings.
#include "config/check.h"
#include "config/import.h"
#include "panel/panel.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include <cstdio>
#include <cstring>

namespace {

QString shareDir()
{
    const QString env = qEnvironmentVariable("RILEY_SETTINGS_SHARE");
    if (!env.isEmpty())
        return env;
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../share/riley-settings"));
}

struct Options {
    QCommandLineOption apply{QStringLiteral("apply-actions"),
                             QStringLiteral("Run each change's actions (reloads); otherwise they are only recorded.")};
    QCommandLineOption description{QStringLiteral("description"),
                                   QStringLiteral("The description to draw (default: the installed settings.xml)."),
                                   QStringLiteral("file")};
    QCommandLineOption values{QStringLiteral("values"),
                              QStringLiteral("The values file (default: ~/.config/riley/settings.xml)."),
                              QStringLiteral("file")};
    QCommandLineOption importFlag{QStringLiteral("import"),
                              QStringLiteral("Make the values file from what Riley's GTK panel saved, and exit.")};
    QCommandLineOption checkFlag{QStringLiteral("check"),
                             QStringLiteral("Compare what every writer would write with the files there, and exit.")};
    QCommandLineOption out{QStringLiteral("out"),
                           QStringLiteral("With --check: keep the files the writers would leave, under DIR."),
                           QStringLiteral("dir")};

    void addTo(QCommandLineParser* parser) const
    {
        parser->setApplicationDescription(QStringLiteral("Riley's settings panel"));
        parser->addHelpOption();
        parser->addOptions({apply, description, values, importFlag, checkFlag, out});
    }
};

// --import or --check: a QCoreApplication only (a GUI application would need a display).
bool wantsNoWindow(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--import") || !std::strcmp(argv[i], "--check") || !std::strcmp(argv[i], "-import")
            || !std::strcmp(argv[i], "-check"))
            return true;
    }
    return false;
}

int runWithoutWindow(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("riley-settings"));
    QCommandLineParser parser;
    const Options o;
    o.addTo(&parser);
    parser.process(app);
    if (parser.isSet(o.importFlag) == parser.isSet(o.checkFlag)) {
        std::fputs("riley-settings-qt: give --import or --check, not both\n", stderr);
        return 2;
    }
    const QString share = shareDir();
    const QString schemaDir = share + QStringLiteral("/schema");
    const QString descriptionPath = parser.isSet(o.description) ? parser.value(o.description)
                                                                : share + QStringLiteral("/settings.xml");
    const riley::config::HomePaths paths = riley::config::HomePaths::fromEnvironment();
    const QString valuesPath = parser.isSet(o.values) ? parser.value(o.values) : paths.defaultValues();
    if (parser.isSet(o.importFlag))
        return riley::config::runImport(descriptionPath, schemaDir, valuesPath, paths);
    return riley::config::runCheck(descriptionPath, schemaDir, valuesPath, paths, parser.value(o.out));
}

}  // namespace

int main(int argc, char** argv)
{
    if (wantsNoWindow(argc, argv))
        return runWithoutWindow(argc, argv);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("riley-settings"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Riley Settings"));
    QGuiApplication::setDesktopFileName(QStringLiteral("riley-settings"));

    QCommandLineParser parser;
    const Options o;
    o.addTo(&parser);
    parser.process(app);

    // Fusion follows the system's palette, light or dark; an explicit QT_QUICK_CONTROLS_STYLE wins.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        QQuickStyle::setStyle(QStringLiteral("Fusion"));

    riley::panel::LoadOptions options;
    options.applyActions = parser.isSet(o.apply);
    options.descriptionPath = parser.value(o.description);
    options.valuesPath = parser.value(o.values);

    // The first start: no values file yet, but Riley's GTK panel saved a model: import it first (the same import as
    // --import), then open.
    const riley::config::HomePaths paths = riley::config::HomePaths::fromEnvironment();
    const QString valuesPath = options.valuesPath.isEmpty() ? paths.defaultValues() : options.valuesPath;
    if (!QFileInfo::exists(valuesPath) && QFileInfo(paths.oldModel()).isFile()) {
        const QString share = shareDir();
        riley::config::runImport(options.descriptionPath.isEmpty() ? share + QStringLiteral("/settings.xml")
                                                                   : options.descriptionPath,
                                 share + QStringLiteral("/schema"), valuesPath, paths);
    }

    QQmlApplicationEngine engine;
    QString error;
    if (!riley::panel::load(engine, options, &error)) {
        std::fprintf(stderr, "riley-settings-qt: %s\n", error.toUtf8().constData());
        return 1;
    }
    return app.exec();
}
