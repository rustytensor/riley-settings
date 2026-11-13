// Writer for the target "greeter-sway": the login screen's sway file, the whole file.
// Port of Riley's settings/riley_settings/core.py render_greeter: the same screens as the desktop, sway's rotation
// names, and the keyboard layout (input.kb_layout, or "us" when empty).
#include "writers.h"

#include <cmath>

namespace riley::config::writers {
namespace {

// A number as the fixture's JSON gave it: an integer stays an integer, anything else as %g.
QByteArray plainNumber(double v)
{
    if (std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 1e15)
        return QByteArray::number(static_cast<long long>(v));
    return formatNumber(v).toUtf8();
}

class GreeterSway final : public Writer {
public:
    QString target() const override { return QStringLiteral("greeter-sway"); }

    bool render(const Config& config, const QByteArray* existing, QByteArray* out, QString* error) const override
    {
        Q_UNUSED(existing);
        static const QStringList keys = {
            QStringLiteral("display.monitors"),
            QStringLiteral("input.kb_layout"),
        };
        if (!config.require(keys, error))
            return false;

        static const char* const swayRotation[4] = {"normal", "270", "180", "90"};   // sway turns the other way

        QByteArray s;
        s += "# Sway for the SDDM greeter (Riley). Written by riley-greeter-sync from your display settings; do not "
             "edit.\n";
        s += "xwayland disable\n";
        s += "swaybg_command -\n";
        s += "# Screens as in Riley Settings. sway turns the other way from Hyprland: sway 90 = Hyprland transform 3.\n";
        for (const Item& m : config.list(QStringLiteral("display.monitors"))) {
            const QByteArray desc = m.text(QStringLiteral("desc")).toUtf8();
            if (!m.flag(QStringLiteral("enabled"))) {
                s += "output \"" + desc + "\" disable\n";
                continue;
            }
            const int t = ((static_cast<int>(m.number(QStringLiteral("transform"))) % 4) + 4) % 4;   // Python's %
            s += "output \"" + desc + "\" mode " + m.text(QStringLiteral("mode")).toUtf8() + "Hz position "
                + plainNumber(m.number(QStringLiteral("x"))) + " " + plainNumber(m.number(QStringLiteral("y")))
                + " scale " + formatNumber(m.number(QStringLiteral("scale"))).toUtf8() + " transform "
                + swayRotation[t] + "\n";
        }
        QString layout = config.text(QStringLiteral("input.kb_layout"));
        if (layout.isEmpty())
            layout = QStringLiteral("us");
        s += "seat * xcursor_theme Adwaita 24\n";
        s += "input type:keyboard xkb_layout " + layout.toUtf8() + "\n";
        s += "exec command -v swayidle >/dev/null && swayidle -w timeout 300 'swaymsg \"output * power off\"' resume "
             "'swaymsg \"output * power on\"'\n";
        s += "for_window [app_id=\"^sddm-greeter\"] fullscreen enable\n";
        *out = s;
        return true;
    }
};

}  // namespace

std::unique_ptr<Writer> makeGreeterSway()
{
    return std::make_unique<GreeterSway>();
}

}  // namespace riley::config::writers
