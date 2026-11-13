// Writer for the target "riley-desktop": $XDG_CONFIG_HOME/riley/desktop.json in the format of Riley's
// desktop/riley_desktop/layout.py (docs/pages-contract.md, "Desktop"). The layout rules live in desktop-layout.h.
// M3 scaffolding: not ready yet; every write is refused.
#include "desktop-layout.h"
#include "writers.h"

namespace riley::config::writers {
namespace {

class RileyDesktop final : public Writer {
public:
    QString target() const override { return QStringLiteral("riley-desktop"); }

    bool render(const Config&, const QByteArray*, QByteArray*, QString* error) const override
    {
        *error = QStringLiteral("riley-desktop: writes its own file (desktop.json), not one rendered file");
        return false;
    }

    bool writesOwnFiles() const override { return true; }

    bool writeFiles(const Config&, const WriteContext&, QString* error) const override
    {
        *error = QStringLiteral("riley-desktop: not ready");
        return false;
    }
};

}  // namespace

std::unique_ptr<Writer> makeRileyDesktop()
{
    return std::make_unique<RileyDesktop>();
}

}  // namespace riley::config::writers
