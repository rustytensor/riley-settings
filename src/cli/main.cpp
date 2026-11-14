// riley-config: the configuration system from the command line. Every file is named on the command line; the tool
// never reads or writes ~/.config on its own. The schemas come from $RILEY_SETTINGS_SHARE/schema/ (default: the
// installed share directory).
//
//   riley-config validate <description.xml>
//   riley-config check-values <description.xml> <values.xml>
//   riley-config set <values.xml> <key> <value>
//   riley-config write <target> --description D --values V --out FILE [--in EXISTING]
//   riley-config edid <edid-file>      the most bits per colour the screen takes (0: the EDID does not say)
//
// Exit 0 on success, 1 with a one-line reason on stderr otherwise, 2 for a usage error.

#include "config/config.h"
#include "config/description.h"
#include "config/edid.h"
#include "config/values.h"
#include "config/writer.h"

#include <QFile>
#include <QString>
#include <QStringList>

#include <cstdio>
#include <cstdlib>
#include <optional>

#ifndef RILEY_SETTINGS_SHARE_DIR
#error "RILEY_SETTINGS_SHARE_DIR must be defined by the build"
#endif

using namespace riley::config;

namespace {

const char kUsage[] =
    "usage: riley-config validate <description.xml>\n"
    "       riley-config check-values <description.xml> <values.xml>\n"
    "       riley-config set <values.xml> <key> <value>\n"
    "       riley-config write <target> --description D --values V --out FILE [--in EXISTING]\n"
    "       riley-config edid <edid-file>\n";

int usage()
{
    std::fputs(kUsage, stderr);
    return 2;
}

int fail(const QString& reason)
{
    QString line = reason;
    line.replace(QLatin1Char('\n'), QLatin1Char(' '));
    std::fprintf(stderr, "riley-config: %s\n", line.toUtf8().constData());
    return 1;
}

QString shareDir()
{
    const char* env = std::getenv("RILEY_SETTINGS_SHARE");
    if (env && *env)
        return QString::fromLocal8Bit(env);
    return QStringLiteral(RILEY_SETTINGS_SHARE_DIR);
}

QString schemaDir()
{
    return shareDir() + QStringLiteral("/schema");
}

int validate(const QStringList& args)
{
    if (args.size() != 1)
        return usage();
    QString error;
    if (!Description::load(args[0], schemaDir(), &error))
        return fail(error);
    return 0;
}

int checkValues(const QStringList& args)
{
    if (args.size() != 2)
        return usage();
    QString error;
    const auto description = Description::load(args[0], schemaDir(), &error);
    if (!description)
        return fail(error);
    const auto values = Values::load(args[1], schemaDir(), &error);
    if (!values)
        return fail(error);
    QStringList unknown;
    const bool ok = values->check(*description, &unknown, &error);
    for (const QString& key : unknown)   // kept and reported, never an error
        std::fprintf(stderr, "unknown key: %s\n", key.toUtf8().constData());
    return ok ? 0 : fail(error);
}

int set(const QStringList& args)
{
    if (args.size() != 3)
        return usage();
    // The description that says what the key may hold is the installed (or $RILEY_SETTINGS_SHARE) one.
    QString error;
    const auto description = Description::load(shareDir() + QStringLiteral("/settings.xml"), schemaDir(), &error);
    if (!description)
        return fail(error);
    if (!setValue(args[0], *description, schemaDir(), args[1], args[2], &error))
        return fail(error);
    return 0;
}

int write(const QStringList& args)
{
    if (args.isEmpty())
        return usage();
    const QString target = args[0];
    std::optional<QString> descPath, valuesPath, outPath, inPath;
    for (qsizetype i = 1; i < args.size(); i += 2) {
        if (i + 1 >= args.size())
            return usage();
        const QString& flag = args[i];
        const QString& value = args[i + 1];
        if (flag == QLatin1String("--description")) descPath = value;
        else if (flag == QLatin1String("--values")) valuesPath = value;
        else if (flag == QLatin1String("--out")) outPath = value;
        else if (flag == QLatin1String("--in")) inPath = value;
        else return usage();
    }
    if (!descPath || !valuesPath || !outPath)
        return usage();

    const Writer* writer = findWriter(target);
    if (!writer)
        return fail(QStringLiteral("no writer for target ") + target + QStringLiteral(" (have: ")
                    + writerTargets().join(QStringLiteral(", ")) + QLatin1Char(')'));

    QString error;
    const auto description = Description::load(*descPath, schemaDir(), &error);
    if (!description)
        return fail(error);
    const auto values = Values::load(*valuesPath, schemaDir(), &error);
    if (!values)
        return fail(error);
    QStringList unknown;
    if (!values->check(*description, &unknown, &error))
        return fail(error);
    for (const QString& key : unknown)
        std::fprintf(stderr, "unknown key: %s\n", key.toUtf8().constData());

    QByteArray existing;
    if (inPath) {
        QFile f(*inPath);
        if (!f.open(QIODevice::ReadOnly))
            return fail(*inPath + QStringLiteral(": ") + f.errorString());
        existing = f.readAll();
    }
    const Config config(*description, *values);
    if (!writer->write(config, *outPath, inPath ? &existing : nullptr, &error))
        return fail(error);
    return 0;
}

int edid(const QStringList& args)
{
    if (args.size() != 1)
        return usage();
    QFile f(args[0]);
    if (!f.open(QIODevice::ReadOnly))
        return fail(args[0] + QStringLiteral(": ") + f.errorString());
    std::printf("%d\n", edidBitsPerColour(f.readAll()));
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
        return usage();
    QStringList args;
    for (int i = 2; i < argc; ++i)
        args.append(QString::fromLocal8Bit(argv[i]));
    const QString command = QString::fromLocal8Bit(argv[1]);
    if (command == QLatin1String("validate")) return validate(args);
    if (command == QLatin1String("check-values")) return checkValues(args);
    if (command == QLatin1String("set")) return set(args);
    if (command == QLatin1String("write")) return write(args);
    if (command == QLatin1String("edid")) return edid(args);
    if (command == QLatin1String("-h") || command == QLatin1String("--help")) {
        std::fputs(kUsage, stdout);
        return 0;
    }
    return usage();
}
