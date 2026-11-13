// Writer for the target "riley-dock": the dock's own files (docs/pages-contract.md, "Dock").
//   - $XDG_CACHE_HOME/nwg-dock-pinned, whole: one app id per line, in dock order (dock.pins; Riley's
//     dock_core.py save_pins);
//   - $XDG_CONFIG_HOME/nwg-dock-hyprland/riley.conf, one KEY=value line per changed option, by Riley's
//     dock_core.py _set_config rule; every other byte kept, replaced by a rename.
// Only the files of the keys that changed are written; with no changed keys, every file is.
#include "writers.h"

#include "../atomic_file.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cmath>

namespace riley::config::writers {
namespace {

const QString PINS = QStringLiteral("dock.pins");

// One riley.conf line per option: the key in riley.conf, and how the value is written.
struct ConfLine {
    QString option;
    QString confKey;
    enum Kind { Integer, Flag, Text, Descs } kind;
};

const QList<ConfLine>& confLines()
{
    static const QList<ConfLine> lines{
        {QStringLiteral("dock.icon_size"), QStringLiteral("ICON_SIZE"), ConfLine::Integer},
        {QStringLiteral("dock.screens"), QStringLiteral("SCREENS"), ConfLine::Descs},
        {QStringLiteral("dock.reflections"), QStringLiteral("REFLECTIONS"), ConfLine::Flag},
        {QStringLiteral("dock.launcher"), QStringLiteral("LAUNCHER"), ConfLine::Text},
        {QStringLiteral("dock.autohide"), QStringLiteral("AUTOHIDE"), ConfLine::Flag},
    };
    return lines;
}

// The text after "KEY=" (Riley's set_icon_size, set_screens, set_reflections, set_launcher, set_autohide).
bool confValue(const Config& config, const ConfLine& line, QString* out, QString* error)
{
    switch (line.kind) {
    case ConfLine::Integer:
        *out = QString::number(static_cast<long long>(std::trunc(config.number(line.option))));
        return true;
    case ConfLine::Flag:
        *out = config.flag(line.option) ? QStringLiteral("1") : QStringLiteral("0");
        return true;
    case ConfLine::Text:
        *out = config.text(line.option);
        return true;
    case ConfLine::Descs: {
        QStringList values;
        for (const Item& item : config.list(line.option)) {
            QString v = item.text(QStringLiteral("desc"));
            if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('\r')) || v.contains(QLatin1Char('\n'))) {
                *error = QStringLiteral("screen descriptions cannot contain commas or newlines");
                return false;
            }
            v = v.trimmed();
            if (!v.isEmpty() && !values.contains(v))
                values.append(v);
        }
        *out = values.join(QLatin1Char(','));
        return true;
    }
    }
    return false;
}

// Python's str.splitlines(keepends=True) for the line ends a config file has: \n, \r\n and a lone \r.
QStringList splitLinesKeepEnds(const QString& text)
{
    QStringList lines;
    qsizetype start = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            if (c == QLatin1Char('\r') && i + 1 < text.size() && text.at(i + 1) == QLatin1Char('\n'))
                ++i;
            lines.append(text.mid(start, i + 1 - start));
            start = i + 1;
        }
    }
    if (start < text.size())
        lines.append(text.mid(start));
    return lines;
}

// Riley's dock_core.py _set_config on text: the first line whose part before "=" is the key (trimmed) becomes
// KEY=value (keeping a \r\n end), later lines of that key go, and with none the line is added at the end.
QString setConfig(const QString& text, const QString& key, const QString& value)
{
    QStringList lines;
    bool written = false;
    for (const QString& line : splitLinesKeepEnds(text)) {
        if (line.section(QLatin1Char('='), 0, 0).trimmed() == key) {
            if (!written) {
                lines.append(key + QLatin1Char('=') + value
                             + (line.endsWith(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n")));
                written = true;
            }
        } else {
            lines.append(line);
        }
    }
    if (!written) {
        if (!lines.isEmpty() && !lines.last().endsWith(QLatin1Char('\n')))
            lines.append(QStringLiteral("\n"));
        lines.append(key + QLatin1Char('=') + value + QLatin1Char('\n'));
    }
    return lines.join(QString());
}

class RileyDock final : public Writer {
public:
    QString target() const override { return QStringLiteral("riley-dock"); }

    bool render(const Config&, const QByteArray*, QByteArray*, QString* error) const override
    {
        *error = QStringLiteral("riley-dock: writes its own files (pins and riley.conf), not one rendered file");
        return false;
    }

    bool writesOwnFiles() const override { return true; }

    bool writeFiles(const Config& config, const WriteContext& context, QString* error) const override
    {
        const auto changed = [&](const QString& key) { return context.changed.isEmpty() || context.changed.contains(key); };

        QStringList keys{PINS};
        for (const ConfLine& l : confLines())
            keys.append(l.option);
        if (!config.require(keys, error))
            return false;

        if (changed(PINS)) {
            QByteArray text;
            for (const Item& item : config.list(PINS)) {
                const QString app = item.text(QStringLiteral("app")).trimmed();
                if (app.isEmpty() || app.contains(QLatin1Char('\n')) || app.contains(QLatin1Char('\r'))) {
                    *error = QStringLiteral("an app id on the dock is empty or holds a newline");
                    return false;
                }
                text += app.toUtf8() + '\n';
            }
            const QString path = context.cacheHome + QStringLiteral("/nwg-dock-pinned");
            QDir().mkpath(QFileInfo(path).absolutePath());
            if (!writeTargetFile(path, text, error))
                return false;
        }

        QList<const ConfLine*> todo;
        for (const ConfLine& l : confLines()) {
            if (changed(l.option))
                todo.append(&l);
        }
        if (todo.isEmpty())
            return true;
        const QString path = context.configHome + QStringLiteral("/nwg-dock-hyprland/riley.conf");
        QString text;
        {
            QFile f(path);
            if (f.exists()) {
                if (!f.open(QIODevice::ReadOnly)) {
                    *error = path + QStringLiteral(": ") + f.errorString();
                    return false;
                }
                text = QString::fromUtf8(f.readAll());
            }
        }
        for (const ConfLine* l : std::as_const(todo)) {
            QString value;
            if (!confValue(config, *l, &value, error))
                return false;
            text = setConfig(text, l->confKey, value);
        }
        QDir().mkpath(QFileInfo(path).absolutePath());
        return writeTargetFile(path, text.toUtf8(), error);
    }
};

}  // namespace

std::unique_ptr<Writer> makeRileyDock()
{
    return std::make_unique<RileyDock>();
}

}  // namespace riley::config::writers
