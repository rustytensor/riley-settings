#include "check.h"

#include "config.h"
#include "values.h"
#include "writer.h"
#include "writers/desktop-layout.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>

#include <cmath>
#include <cstdio>
#include <optional>

namespace riley::config {
namespace {

void printErr(const QString& line)
{
    std::fprintf(stderr, "riley-settings-qt: %s\n", line.toUtf8().constData());
}

std::optional<QByteArray> readFile(const QString& path)
{
    QFile f(path);
    if (!QFileInfo(path).isFile() || !f.open(QIODevice::ReadOnly))
        return std::nullopt;
    return f.readAll();
}

// Where a real path lives in the temporary copy of the home (and in --out): its path relative to $HOME.
class Mirror {
public:
    Mirror(const QString& home, const QString& root) : m_home(QDir::cleanPath(home)), m_root(root) {}

    QString relative(const QString& path) const
    {
        const QString p = QDir::cleanPath(path);
        if (p.startsWith(m_home + QLatin1Char('/')))
            return p.mid(m_home.size() + 1);
        return QStringLiteral("_outside_home") + p;
    }
    QString of(const QString& path) const { return m_root + QLatin1Char('/') + relative(path); }

private:
    QString m_home;
    QString m_root;
};

bool copyInto(const QString& from, const QString& to)
{
    const auto data = readFile(from);
    if (!data)
        return true;   // no file: nothing to copy
    QDir().mkpath(QFileInfo(to).absolutePath());
    QFile f(to);
    return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(*data) == data->size();
}

// The screens riley-desktop places a new box on: the enabled screens of display.monitors, by their logical size
// (core.logical_size: rotated on an odd transform, divided by the scale, rounded half to even), no command.
QVariantMap screensOf(const Config& config)
{
    desktop::Screens screens;
    QStringList descs;
    if (!config.option(QStringLiteral("display.monitors")))
        return {};
    for (const Item& m : config.list(QStringLiteral("display.monitors"))) {
        if (!m.flag(QStringLiteral("enabled")))
            continue;
        const QStringList wh = m.text(QStringLiteral("mode")).section(QLatin1Char('@'), 0, 0).split(QLatin1Char('x'));
        const double scale = m.number(QStringLiteral("scale"));
        bool okW = false, okH = false;
        int w = wh.size() == 2 ? wh[0].trimmed().toInt(&okW) : 0;
        int h = wh.size() == 2 ? wh[1].trimmed().toInt(&okH) : 0;
        if (!okW || !okH || scale <= 0)
            continue;
        if (int(m.number(QStringLiteral("transform"))) % 2)
            std::swap(w, h);
        const QString desc = m.text(QStringLiteral("desc"));
        screens.sizes.insert(desc, QSize(int(std::nearbyint(w / scale)), int(std::nearbyint(h / scale))));
        descs.append(desc);
    }
    screens.main = desktop::mainOf(descs);
    return screens.toPageData();
}

struct Line {
    QString target;
    QString path;
};

}  // namespace

int runCheck(const QString& descriptionPath, const QString& schemaDir, const QString& valuesPath,
             const HomePaths& paths, const QString& outDir)
{
    QString error;
    const auto description = Description::load(descriptionPath, schemaDir, &error);
    if (!description) {
        printErr(error);
        return 2;
    }
    std::unique_ptr<Values> values = QFileInfo::exists(valuesPath)
        ? Values::load(valuesPath, schemaDir, &error)
        : Values::parse(newValuesFileText(), valuesPath, schemaDir, &error);   // no values file: every default
    if (!values) {
        printErr(error);
        return 2;
    }
    QStringList unknown;
    if (!values->check(*description, &unknown, &error)) {
        printErr(error);
        return 2;
    }
    for (const QString& key : std::as_const(unknown))
        printErr(QStringLiteral("unknown key: ") + key);

    QTemporaryDir tmp;   // under $TMPDIR
    if (!tmp.isValid()) {
        printErr(QStringLiteral("no temporary directory: ") + tmp.errorString());
        return 2;
    }
    const Mirror mirror(paths.home, tmp.path());
    const Config config(*description, *values);

    // The targets in the order they first appear (options in file order, then their target attribute's order),
    // each with the keys of its options that the values file sets.
    QStringList targets;
    QHash<QString, QStringList> setKeys;
    for (const Option& o : description->options()) {
        for (const QString& t : o.targets) {
            if (!findWriter(t))
                continue;
            if (!targets.contains(t))
                targets.append(t);
            if (config.isSet(o.key))
                setKeys[t].append(o.key);
        }
    }

    WriteContext real;
    real.home = paths.home;
    real.configHome = paths.configHome;
    real.cacheHome = paths.cacheHome;
    WriteContext copy = real;   // the same files, in the temporary copy; ~/Desktop is read where it is
    copy.configHome = mirror.of(paths.configHome);
    copy.cacheHome = mirror.of(paths.cacheHome);
    copy.pageData = screensOf(config);

    QList<Line> lines;
    QSet<QString> failed;   // targets whose writer gave an error: they differ
    for (const QString& target : std::as_const(targets)) {
        const Writer* writer = findWriter(target);
        if (writer->writesOwnFiles()) {
            const QStringList files = writer->ownFiles(real);
            for (const QString& f : files) {
                copyInto(f, mirror.of(f));
                lines.append({target, f});
            }
            const QStringList changed = setKeys.value(target);
            if (changed.isEmpty())
                continue;   // none of its keys set: it leaves its files as they are
            copy.changed = changed;
            if (!writer->writeFiles(config, copy, &error)) {
                printErr(target + QStringLiteral(": ") + error);
                failed.insert(target);
            }
            continue;
        }
        const QString name = targetFileName(target);
        if (name.isEmpty())
            continue;
        const QString path = paths.configHome + QLatin1Char('/') + name;
        const QString out = mirror.of(path);
        copyInto(path, out);
        lines.append({target, path});
        QByteArray existing;
        const auto present = readFile(path);
        if (present)
            existing = *present;
        QDir().mkpath(QFileInfo(out).absolutePath());
        if (!writer->write(config, out, writer->ownBlockOnly() && present ? &existing : nullptr, &error)) {
            printErr(target + QStringLiteral(": ") + error);
            failed.insert(target);
        }
    }

    bool allSame = true;
    for (const Line& l : std::as_const(lines)) {
        const auto now = readFile(l.path);
        const auto after = readFile(mirror.of(l.path));
        const bool same = !failed.contains(l.target) && now.has_value() == after.has_value() && (!now || *now == *after);
        allSame = allSame && same;
        std::printf("%s\t%s\t%s\n", l.target.toUtf8().constData(), same ? "same" : "differs",
                    l.path.toUtf8().constData());
        if (!outDir.isEmpty() && after) {
            const QString dest = outDir + QLatin1Char('/') + mirror.relative(l.path);
            QDir().mkpath(QFileInfo(dest).absolutePath());
            QFile f(dest);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(*after) != after->size())
                printErr(dest + QStringLiteral(": ") + f.errorString());
        }
    }
    std::fflush(stdout);
    return allSame ? 0 : 1;
}

}  // namespace riley::config
