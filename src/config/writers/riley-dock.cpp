// Writer for the target "riley-dock": the dock's own files (docs/pages-contract.md, "Dock").
//   - $XDG_CACHE_HOME/nwg-dock-pinned, whole: one app id per line, in dock order (dock.pins);
//   - $XDG_CONFIG_HOME/nwg-dock-hyprland/riley.conf, one KEY=value line per changed option, by Riley's
//     dock_core.py _set_config rule; every other byte kept, replaced by a rename.
// M3 scaffolding: not ready yet; every write is refused.
#include "writers.h"

namespace riley::config::writers {
namespace {

class RileyDock final : public Writer {
public:
    QString target() const override { return QStringLiteral("riley-dock"); }

    bool render(const Config&, const QByteArray*, QByteArray*, QString* error) const override
    {
        *error = QStringLiteral("riley-dock: writes its own files (pins and riley.conf), not one rendered file");
        return false;
    }

    bool writesOwnFiles() const override { return true; }

    bool writeFiles(const Config&, const WriteContext&, QString* error) const override
    {
        *error = QStringLiteral("riley-dock: not ready");
        return false;
    }
};

}  // namespace

std::unique_ptr<Writer> makeRileyDock()
{
    return std::make_unique<RileyDock>();
}

}  // namespace riley::config::writers
