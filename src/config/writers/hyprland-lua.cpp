// Writer for the target "hyprland-lua": ~/.config/hypr/settings.lua, the whole file.
// A port of Riley's settings/riley_settings/core.py render(): the Displays part always, then Appearance and
// Keyboard-and-mouse when one of their keys is set in the values, then the startup apps when one is enabled.
#include "writers.h"

#include <cmath>
#include <vector>

namespace riley::config::writers {
namespace {

const char* const kHeader =
    "-- Written by Riley Settings; do not edit by hand (the app rewrites it).\n"
    "-- Delete this file and settings.json to go back to the defaults in hyprland.lua.\n";

// Option key -> its place in Hyprland's config, in the order written (core.py APPEARANCE and INPUT).
struct Place {
    const char* key;
    const char* path;
};

const std::vector<Place> kAppearance = {
    {"appearance.gaps_in", "general.gaps_in"},
    {"appearance.gaps_out", "general.gaps_out"},
    {"appearance.border_size", "general.border_size"},
    {"appearance.rounding", "decoration.rounding"},
    {"appearance.active_opacity", "decoration.active_opacity"},
    {"appearance.inactive_opacity", "decoration.inactive_opacity"},
    {"appearance.blur", "decoration.blur.enabled"},
    {"appearance.blur_size", "decoration.blur.size"},
    {"appearance.blur_passes", "decoration.blur.passes"},
    {"appearance.shadow", "decoration.shadow.enabled"},
    {"appearance.animations", "animations.enabled"},
};

const std::vector<Place> kInput = {
    {"input.kb_layout", "input.kb_layout"},
    {"input.kb_variant", "input.kb_variant"},
    {"input.kb_options", "input.kb_options"},
    {"input.repeat_rate", "input.repeat_rate"},
    {"input.repeat_delay", "input.repeat_delay"},
    {"input.sensitivity", "input.sensitivity"},
    {"input.accel_profile", "input.accel_profile"},
    {"input.natural_scroll", "input.natural_scroll"},
    {"input.follow_mouse", "input.follow_mouse"},
};

struct Section {
    const char* title;
    const std::vector<Place>* places;
};

const std::vector<Section> kSections = {
    {"Appearance", &kAppearance},
    {"Keyboard and mouse", &kInput},
};

// A Lua string literal, escaped as core.py lua_value does (backslash and double quote).
QString luaString(const QString& s)
{
    QString out = s;
    out.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    out.replace(QLatin1Char('"'), QLatin1String("\\\""));
    return QLatin1Char('"') + out + QLatin1Char('"');
}

QString luaBool(bool b)
{
    return b ? QStringLiteral("true") : QStringLiteral("false");
}

// An integer as Python's str(int) writes it; any other number as %g.
QString positionNumber(double v)
{
    if (std::isfinite(v) && v == std::trunc(v) && std::fabs(v) < 9.0e15)
        return QString::number(static_cast<qint64>(v));
    return formatNumber(v);
}

// One option's value as Lua, by its type in the description.
QString luaValue(const Config& config, const Option& option)
{
    switch (option.type) {
    case OptionType::Switch:
        return luaBool(config.flag(option.key));
    case OptionType::Slider:
    case OptionType::Number:
        return formatNumber(config.number(option.key));
    case OptionType::Choice:
        if (option.integerChoices())
            return formatNumber(config.number(option.key));
        return luaString(config.text(option.key));
    default:
        return luaString(config.text(option.key));
    }
}

// A nested Lua table kept in first-insertion order (as core.py nested() builds a dict).
struct Node {
    QString name;
    QString leaf;   // the Lua value, for a leaf
    bool isLeaf = false;
    std::vector<Node> children;

    Node* child(const QString& n)
    {
        for (Node& c : children)
            if (c.name == n)
                return &c;
        children.push_back(Node{n, {}, false, {}});
        return &children.back();
    }
};

QString luaTable(const std::vector<Node>& fields)
{
    QStringList parts;
    for (const Node& n : fields)
        parts << n.name + QStringLiteral(" = ") + (n.isLeaf ? n.leaf : luaTable(n.children));
    return QStringLiteral("{ ") + parts.join(QStringLiteral(", ")) + QStringLiteral(" }");
}

QString luaPairs(const QList<QPair<QString, QString>>& pairs)
{
    QStringList parts;
    for (const auto& p : pairs)
        parts << p.first + QStringLiteral(" = ") + p.second;
    return QStringLiteral("{ ") + parts.join(QStringLiteral(", ")) + QStringLiteral(" }");
}

class HyprlandLua final : public Writer {
public:
    QString target() const override { return QStringLiteral("hyprland-lua"); }

    bool render(const Config& config, const QByteArray* existing, QByteArray* out, QString* error) const override
    {
        Q_UNUSED(existing);
        QStringList keys = {QStringLiteral("display.monitors"), QStringLiteral("startup.apps")};
        for (const Section& s : kSections)
            for (const Place& p : *s.places)
                keys << QString::fromLatin1(p.key);
        if (!config.require(keys, error))
            return false;

        QString text = QString::fromLatin1(kHeader) + QStringLiteral("\n-- Displays\n");
        for (const Item& m : config.list(QStringLiteral("display.monitors"))) {
            const QString output = luaString(QStringLiteral("desc:") + m.text(QStringLiteral("desc")));
            if (!m.flag(QStringLiteral("enabled"))) {
                text += QStringLiteral("hl.monitor(")
                    + luaPairs({{QStringLiteral("output"), output}, {QStringLiteral("disabled"), luaBool(true)}})
                    + QStringLiteral(")\n");
                continue;
            }
            const QString position = positionNumber(m.number(QStringLiteral("x"))) + QLatin1Char('x')
                + positionNumber(m.number(QStringLiteral("y")));
            text += QStringLiteral("hl.monitor(")
                + luaPairs({
                    {QStringLiteral("output"), output},
                    {QStringLiteral("mode"), luaString(m.text(QStringLiteral("mode")))},
                    {QStringLiteral("position"), luaString(position)},
                    {QStringLiteral("scale"), formatNumber(m.number(QStringLiteral("scale")))},
                    {QStringLiteral("transform"), formatNumber(m.number(QStringLiteral("transform")))},
                })
                + QStringLiteral(")\n");
        }
        text += QStringLiteral(
            "hl.monitor({ output = \"\", mode = \"preferred\", position = \"auto\", scale = \"auto\" })   -- any "
            "other screen\n");

        for (const Section& s : kSections) {
            Node top;   // holds the section's top-level fields
            for (const Place& p : *s.places) {
                const QString key = QString::fromLatin1(p.key);
                if (!config.isSet(key))
                    continue;
                const QStringList path = QString::fromLatin1(p.path).split(QLatin1Char('.'));
                Node* node = &top;
                for (int i = 0; i + 1 < path.size(); ++i)
                    node = node->child(path[i]);
                Node leaf{path.last(), luaValue(config, *config.option(key)), true, {}};
                node->children.push_back(leaf);
            }
            if (top.children.empty())
                continue;
            text += QStringLiteral("\n-- ") + QString::fromLatin1(s.title) + QStringLiteral("\nhl.config(")
                + luaTable(top.children) + QStringLiteral(")\n");
        }

        QStringList apps;
        for (const Item& e : config.list(QStringLiteral("startup.apps"))) {
            const QString exec = e.text(QStringLiteral("exec"));
            if (!e.flag(QStringLiteral("enabled")) || exec.trimmed().isEmpty())
                continue;
            apps << QStringLiteral("    hl.exec_cmd(") + luaString(exec) + QStringLiteral(")   -- ")
                    + e.text(QStringLiteral("name")).simplified() + QLatin1Char('\n');
        }
        if (!apps.isEmpty())
            text += QStringLiteral("\n-- Startup apps\nhl.on(\"hyprland.start\", function()\n") + apps.join(QString())
                + QStringLiteral("end)\n");

        *out = text.toUtf8();
        return true;
    }
};

}  // namespace

std::unique_ptr<Writer> makeHyprlandLua()
{
    return std::make_unique<HyprlandLua>();
}

}  // namespace riley::config::writers
