// The import of what Riley's GTK panel saved (docs/switch-contract.md, "Import"): its model
// (~/.config/hypr/settings.json) and the other files its pages read, turned into a values file. It reads only; it
// writes the values file and nothing else, and runs no command.
#pragma once

#include "description.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace riley::config {

// $HOME, $XDG_CONFIG_HOME and $XDG_CACHE_HOME (each with its default), as Riley's pages and the writers use them.
struct HomePaths {
    QString home;
    QString configHome;
    QString cacheHome;

    static HomePaths fromEnvironment();
    QString oldModel() const { return home + QStringLiteral("/.config/hypr/settings.json"); }   // Riley: Path.home()
    QString defaultValues() const { return configHome + QStringLiteral("/riley/settings.xml"); }
};

struct ImportResult {
    QByteArray text;      // the values file
    int keys = 0;         // how many keys it sets
    QStringList skipped;  // one line per value left out: "skipped <file>: <key> <value>: <reason>"
};

// The values file the old files make: the M2 header, then each key in description order through the M1 set path.
// False with a one-line *error when the result is not a valid values file.
bool importOldFiles(const Description& description, const QString& schemaDir, const HomePaths& paths,
                    ImportResult* result, QString* error);

// --import and the first start: refuses (exit 1) when `valuesPath` exists, else writes it atomically. Prints the
// skipped lines on stderr and "imported N keys into PATH" on stdout. Returns the exit code.
int runImport(const QString& descriptionPath, const QString& schemaDir, const QString& valuesPath,
              const HomePaths& paths);

}  // namespace riley::config
