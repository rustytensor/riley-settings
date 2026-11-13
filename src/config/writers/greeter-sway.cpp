// Writer for the target "greeter-sway": the login screen's sway file, the whole file.
// Not implemented yet: it checks that the description holds the keys it reads, then refuses.
#include "writers.h"

namespace riley::config::writers {
namespace {

class GreeterSway final : public Writer {
public:
    QString target() const override { return QStringLiteral("greeter-sway"); }

    bool render(const Config& config, const QByteArray* existing, QByteArray* out, QString* error) const override
    {
        Q_UNUSED(existing);
        Q_UNUSED(out);
        static const QStringList keys = {
            QStringLiteral("display.monitors"),
            QStringLiteral("input.kb_layout"),
        };
        if (!config.require(keys, error))
            return false;
        *error = QStringLiteral("writer greeter-sway: not implemented");
        return false;
    }
};

}  // namespace

std::unique_ptr<Writer> makeGreeterSway()
{
    return std::make_unique<GreeterSway>();
}

}  // namespace riley::config::writers
