// Writer for the target "hypridle": ~/.config/hypr/idle.conf, the whole file.
// Port of Riley's settings/riley_settings/core.py render_idle: a fixed general block, then one listener per step
// whose time is above 0 seconds, in the order screensaver, lock, screens off, sleep.
#include "writers.h"

namespace riley::config::writers {
namespace {

const char* const kScreensOn = "hyprctl dispatch 'hl.dsp.dpms({action = \"on\"})'";

class Hypridle final : public Writer {
public:
    QString target() const override { return QStringLiteral("hypridle"); }

    bool render(const Config& config, const QByteArray* existing, QByteArray* out, QString* error) const override
    {
        Q_UNUSED(existing);
        static const QStringList keys = {
            QStringLiteral("power.screensaver_after"),
            QStringLiteral("power.lock_after"),
            QStringLiteral("power.screens_off_after"),
            QStringLiteral("power.sleep_after"),
        };
        if (!config.require(keys, error))
            return false;

        struct Step {
            const char* key;
            const char* command;
            bool resume;   // screens off: turn them on again on input
        };
        static const Step steps[] = {
            {"power.screensaver_after", "hypr-screensaver", false},
            {"power.lock_after", "loginctl lock-session", false},
            {"power.screens_off_after", "hyprctl dispatch 'hl.dsp.dpms({action = \"off\"})'", true},
            {"power.sleep_after", "systemctl suspend", false},
        };

        QByteArray s;
        s += "# hypridle: written by Riley Settings (Power); do not edit by hand. Delete it to go back to "
             "hypridle.conf.\n";
        s += "general {\n";
        s += "    lock_cmd = pidof hyprlock || hyprlock\n";
        s += "    before_sleep_cmd = loginctl lock-session\n";
        s += QByteArray("    after_sleep_cmd = ") + kScreensOn + "\n";
        s += "}\n";
        for (const Step& step : steps) {
            const long long t = static_cast<long long>(config.number(QString::fromLatin1(step.key)));   // int()
            if (t <= 0)
                continue;
            s += "\nlistener {\n    timeout = " + QByteArray::number(t) + "\n    on-timeout = " + step.command + "\n";
            if (step.resume)
                s += QByteArray("    on-resume = ") + kScreensOn + "\n";
            s += "}\n";
        }
        *out = s;
        return true;
    }
};

}  // namespace

std::unique_ptr<Writer> makeHypridle()
{
    return std::make_unique<Hypridle>();
}

}  // namespace riley::config::writers
