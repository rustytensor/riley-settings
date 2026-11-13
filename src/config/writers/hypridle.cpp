// Writer for the target "hypridle": ~/.config/hypr/idle.conf, the whole file.
// Not implemented yet: it checks that the description holds the keys it reads, then refuses.
#include "writers.h"

namespace riley::config::writers {
namespace {

class Hypridle final : public Writer {
public:
    QString target() const override { return QStringLiteral("hypridle"); }

    bool render(const Config& config, const QByteArray* existing, QByteArray* out, QString* error) const override
    {
        Q_UNUSED(existing);
        Q_UNUSED(out);
        static const QStringList keys = {
            QStringLiteral("power.screensaver_after"),
            QStringLiteral("power.lock_after"),
            QStringLiteral("power.screens_off_after"),
            QStringLiteral("power.sleep_after"),
        };
        if (!config.require(keys, error))
            return false;
        *error = QStringLiteral("writer hypridle: not implemented");
        return false;
    }
};

}  // namespace

std::unique_ptr<Writer> makeHypridle()
{
    return std::make_unique<Hypridle>();
}

}  // namespace riley::config::writers
