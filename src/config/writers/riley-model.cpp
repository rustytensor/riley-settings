// Writer for the target "riley-model": ~/.config/hypr/settings.json, the whole file, in the format of Riley's GTK
// panel (settings/riley_settings/core.py: _write_atomic(json.dumps(model_for_save(model), indent=2) + "\n")).
// riley-greeter-sync.path watches this file and renders the login screen's sway config from it as root
// (Riley's sddm/install-sddm.sh; docs/switch-contract.md).
//
// The keys, in order: "version" 1, "monitors" (always), then "appearance" and "input" when one of their keys is set
// (each written whole, unset keys at their defaults), "startup" when it has items, "power" when one of its keys is set.
// Numbers as %g; booleans and strings as JSON with Python's ensure_ascii escaping.
#include "writers.h"

#include <vector>

namespace riley::config::writers {
namespace {

struct Place {
    const char* key;     // option key
    const char* field;   // its name in the model
};

const std::vector<Place> kAppearance = {
    {"appearance.gaps_in", "gaps_in"},
    {"appearance.gaps_out", "gaps_out"},
    {"appearance.border_size", "border_size"},
    {"appearance.rounding", "rounding"},
    {"appearance.active_opacity", "active_opacity"},
    {"appearance.inactive_opacity", "inactive_opacity"},
    {"appearance.blur", "blur"},
    {"appearance.blur_size", "blur_size"},
    {"appearance.blur_passes", "blur_passes"},
    {"appearance.shadow", "shadow"},
    {"appearance.animations", "animations"},
};

const std::vector<Place> kInput = {
    {"input.kb_layout", "kb_layout"},
    {"input.kb_variant", "kb_variant"},
    {"input.kb_options", "kb_options"},
    {"input.repeat_rate", "repeat_rate"},
    {"input.repeat_delay", "repeat_delay"},
    {"input.sensitivity", "sensitivity"},
    {"input.accel_profile", "accel_profile"},
    {"input.natural_scroll", "natural_scroll"},
    {"input.follow_mouse", "follow_mouse"},
};

const std::vector<Place> kPower = {
    {"power.screensaver_after", "screensaver"},
    {"power.lock_after", "lock"},
    {"power.screens_off_after", "screens_off"},
    {"power.sleep_after", "sleep"},
};

const char* const kMonitors = "display.monitors";
const char* const kStartup = "startup.apps";

// A JSON string as Python's json.dumps writes it (ensure_ascii): \" \\ \n \r \t \b \f, other control characters
// and everything above U+007F as \uXXXX (lower-case hex, UTF-16 units).
QString jsonString(const QString& s)
{
    QString out = QStringLiteral("\"");
    for (const QChar c : s) {
        const ushort u = c.unicode();
        switch (u) {
        case '"': out += QStringLiteral("\\\""); break;
        case '\\': out += QStringLiteral("\\\\"); break;
        case '\n': out += QStringLiteral("\\n"); break;
        case '\r': out += QStringLiteral("\\r"); break;
        case '\t': out += QStringLiteral("\\t"); break;
        case '\b': out += QStringLiteral("\\b"); break;
        case '\f': out += QStringLiteral("\\f"); break;
        default:
            if (u < 0x20 || u > 0x7e)
                out += QStringLiteral("\\u") + QStringLiteral("%1").arg(u, 4, 16, QLatin1Char('0'));
            else
                out += c;
        }
    }
    return out + QLatin1Char('"');
}

QString jsonBool(bool b)
{
    return b ? QStringLiteral("true") : QStringLiteral("false");
}

// One option's value as JSON, by its type in the description.
QString jsonValue(const Config& config, const Option& option)
{
    switch (option.type) {
    case OptionType::Switch:
        return jsonBool(config.flag(option.key));
    case OptionType::Slider:
    case OptionType::Number:
        return formatNumber(config.number(option.key));
    case OptionType::Choice:
        if (option.integerChoices())
            return formatNumber(config.number(option.key));
        break;
    default:
        break;
    }
    QString text = config.text(option.key);
    if (option.key == QLatin1String("input.accel_profile") && text == QLatin1String("adaptive"))
        text.clear();   // Riley's "" is Hyprland's own acceleration, adaptive
    return jsonString(text);
}

// An object at `indent` (its members one level deeper), json.dumps(indent=2) style.
QString jsonObject(const QList<QPair<QString, QString>>& members, const QString& indent)
{
    if (members.isEmpty())
        return QStringLiteral("{}");
    const QString inner = indent + QStringLiteral("  ");
    QStringList parts;
    for (const auto& m : members)
        parts << inner + jsonString(m.first) + QStringLiteral(": ") + m.second;
    return QStringLiteral("{\n") + parts.join(QStringLiteral(",\n")) + QLatin1Char('\n') + indent + QLatin1Char('}');
}

QString jsonArray(const QStringList& elements, const QString& indent)
{
    if (elements.isEmpty())
        return QStringLiteral("[]");
    const QString inner = indent + QStringLiteral("  ");
    QStringList parts;
    for (const QString& e : elements)
        parts << inner + e;
    return QStringLiteral("[\n") + parts.join(QStringLiteral(",\n")) + QLatin1Char('\n') + indent + QLatin1Char(']');
}

class RileyModel final : public Writer {
public:
    QString target() const override { return QStringLiteral("riley-model"); }

    bool render(const Config& config, const QByteArray* existing, QByteArray* out, QString* error) const override
    {
        Q_UNUSED(existing);
        QStringList keys = {QString::fromLatin1(kMonitors), QString::fromLatin1(kStartup)};
        for (const auto* places : {&kAppearance, &kInput, &kPower})
            for (const Place& p : *places)
                keys << QString::fromLatin1(p.key);
        if (!config.require(keys, error))
            return false;

        const QString l1 = QStringLiteral("  ");
        const QString l2 = QStringLiteral("    ");
        QList<QPair<QString, QString>> top;
        top.append({QStringLiteral("version"), QStringLiteral("1")});

        QStringList monitors;
        for (const Item& m : config.list(QString::fromLatin1(kMonitors))) {
            QList<QPair<QString, QString>> fields{
                {QStringLiteral("desc"), jsonString(m.text(QStringLiteral("desc")))},
                {QStringLiteral("enabled"), jsonBool(m.flag(QStringLiteral("enabled")))},
                {QStringLiteral("mode"), jsonString(m.text(QStringLiteral("mode")))},
                {QStringLiteral("x"), formatNumber(m.number(QStringLiteral("x")))},
                {QStringLiteral("y"), formatNumber(m.number(QStringLiteral("y")))},
                {QStringLiteral("scale"), formatNumber(m.number(QStringLiteral("scale")))},
                {QStringLiteral("transform"), formatNumber(m.number(QStringLiteral("transform")))},
            };
            // As Riley's model keeps it (riley 0027087): "bitdepth" last, and only when it is not 8.
            if (m.has(QStringLiteral("bitdepth")) && m.number(QStringLiteral("bitdepth")) != 8)
                fields.append({QStringLiteral("bitdepth"), formatNumber(m.number(QStringLiteral("bitdepth")))});
            monitors << jsonObject(fields, l2);
        }
        top.append({QStringLiteral("monitors"), jsonArray(monitors, l1)});

        const auto section = [&](const char* name, const std::vector<Place>& places) {
            bool any = false;
            for (const Place& p : places)
                any = any || config.isSet(QString::fromLatin1(p.key));
            if (!any)
                return;
            QList<QPair<QString, QString>> members;
            for (const Place& p : places) {
                const Option& o = *config.option(QString::fromLatin1(p.key));
                members.append({QString::fromLatin1(p.field), jsonValue(config, o)});
            }
            top.append({QString::fromLatin1(name), jsonObject(members, l1)});
        };
        section("appearance", kAppearance);
        section("input", kInput);

        QStringList apps;
        for (const Item& e : config.list(QString::fromLatin1(kStartup))) {
            apps << jsonObject({
                {QStringLiteral("name"), jsonString(e.text(QStringLiteral("name")))},
                {QStringLiteral("exec"), jsonString(e.text(QStringLiteral("exec")))},
                {QStringLiteral("enabled"), jsonBool(e.flag(QStringLiteral("enabled")))},
            }, l2);
        }
        if (!apps.isEmpty())
            top.append({QStringLiteral("startup"), jsonArray(apps, l1)});

        section("power", kPower);

        *out = (jsonObject(top, QString()) + QLatin1Char('\n')).toUtf8();
        return true;
    }
};

}  // namespace

std::unique_ptr<Writer> makeRileyModel()
{
    return std::make_unique<RileyModel>();
}

}  // namespace riley::config::writers
