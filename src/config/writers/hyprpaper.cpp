// Writer for the target "hyprpaper": ~/.config/hypr/hyprpaper.conf, the whole file.
// Port of Riley's bin/hypr-theme.py set_bg: with no per-screen picture, one catch-all block with wallpaper.picture;
// otherwise one block per enabled screen of display.monitors, in list order, each with its picture from
// wallpaper.screens or else wallpaper.picture (a later entry for the same screen wins, as in the Python dict).
// With neither set, the file is left as it is (the theme's).
#include "writers.h"

#include <QHash>

namespace riley::config::writers {
namespace {

class Hyprpaper final : public Writer {
public:
    QString target() const override { return QStringLiteral("hyprpaper"); }

    // No picture and no screen of its own: the theme decides the wallpapers (hypr-theme.py set_wallpapers), so its
    // hyprpaper.conf is left as it is (docs/switch-contract.md).
    bool leavesFileAlone(const Config& config) const override
    {
        return config.text(QStringLiteral("wallpaper.picture")).isEmpty()
            && config.list(QStringLiteral("wallpaper.screens")).isEmpty();
    }

    bool render(const Config& config, const QByteArray* existing, QByteArray* out, QString* error) const override
    {
        Q_UNUSED(existing);
        static const QStringList keys = {
            QStringLiteral("display.monitors"),
            QStringLiteral("wallpaper.picture"),
            QStringLiteral("wallpaper.screens"),
        };
        if (!config.require(keys, error))
            return false;

        const QString picture = config.text(QStringLiteral("wallpaper.picture"));
        QHash<QString, QString> perScreen;
        for (const Item& item : config.list(QStringLiteral("wallpaper.screens")))
            perScreen.insert(item.text(QStringLiteral("screen")), item.text(QStringLiteral("path")));

        QByteArray s = "# Written by hypr-theme.py (wallpaper picked in Riley Settings).\nsplash = false\nipc = true\n\n";
        if (perScreen.isEmpty()) {
            s += "wallpaper {\n    monitor =\n    path = " + picture.toUtf8() + "\n    fit_mode = cover\n}\n";
        } else {
            for (const Item& m : config.list(QStringLiteral("display.monitors"))) {
                if (!m.flag(QStringLiteral("enabled")))
                    continue;
                const QString desc = m.text(QStringLiteral("desc"));
                s += "wallpaper {\n    monitor = desc:" + desc.toUtf8() + "\n    path = "
                    + perScreen.value(desc, picture).toUtf8() + "\n    fit_mode = cover\n}\n";
            }
        }
        *out = s;
        return true;
    }
};

}  // namespace

std::unique_ptr<Writer> makeHyprpaper()
{
    return std::make_unique<Hyprpaper>();
}

}  // namespace riley::config::writers
