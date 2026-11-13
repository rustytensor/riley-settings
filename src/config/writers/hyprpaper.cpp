// Writer for the target "hyprpaper": ~/.config/hypr/hyprpaper.conf, the whole file.
// Not implemented yet: it checks that the description holds the keys it reads, then refuses.
#include "writers.h"

namespace riley::config::writers {
namespace {

class Hyprpaper final : public Writer {
public:
    QString target() const override { return QStringLiteral("hyprpaper"); }

    bool render(const Config& config, const QByteArray* existing, QByteArray* out, QString* error) const override
    {
        Q_UNUSED(existing);
        Q_UNUSED(out);
        static const QStringList keys = {
            QStringLiteral("display.monitors"),
            QStringLiteral("wallpaper.picture"),
            QStringLiteral("wallpaper.screens"),
        };
        if (!config.require(keys, error))
            return false;
        *error = QStringLiteral("writer hyprpaper: not implemented");
        return false;
    }
};

}  // namespace

std::unique_ptr<Writer> makeHyprpaper()
{
    return std::make_unique<Hyprpaper>();
}

}  // namespace riley::config::writers
