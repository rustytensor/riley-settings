// Writer for the target "hyprland-keys": ~/.config/hypr/keys-user.lua, the whole file.
// Riley's hypr/keys.lua (riley bf5b0fb) does pcall(require, "keys-user") and reads a table from option key to
// binding in the panel's format ("SUPER+SHIFT+Escape"). Only the keys your values file sets are written, in the
// description's order; a missing key keeps Riley's default. With no key set the file is removed (a missing file
// keeps every default).
#include "writers.h"

namespace riley::config::writers {
namespace {

// A Lua string literal in double quotes: \ and " escaped, control bytes as \ddd (three digits, so a digit after
// one cannot join it).
QByteArray luaString(const QString& text)
{
    QByteArray out = "\"";
    for (const char c : text.toUtf8()) {
        const auto u = static_cast<unsigned char>(c);
        if (c == '\\' || c == '"') {
            out += '\\';
            out += c;
        } else if (u < 0x20 || u == 0x7f) {
            out += '\\' + QByteArray::number(u).rightJustified(3, '0');
        } else {
            out += c;
        }
    }
    return out + '"';
}

class HyprlandKeys final : public Writer {
public:
    QString target() const override { return QStringLiteral("hyprland-keys"); }

    bool removesFile(const Config& config) const override { return setKeys(config).isEmpty(); }

    bool render(const Config& config, const QByteArray* existing, QByteArray* out, QString* error) const override
    {
        Q_UNUSED(existing);
        Q_UNUSED(error);
        QByteArray s = "-- Written by Riley Settings. Keys you changed; the rest keep Riley's defaults.\n"
                       "return {\n";
        for (const QString& key : setKeys(config))
            s += "  [" + luaString(key) + "] = " + luaString(config.text(key)) + ",\n";
        s += "}\n";
        *out = s;
        return true;
    }

private:
    // The options that name this target and that the values file sets, in the description's order.
    QStringList setKeys(const Config& config) const
    {
        QStringList keys;
#ifndef RILEY_PLANT_FAULT_KEYS
        for (const Option& o : config.description().options()) {
            if (o.targets.contains(target()) && config.isSet(o.key))
                keys.append(o.key);
        }
#else
        Q_UNUSED(config);   // the keys gate's planted fault: the values file is ignored
#endif
        return keys;
    }
};

}  // namespace

std::unique_ptr<Writer> makeHyprlandKeys()
{
    return std::make_unique<HyprlandKeys>();
}

}  // namespace riley::config::writers
