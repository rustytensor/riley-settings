#include "themewallpaper.h"

namespace riley::panel::pages {

bool ThemeWallpaperBackend::refresh()
{
    return notReady();
}

QString ThemeWallpaperBackend::screenAt(double, double, double, double)
{
    notReady();
    return {};
}

bool ThemeWallpaperBackend::dropPicture(const QString&, const QString&)
{
    return notReady();
}

bool ThemeWallpaperBackend::useEverywhere(const QString&)
{
    return notReady();
}

}  // namespace riley::panel::pages
