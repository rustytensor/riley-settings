// Writer for the target "riley-desktop": $XDG_CONFIG_HOME/riley/desktop.json in the format of Riley's
// desktop/riley_desktop/layout.py (docs/pages-contract.md, "Desktop"). The layout rules live in desktop-layout.h.
//
// desktop.json is read fresh (the desktop may have moved a box since), given boxes for new folders of ~/Desktop when
// the screens are known (the Desktop page passes them), brought in line with desktop.boxes, and given desktop.show and
// desktop.icon_size ("show" and "icon_size").
#include "desktop-layout.h"
#include "writers.h"

#include "../atomic_file.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cmath>

namespace riley::config::writers {
namespace {

const QString SHOW = QStringLiteral("desktop.show");
const QString BOXES = QStringLiteral("desktop.boxes");
const QString ICON_SIZE = QStringLiteral("desktop.icon_size");

class RileyDesktop final : public Writer {
public:
    QString target() const override { return QStringLiteral("riley-desktop"); }

    bool render(const Config&, const QByteArray*, QByteArray*, QString* error) const override
    {
        *error = QStringLiteral("riley-desktop: writes its own file (desktop.json), not one rendered file");
        return false;
    }

    bool writesOwnFiles() const override { return true; }

    bool writeFiles(const Config& config, const WriteContext& context, QString* error) const override
    {
        if (!config.require({SHOW, BOXES, ICON_SIZE}, error))
            return false;
        const QString path = context.configHome + QStringLiteral("/riley/desktop.json");
        QJsonObject cfg = desktop::load(path);
        const desktop::Screens screens = desktop::Screens::fromPageData(context.pageData);
        desktop::resolve(&cfg, context.home + QStringLiteral("/Desktop"), screens);

        QList<desktop::ValueBox> boxes;
        for (const Item& item : config.list(BOXES))
            boxes.append({item.text(QStringLiteral("folder")), item.text(QStringLiteral("screen")),
                          item.flag(QStringLiteral("hidden"))});
        desktop::applyValueBoxes(&cfg, boxes, screens);

        cfg.insert(QStringLiteral("show"), config.flag(SHOW));
        cfg.insert(QStringLiteral("icon_size"), int(std::lround(config.number(ICON_SIZE))));

        QDir().mkpath(QFileInfo(path).absolutePath());
        return writeTargetFile(path, desktop::toText(cfg), error);
    }
};

}  // namespace

std::unique_ptr<Writer> makeRileyDesktop()
{
    return std::make_unique<RileyDesktop>();
}

}  // namespace riley::config::writers
