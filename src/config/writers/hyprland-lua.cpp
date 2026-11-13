// Writer for the target "hyprland-lua": ~/.config/hypr/settings.lua, the whole file.
// Not implemented yet: it checks that the description holds the keys it reads, then refuses.
#include "writers.h"

namespace riley::config::writers {
namespace {

class HyprlandLua final : public Writer {
public:
    QString target() const override { return QStringLiteral("hyprland-lua"); }

    bool render(const Config& config, const QByteArray* existing, QByteArray* out, QString* error) const override
    {
        Q_UNUSED(existing);
        Q_UNUSED(out);
        static const QStringList keys = {
            QStringLiteral("display.monitors"),
            QStringLiteral("appearance.gaps_in"),
            QStringLiteral("appearance.gaps_out"),
            QStringLiteral("appearance.border_size"),
            QStringLiteral("appearance.rounding"),
            QStringLiteral("appearance.active_opacity"),
            QStringLiteral("appearance.inactive_opacity"),
            QStringLiteral("appearance.blur"),
            QStringLiteral("appearance.blur_size"),
            QStringLiteral("appearance.blur_passes"),
            QStringLiteral("appearance.shadow"),
            QStringLiteral("appearance.animations"),
            QStringLiteral("input.kb_layout"),
            QStringLiteral("input.kb_variant"),
            QStringLiteral("input.kb_options"),
            QStringLiteral("input.repeat_rate"),
            QStringLiteral("input.repeat_delay"),
            QStringLiteral("input.sensitivity"),
            QStringLiteral("input.accel_profile"),
            QStringLiteral("input.natural_scroll"),
            QStringLiteral("input.follow_mouse"),
            QStringLiteral("startup.apps"),
        };
        if (!config.require(keys, error))
            return false;
        *error = QStringLiteral("writer hyprland-lua: not implemented");
        return false;
    }
};

}  // namespace

std::unique_ptr<Writer> makeHyprlandLua()
{
    return std::make_unique<HyprlandLua>();
}

}  // namespace riley::config::writers
