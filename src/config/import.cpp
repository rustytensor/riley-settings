// The import of Riley's GTK panel's files (docs/switch-contract.md, "Import" and "The mapping"). Each reader follows
// the Riley function named in its comment; every value an old file states is imported, even one equal to the
// description's default, so the writers reproduce Riley's files byte for byte.
#include "import.h"

#include "atomic_file.h"
#include "config.h"
#include "values.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <cmath>
#include <cstdio>
#include <optional>

namespace riley::config {
namespace {

QString env(const char* name)
{
    return qEnvironmentVariable(name);
}

std::optional<QByteArray> readFile(const QString& path)
{
    QFile f(path);
    if (!QFileInfo(path).isFile() || !f.open(QIODevice::ReadOnly))
        return std::nullopt;
    return f.readAll();
}

// Python's str.splitlines() for the line ends these files have: \n, \r\n and a lone \r.
QStringList splitLines(const QString& text)
{
    QStringList lines;
    qsizetype start = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            lines.append(text.mid(start, i - start));
            if (c == QLatin1Char('\r') && i + 1 < text.size() && text.at(i + 1) == QLatin1Char('\n'))
                ++i;
            start = i + 1;
        }
    }
    if (start < text.size())
        lines.append(text.mid(start));
    return lines;
}

// A JSON scalar as the values file writes it: booleans true/false, numbers as %g, strings as they are.
std::optional<QString> jsonText(const QJsonValue& v)
{
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isDouble())
        return formatNumber(v.toDouble());
    if (v.isString())
        return v.toString();
    return std::nullopt;
}

QString shown(const QJsonValue& v)
{
    if (v.isString())
        return v.toString();
    const auto t = jsonText(v);
    if (t)
        return *t;
    return QString::fromUtf8(QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
}

// The keys of a JSON file's top-level object in file order (QJsonObject sorts them).
QStringList topLevelKeys(const QByteArray& d)
{
    QStringList keys;
    int depth = 0;
    bool expectKey = false;
    for (qsizetype i = 0; i < d.size(); ++i) {
        const char c = d.at(i);
        if (c == '"') {
            qsizetype j = i + 1;
            while (j < d.size() && d.at(j) != '"')
                j += d.at(j) == '\\' ? 2 : 1;
            if (depth == 1 && expectKey) {
                const QJsonDocument k = QJsonDocument::fromJson("[" + d.mid(i, j + 1 - i) + "]");
                keys.append(k.array().at(0).toString());
                expectKey = false;
            }
            i = j;
        } else if (c == '{' || c == '[') {
            ++depth;
            expectKey = depth == 1 && c == '{';
        } else if (c == '}' || c == ']') {
            --depth;
        } else if (c == ',' && depth == 1) {
            expectKey = true;
        }
    }
    return keys;
}

// ---- a small TOML reader: top-level `key = value` with basic or literal strings, integers and booleans ----------
struct TomlValue {
    enum Kind { String, Integer, Bool } kind = String;
    QString text;
    long long integer = 0;
};

bool parseToml(const QString& text, QHash<QString, TomlValue>* out, QString* error)
{
    static const QRegularExpression bareKey(QStringLiteral("^[A-Za-z0-9_-]+$"));
    static const QRegularExpression intRe(QStringLiteral("^[+-]?[0-9](_?[0-9])*$"));
    bool inTable = false;
    int n = 0;
    for (const QString& raw : splitLines(text)) {
        ++n;
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            inTable = true;
            continue;
        }
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq < 0) {
            *error = QStringLiteral("line %1 is not key = value").arg(n);
            return false;
        }
        QString key = line.left(eq).trimmed();
        if (key.size() >= 2 && key.startsWith(QLatin1Char('"')) && key.endsWith(QLatin1Char('"')))
            key = key.mid(1, key.size() - 2);
        else if (!bareKey.match(key).hasMatch()) {
            *error = QStringLiteral("line %1: a key TOML does not allow").arg(n);
            return false;
        }
        const QString rest = line.mid(eq + 1).trimmed();
        TomlValue v;
        QString after;
        if (rest.startsWith(QLatin1Char('"'))) {
            qsizetype i = 1;
            bool closed = false;
            for (; i < rest.size(); ++i) {
                const QChar c = rest.at(i);
                if (c == QLatin1Char('"')) {
                    closed = true;
                    break;
                }
                if (c != QLatin1Char('\\')) {
                    v.text += c;
                    continue;
                }
                if (++i >= rest.size())
                    break;
                const QChar e = rest.at(i);
                switch (e.unicode()) {
                case 'n': v.text += QLatin1Char('\n'); break;
                case 't': v.text += QLatin1Char('\t'); break;
                case 'r': v.text += QLatin1Char('\r'); break;
                case 'b': v.text += QLatin1Char('\b'); break;
                case 'f': v.text += QLatin1Char('\f'); break;
                case '"': v.text += QLatin1Char('"'); break;
                case '\\': v.text += QLatin1Char('\\'); break;
                case 'u':
                case 'U': {
                    const int len = e == QLatin1Char('u') ? 4 : 8;
                    bool ok = false;
                    const uint cp = rest.mid(i + 1, len).toUInt(&ok, 16);
                    if (!ok || rest.mid(i + 1, len).size() != len) {
                        *error = QStringLiteral("line %1: a bad \\u escape").arg(n);
                        return false;
                    }
                    const char32_t c32 = cp;
                    v.text += QString::fromUcs4(&c32, 1);
                    i += len;
                    break;
                }
                default:
                    *error = QStringLiteral("line %1: an escape TOML does not allow").arg(n);
                    return false;
                }
            }
            if (!closed) {
                *error = QStringLiteral("line %1: a string without its closing quote").arg(n);
                return false;
            }
            after = rest.mid(i + 1).trimmed();
        } else if (rest.startsWith(QLatin1Char('\''))) {
            const qsizetype end = rest.indexOf(QLatin1Char('\''), 1);
            if (end < 0) {
                *error = QStringLiteral("line %1: a string without its closing quote").arg(n);
                return false;
            }
            v.text = rest.mid(1, end - 1);
            after = rest.mid(end + 1).trimmed();
        } else {
            QString word = rest;
            const qsizetype hash = word.indexOf(QLatin1Char('#'));
            if (hash >= 0) {
                after = word.mid(hash);
                word = word.left(hash).trimmed();
            }
            if (word == QLatin1String("true") || word == QLatin1String("false")) {
                v.kind = TomlValue::Bool;
                v.text = word;
            } else if (intRe.match(word).hasMatch()) {
                v.kind = TomlValue::Integer;
                QString digits = word;
                digits.remove(QLatin1Char('_'));
                v.integer = digits.toLongLong();
                v.text = QString::number(v.integer);
            } else {
                *error = QStringLiteral("line %1: a value this reader does not take").arg(n);
                return false;
            }
        }
        if (!after.isEmpty() && !after.startsWith(QLatin1Char('#'))) {
            *error = QStringLiteral("line %1: text after the value").arg(n);
            return false;
        }
        if (!inTable)
            out->insert(key, v);
    }
    return true;
}

// ---- what the import collects, then writes in description order ---------------------------------------------------
struct Entry {
    QString file;
    bool isList = false;
    QString text;
    QList<ItemFields> items;
};

class Collector {
public:
    explicit Collector(const Description& d) : m_d(d) {}

    void skip(const QString& file, const QString& key, const QString& value, const QString& reason)
    {
        m_skipped.append(QStringLiteral("skipped ") + file + QStringLiteral(": ") + key + QLatin1Char(' ') + value
                         + QStringLiteral(": ") + reason);
    }

    void scalar(const QString& file, const QString& key, const QString& text)
    {
        const Option* o = m_d.option(key);
        if (!o) {
            skip(file, key, text, QStringLiteral("the description has no such option"));
            return;
        }
        const QString why = checkTyped(*o, text);
        if (!why.isEmpty()) {
            skip(file, key, text, why);
            return;
        }
        m_entries.insert(key, Entry{file, false, text, {}});
    }

    void list(const QString& file, const QString& key, const QList<ItemFields>& items)
    {
        const Option* o = m_d.option(key);
        if (!o) {
            skip(file, key, QString(), QStringLiteral("the description has no such option"));
            return;
        }
        QList<ItemFields> kept;
        for (const ItemFields& item : items) {
            QString why;
            for (auto it = item.cbegin(); it != item.cend() && why.isEmpty(); ++it) {
                const Field* f = o->field(it.key());
                why = f ? checkTyped(*f, it.value()) : QStringLiteral("no field ") + it.key();
                if (!why.isEmpty())
                    why = it.key() + QStringLiteral(" \"") + it.value() + QStringLiteral("\" ") + why;
            }
            if (why.isEmpty())
                kept.append(item);
            else
                skip(file, key, QStringLiteral("item"), why);
        }
        if (!kept.isEmpty())   // an empty list is the default and is not written
            m_entries.insert(key, Entry{file, true, {}, kept});
    }

    const QHash<QString, Entry>& entries() const { return m_entries; }
    const QStringList& skipped() const { return m_skipped; }

private:
    // checkScalar, plus a slider's step (a dock icon size of 45 is not on the 2 px steps).
    static QString checkTyped(const Typed& t, const QString& text)
    {
        const QString why = checkScalar(t, text);
        if (!why.isEmpty())
            return why;
        if (t.type == OptionType::Slider && t.step && *t.step > 0) {
            double v = 0;
            parseNumber(text, &v);
            const double k = (v - t.min.value_or(0)) / *t.step;
            if (std::fabs(k - std::round(k)) > 1e-9)
                return QStringLiteral("is off its step ") + formatNumber(*t.step);
        }
        return {};
    }

    const Description& m_d;
    QHash<QString, Entry> m_entries;
    QStringList m_skipped;
};

const char* const kAppearance[] = {"gaps_in", "gaps_out", "border_size", "rounding", "active_opacity",
                                   "inactive_opacity", "blur", "blur_size", "blur_passes", "shadow", "animations"};
const char* const kInput[] = {"kb_layout", "kb_variant", "kb_options", "repeat_rate", "repeat_delay",
                              "sensitivity", "accel_profile", "natural_scroll", "follow_mouse"};
// Riley's power steps: the model's name, the option key, and the command hypridle runs for it (core.POWER).
struct PowerStep {
    const char* model;
    const char* key;
    const char* command;
};
const PowerStep kPower[] = {
    {"screensaver", "power.screensaver_after", "hypr-screensaver"},
    {"lock", "power.lock_after", "loginctl lock-session"},
    {"screens_off", "power.screens_off_after", "hyprctl dispatch 'hl.dsp.dpms({action = \"off\"})'"},
    {"sleep", "power.sleep_after", "systemctl suspend"},
};

// core.parse_idle: each step's timeout (0 for a step the file lacks).
QHash<QString, long long> parseIdle(const QString& text)
{
    QHash<QString, long long> out;
    for (const PowerStep& s : kPower)
        out.insert(QString::fromLatin1(s.model), 0);
    static const QRegularExpression digits(QStringLiteral("^[0-9]+$"));
    const QStringList blocks = text.split(QStringLiteral("\nlistener"));
    for (qsizetype b = 1; b < blocks.size(); ++b) {
        const QString body = blocks[b].section(QStringLiteral("\n}"), 0, 0);
        QHash<QString, QString> lines;
        for (const QString& l : splitLines(body)) {
            const qsizetype eq = l.indexOf(QLatin1Char('='));
            if (eq < 0 || eq + 1 >= l.size())
                continue;   // Python: `if v`, v the part after "="
            lines.insert(l.left(eq).trimmed(), l.mid(eq + 1).trimmed());
        }
        for (const PowerStep& s : kPower) {
            const QString timeout = lines.value(QStringLiteral("timeout"));
            if (lines.value(QStringLiteral("on-timeout")) == QLatin1String(s.command) && digits.match(timeout).hasMatch())
                out.insert(QString::fromLatin1(s.model), timeout.toLongLong());
        }
    }
    return out;
}

// ---- the readers, one per old file ----------------------------------------------------------------------------------

// The model (core.load_model, without the live screens: no command runs).
void readModel(Collector* c, const QString& path, bool* hasPower)
{
    *hasPower = false;
    const auto data = readFile(path);
    if (!data)
        return;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(*data, &pe);
    if (!doc.isObject()) {
        c->skip(path, QStringLiteral("(the whole file)"), QString(), QStringLiteral("not a JSON object: ") + pe.errorString());
        return;
    }
    const QJsonObject model = doc.object();

    QList<ItemFields> monitors;
    for (const QJsonValue& mv : model.value(QStringLiteral("monitors")).toArray()) {
        const QJsonObject m = mv.toObject();
        ItemFields item;
        for (const char* f : {"desc", "enabled", "mode", "x", "y", "scale", "transform"}) {
            const QString name = QString::fromLatin1(f);
            if (const auto t = jsonText(m.value(name)))
                item.insert(name, *t);
        }
        if (!item.contains(QStringLiteral("enabled")))
            item.insert(QStringLiteral("enabled"), QStringLiteral("true"));
        if (!item.contains(QStringLiteral("transform")))
            item.insert(QStringLiteral("transform"), QStringLiteral("0"));
        monitors.append(item);
    }
    c->list(path, QStringLiteral("display.monitors"), monitors);

    const auto section = [&](const char* name, const char* const* fields, size_t count) {
        const QJsonObject s = model.value(QString::fromLatin1(name)).toObject();
        for (size_t i = 0; i < count; ++i) {
            const QString field = QString::fromLatin1(fields[i]);
            if (!s.contains(field))
                continue;
            const QString key = QString::fromLatin1(name) + QLatin1Char('.') + field;
            auto t = jsonText(s.value(field));
            if (!t) {
                c->skip(path, key, shown(s.value(field)), QStringLiteral("not a value"));
                continue;
            }
            if (key == QLatin1String("input.accel_profile") && t->isEmpty())
                *t = QStringLiteral("adaptive");   // Riley's "" is Hyprland's own acceleration
            c->scalar(path, key, *t);
        }
    };
    section("appearance", kAppearance, std::size(kAppearance));
    section("input", kInput, std::size(kInput));

    QList<ItemFields> apps;
    for (const QJsonValue& av : model.value(QStringLiteral("startup")).toArray()) {
        const QJsonObject a = av.toObject();
        ItemFields item;
        for (const char* f : {"name", "exec", "enabled"}) {
            const QString name = QString::fromLatin1(f);
            if (const auto t = jsonText(a.value(name)))
                item.insert(name, *t);
        }
        if (!item.contains(QStringLiteral("enabled")))
            item.insert(QStringLiteral("enabled"), QStringLiteral("true"));
        apps.append(item);
    }
    c->list(path, QStringLiteral("startup.apps"), apps);

    // power.current_times: the saved times when the model has a non-empty "power", int(saved.get(k) or 0) each.
    const QJsonObject power = model.value(QStringLiteral("power")).toObject();
    if (!power.isEmpty()) {
        *hasPower = true;
        for (const PowerStep& s : kPower) {
            const QJsonValue v = power.value(QString::fromLatin1(s.model));
            const long long t = v.isDouble() ? static_cast<long long>(std::trunc(v.toDouble()))
                                             : v.isString() ? v.toString().trimmed().toLongLong() : 0;
            c->scalar(path, QString::fromLatin1(s.key), QString::number(t));
        }
    }
}

// power.current_times without saved times: idle.conf, else hypridle.conf, through core.parse_idle.
void readIdle(Collector* c, const QString& hypr)
{
    for (const QString& name : {QStringLiteral("idle.conf"), QStringLiteral("hypridle.conf")}) {
        const QString path = hypr + QLatin1Char('/') + name;
        const auto data = readFile(path);
        if (!data)
            continue;
        const QHash<QString, long long> times = parseIdle(QString::fromUtf8(*data));
        for (const PowerStep& s : kPower)
            c->scalar(path, QString::fromLatin1(s.key), QString::number(times.value(QString::fromLatin1(s.model))));
        return;
    }
}

// appearance.float_default: float-mode.json's "default".
void readFloatMode(Collector* c, const QString& path)
{
    const auto data = readFile(path);
    if (!data)
        return;
    const QJsonObject o = QJsonDocument::fromJson(*data).object();
    if (!o.contains(QStringLiteral("default")))
        return;
    const QJsonValue v = o.value(QStringLiteral("default"));
    if (!v.isString()) {
        c->skip(path, QStringLiteral("appearance.workspaces"), shown(v), QStringLiteral("not tiled or floating"));
        return;
    }
    c->scalar(path, QStringLiteral("appearance.workspaces"), v.toString());
}

// A TOML file of Riley's (fonts_core.load_user, weather_core.load_config): the keys present. Each name is read as a
// string, or as an integer when the option is a slider (fonts_core._validate: terminal_size must be an int).
void readToml(Collector* c, const Description& d, const QString& path, const QList<QPair<QString, QString>>& keys,
              bool trimStrings)
{
    const auto data = readFile(path);
    if (!data)
        return;
    QHash<QString, TomlValue> values;
    QString error;
    if (!parseToml(QString::fromUtf8(*data), &values, &error)) {
        c->skip(path, QStringLiteral("(the whole file)"), QString(), error);
        return;
    }
    for (const auto& [name, key] : keys) {
        if (!values.contains(name))
            continue;
        const TomlValue v = values.value(name);
        const Option* o = d.option(key);
        const bool wantInteger = o && o->type == OptionType::Slider;
        if (wantInteger ? v.kind != TomlValue::Integer : v.kind != TomlValue::String) {
            c->skip(path, key, v.text, wantInteger ? QStringLiteral("not an integer") : QStringLiteral("not a string"));
            continue;
        }
        c->scalar(path, key, trimStrings && !wantInteger ? v.text.trimmed() : v.text);
    }
}

// theme_wallpaper.current_theme: the file's text, trimmed.
void readTheme(Collector* c, const QString& path)
{
    const auto data = readFile(path);
    if (data)
        c->scalar(path, QStringLiteral("theme.name"), QString::fromUtf8(*data).trimmed());
}

// hyprpaper.conf with wallpapers.json, only when the panel picked the wallpaper (hypr-theme.py set_bg's header).
void readWallpaper(Collector* c, const QString& hypr)
{
    const QString conf = hypr + QStringLiteral("/hyprpaper.conf");
    const auto data = readFile(conf);
    if (!data)
        return;
    const QStringList lines = splitLines(QString::fromUtf8(*data));
    if (lines.isEmpty() || lines.first() != QLatin1String("# Written by hypr-theme.py (wallpaper picked in Riley Settings)."))
        return;   // the theme's: the theme decides
    const QString state = hypr + QStringLiteral("/wallpapers.json");
    if (const auto js = readFile(state)) {
        const QJsonObject o = QJsonDocument::fromJson(*js).object();
        QList<ItemFields> screens;
        for (const QString& k : topLevelKeys(*js)) {
            const QJsonValue v = o.value(k);
            if (!v.isString()) {
                c->skip(state, k == QLatin1String("*") ? QStringLiteral("wallpaper.picture") : QStringLiteral("wallpaper.screens"),
                        shown(v), QStringLiteral("not a path"));
                continue;
            }
            if (k == QLatin1String("*"))
                c->scalar(state, QStringLiteral("wallpaper.picture"), v.toString());
            else
                screens.append({{QStringLiteral("screen"), k}, {QStringLiteral("path"), v.toString()}});
        }
        c->list(state, QStringLiteral("wallpaper.screens"), screens);
        return;
    }
    // One picture everywhere: the block whose "monitor =" is empty.
    bool inBlock = false;
    std::optional<QString> monitor, path;
    for (const QString& raw : lines) {
        const QString l = raw.trimmed();
        if (!inBlock) {
            if (l.startsWith(QLatin1String("wallpaper")) && l.endsWith(QLatin1Char('{'))) {
                inBlock = true;
                monitor.reset();
                path.reset();
            }
            continue;
        }
        if (l == QLatin1String("}")) {
            inBlock = false;
            if (monitor && monitor->isEmpty() && path) {
                c->scalar(conf, QStringLiteral("wallpaper.picture"), *path);
                return;
            }
            continue;
        }
        const qsizetype eq = l.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString k = l.left(eq).trimmed(), v = l.mid(eq + 1).trimmed();
        if (k == QLatin1String("monitor"))
            monitor = v;
        else if (k == QLatin1String("path"))
            path = v;
    }
}

// dock_core.load_pins: each line trimmed, empty lines left out.
void readPins(Collector* c, const QString& path)
{
    const auto data = readFile(path);
    if (!data)
        return;
    QList<ItemFields> pins;
    for (const QString& l : splitLines(QString::fromUtf8(*data))) {
        if (!l.trimmed().isEmpty())
            pins.append({{QStringLiteral("app"), l.trimmed()}});
    }
    c->list(path, QStringLiteral("dock.pins"), pins);
}

// riley.conf through dock_core's readers: screen_filters, icon_size, reflections, launcher, autohide.
void readDockConf(Collector* c, const QString& path)
{
    const auto data = readFile(path);
    if (!data)
        return;
    const QStringList lines = splitLines(QString::fromUtf8(*data));
    // The last line whose part before "=" is `key` (trimmed): its part after "=".
    const auto last = [&](const QString& key) -> std::optional<QString> {
        std::optional<QString> v;
        for (const QString& l : lines) {
            const qsizetype eq = l.indexOf(QLatin1Char('='));
            if (eq >= 0 && l.left(eq).trimmed() == key)
                v = l.mid(eq + 1);
        }
        return v;
    };

    if (const auto screens = last(QStringLiteral("SCREENS"))) {
        QStringList seen;
        QList<ItemFields> items;
        for (const QString& part : screens->split(QLatin1Char(','))) {
            const QString d = part.trimmed();
            if (d.isEmpty() || seen.contains(d))
                continue;
            seen.append(d);
            items.append({{QStringLiteral("desc"), d}});
        }
        c->list(path, QStringLiteral("dock.screens"), items);
    }
    for (const QString& l : lines) {   // icon_size: the first ICON_SIZE= line whose rest is an integer
        if (!l.startsWith(QLatin1String("ICON_SIZE=")))
            continue;
        bool ok = false;
        const int px = l.mid(10).trimmed().toInt(&ok);
        if (!ok)
            continue;
        c->scalar(path, QStringLiteral("dock.icon_size"), QString::number(px));
        break;
    }
    if (const auto r = last(QStringLiteral("REFLECTIONS")))
        c->scalar(path, QStringLiteral("dock.reflections"),
                  r->trimmed() != QLatin1String("0") ? QStringLiteral("true") : QStringLiteral("false"));
    if (const auto l = last(QStringLiteral("LAUNCHER"))) {
        const QString v = l->trimmed();
        static const QStringList positions = {QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("center"),
                                              QStringLiteral("off")};
        if (positions.contains(v))
            c->scalar(path, QStringLiteral("dock.launcher"), v);
        else
            c->skip(path, QStringLiteral("dock.launcher"), v, QStringLiteral("an unknown launcher position"));
    }
    if (const auto a = last(QStringLiteral("AUTOHIDE")))
        c->scalar(path, QStringLiteral("dock.autohide"),
                  a->trimmed() == QLatin1String("1") ? QStringLiteral("true") : QStringLiteral("false"));
}

// desktop.json: show, the boxes that are not ~/Desktop's own (or are hidden), icon_size.
void readDesktop(Collector* c, const QString& path)
{
    const auto data = readFile(path);
    if (!data)
        return;
    const QJsonObject cfg = QJsonDocument::fromJson(*data).object();
    if (cfg.contains(QStringLiteral("show"))) {
        if (const auto t = jsonText(cfg.value(QStringLiteral("show"))))
            c->scalar(path, QStringLiteral("desktop.show"), *t);
    }
    QList<ItemFields> boxes;
    for (const QJsonValue& bv : cfg.value(QStringLiteral("boxes")).toArray()) {
        const QJsonObject b = bv.toObject();
        const bool hidden = b.value(QStringLiteral("hidden")).toBool(false);
        if (b.value(QStringLiteral("auto")).toBool(false) && !hidden)
            continue;
        boxes.append({{QStringLiteral("folder"), b.value(QStringLiteral("folder")).toString()},
                      {QStringLiteral("screen"), b.value(QStringLiteral("screen")).toString()},
                      {QStringLiteral("hidden"), hidden ? QStringLiteral("true") : QStringLiteral("false")}});
    }
    c->list(path, QStringLiteral("desktop.boxes"), boxes);
    if (cfg.contains(QStringLiteral("icon_size"))) {
        const QJsonValue v = cfg.value(QStringLiteral("icon_size"));
        if (const auto t = jsonText(v))
            c->scalar(path, QStringLiteral("desktop.icon_size"), *t);
        else
            c->skip(path, QStringLiteral("desktop.icon_size"), shown(v), QStringLiteral("not a number"));
    }
}

void printErr(const QString& line)
{
    std::fprintf(stderr, "riley-settings-qt: %s\n", line.toUtf8().constData());
}

}  // namespace

HomePaths HomePaths::fromEnvironment()
{
    HomePaths p;
    p.home = QDir::homePath();
    p.configHome = env("XDG_CONFIG_HOME");
    if (p.configHome.isEmpty())
        p.configHome = p.home + QStringLiteral("/.config");
    p.cacheHome = env("XDG_CACHE_HOME");
    if (p.cacheHome.isEmpty())
        p.cacheHome = p.home + QStringLiteral("/.cache");
    return p;
}

bool importOldFiles(const Description& description, const QString& schemaDir, const HomePaths& paths,
                    ImportResult* result, QString* error)
{
    Collector c(description);
    const QString hypr = paths.home + QStringLiteral("/.config/hypr");   // Riley: Path.home(), not XDG_CONFIG_HOME

    bool hasPower = false;
    readModel(&c, paths.oldModel(), &hasPower);
    if (!hasPower)
        readIdle(&c, hypr);
    readFloatMode(&c, hypr + QStringLiteral("/float-mode.json"));
    readToml(&c, description, paths.configHome + QStringLiteral("/riley/fonts.toml"),
             {{QStringLiteral("sans"), QStringLiteral("fonts.sans")},
              {QStringLiteral("mono"), QStringLiteral("fonts.mono")},
              {QStringLiteral("terminal_size"), QStringLiteral("fonts.terminal_size")}},
             false);
    readToml(&c, description, paths.configHome + QStringLiteral("/riley/weather.toml"),
             {{QStringLiteral("location"), QStringLiteral("weather.location")},
              {QStringLiteral("units"), QStringLiteral("weather.units")}},
             true);
    readTheme(&c, hypr + QStringLiteral("/current-theme"));
    readWallpaper(&c, hypr);
    readPins(&c, paths.home + QStringLiteral("/.cache/nwg-dock-pinned"));   // Riley: H / ".cache", not XDG
    readDockConf(&c, paths.configHome + QStringLiteral("/nwg-dock-hyprland/riley.conf"));
    readDesktop(&c, paths.home + QStringLiteral("/.config/riley/desktop.json"));

    // Each key in description order, through the M1 set path (scalars) and M2's list format.
    QList<const Option*> todo;
    for (const Option& o : description.options()) {
        if (c.entries().contains(o.key))
            todo.append(&o);
    }
#ifdef RILEY_PLANT_FAULT_SWITCH
    if (!todo.isEmpty())   // the M4 gate's planted fault: the last key is left out
        todo.removeLast();
#endif
    QByteArray text = newValuesFileText();
    for (const Option* o : std::as_const(todo)) {
        const Entry& e = c.entries().value(o->key);
        QString why;
        const QByteArray next = e.isList ? editListValue(text, *o, e.items, &why) : editValueText(text, o->key, e.text, &why);
        if (next.isEmpty()) {
            *error = o->key + QStringLiteral(": ") + why;
            return false;
        }
        text = next;
    }
    const auto check = Values::parse(text, QStringLiteral("the imported values"), schemaDir, error);
    if (!check)
        return false;
    QStringList unknown;
    if (!check->check(description, &unknown, error))
        return false;

    result->text = text;
    result->keys = int(todo.size());
    result->skipped = c.skipped();
    return true;
}

int runImport(const QString& descriptionPath, const QString& schemaDir, const QString& valuesPath,
              const HomePaths& paths)
{
    if (QFileInfo::exists(valuesPath) || QFileInfo(valuesPath).isSymLink()) {
        printErr(valuesPath + QStringLiteral(" exists; --import never overwrites a values file"));
        return 1;
    }
    QString error;
    const auto description = Description::load(descriptionPath, schemaDir, &error);
    if (!description) {
        printErr(error);
        return 2;
    }
    ImportResult result;
    if (!importOldFiles(*description, schemaDir, paths, &result, &error)) {
        printErr(QStringLiteral("import: ") + error);
        return 2;
    }
    for (const QString& line : std::as_const(result.skipped))
        printErr(line);
    QDir().mkpath(QFileInfo(valuesPath).absolutePath());
    if (!writeFileAtomic(valuesPath, result.text, &error)) {
        printErr(error);
        return 2;
    }
    std::printf("imported %d keys into %s\n", result.keys, valuesPath.toUtf8().constData());
    std::fflush(stdout);
    return 0;
}

}  // namespace riley::config
