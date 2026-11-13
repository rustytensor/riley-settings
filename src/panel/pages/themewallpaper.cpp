#include "themewallpaper.h"

#include <QFileInfo>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace riley::panel::pages {
namespace {

const QString ROLE_PICTURE = QStringLiteral("picture");
const QString ROLE_SCREENS = QStringLiteral("screen-pictures");

// Python's round() on a float: halves to even (std::nearbyint in the default rounding mode).
int pyRound(double v)
{
    return static_cast<int>(std::nearbyint(v));
}

bool isPicture(const QString& path)
{
    static const QStringList suffixes{QStringLiteral(".jpg"), QStringLiteral(".jpeg"), QStringLiteral(".png"),
                                      QStringLiteral(".webp"), QStringLiteral(".avif"), QStringLiteral(".jxl")};
    const QString name = QFileInfo(path).fileName().toLower();
    for (const QString& s : suffixes) {
        if (name.endsWith(s) && name.size() > s.size())
            return true;
    }
    return false;
}

}  // namespace

ThemeWallpaperBackend::ThemeWallpaperBackend(QObject* parent) : PageBackend(parent)
{
    const auto rewatch = [this] {
        for (const QMetaObject::Connection& c : std::as_const(m_watch))
            disconnect(c);
        m_watch.clear();
        for (const QString& role : {ROLE_PICTURE, ROLE_SCREENS}) {
            if (OptionItem* o = option(role))
                m_watch.append(connect(o, &OptionItem::valueChanged, this, &ThemeWallpaperBackend::readValues));
        }
        readValues();
    };
    connect(this, &PageBackend::panelChanged, this, rewatch);
    connect(this, &PageBackend::pageChanged, this, rewatch);
}

OptionItem* ThemeWallpaperBackend::option(const QString& role) const
{
    return page() ? page()->optionByRole(role) : nullptr;
}

bool ThemeWallpaperBackend::screenFields(QString* screenField, QString* pathField) const
{
    OptionItem* o = option(ROLE_SCREENS);
    if (!o)
        return false;
    screenField->clear();
    pathField->clear();
    for (const config::Field& f : o->bound()->fields) {
        if (f.type == config::OptionType::File)
            *pathField = f.name;
        else if (screenField->isEmpty())
            *screenField = f.name;
    }
    return !screenField->isEmpty() && !pathField->isEmpty();
}

void ThemeWallpaperBackend::readValues()
{
    QString every;
    if (OptionItem* p = option(ROLE_PICTURE))
        every = p->valueText();
    QHash<QString, QString> own;
    QString screenField, pathField;
    OptionItem* s = option(ROLE_SCREENS);
    if (s && panel() && screenFields(&screenField, &pathField)) {
        for (const QVariant& v : panel()->items(*s->bound())) {
            const QVariantMap item = v.toMap();
            own.insert(item.value(screenField).toString(), item.value(pathField).toString());
        }
    }
    m_pictures.clear();
    if (!every.isEmpty())
        m_pictures.append(every);
    for (const QString& path : std::as_const(own)) {
        if (!path.isEmpty() && !m_pictures.contains(path))
            m_pictures.append(path);
    }
    std::sort(m_pictures.begin() + (every.isEmpty() ? 0 : 1), m_pictures.end());
    m_screens.clear();
    for (const Monitor& m : std::as_const(m_monitors)) {
        const QString model = m.desc.section(QLatin1Char(' '), -2, -2);
        m_screens.append(QVariantMap{{QStringLiteral("desc"), m.desc},
                                     {QStringLiteral("name"), m.name},
                                     {QStringLiteral("model"), model.isEmpty() ? m.desc : model},
                                     {QStringLiteral("picture"), own.value(m.desc, every)}});
    }
    Q_EMIT changed();
}

bool ThemeWallpaperBackend::refresh()
{
    QString error;
    QList<Monitor> monitors;
    m_haveScreens = hyprMonitors(false, &monitors, &error);
    m_monitors.clear();
    if (m_haveScreens) {
        for (const Monitor& m : std::as_const(monitors)) {
            if (m.enabled)
                m_monitors.append(m);
        }
    }
    readValues();
    return m_haveScreens || refuse(error);
}

bool ThemeWallpaperBackend::ensureScreens()
{
    return m_haveScreens || refresh();
}

// Riley's core.py layout_rects (margin 20) with logical_size: the mode's size, turned for an odd transform, divided
// by the scale and rounded.
QList<ThemeWallpaperBackend::Rect> ThemeWallpaperBackend::layout(double width, double height) const
{
    QList<Rect> out;
    if (m_monitors.isEmpty())
        return out;
    struct Size {
        int w, h;
    };
    QList<Size> sizes;
    for (const Monitor& m : m_monitors) {
        int w = m.width, h = m.height;
        if (m.transform % 2)
            std::swap(w, h);
        const double scale = m.scale > 0 ? m.scale : 1;
        sizes.append({pyRound(w / scale), pyRound(h / scale)});
    }
    int x0 = m_monitors.first().x, y0 = m_monitors.first().y;
    int x1 = x0 + sizes.first().w, y1 = y0 + sizes.first().h;
    for (qsizetype i = 0; i < m_monitors.size(); ++i) {
        x0 = std::min(x0, m_monitors[i].x);
        y0 = std::min(y0, m_monitors[i].y);
        x1 = std::max(x1, m_monitors[i].x + sizes[i].w);
        y1 = std::max(y1, m_monitors[i].y + sizes[i].h);
    }
    const double margin = 20;
    const double s = std::min((width - 2 * margin) / std::max(1, x1 - x0), (height - 2 * margin) / std::max(1, y1 - y0));
    const double ox = (width - (x1 - x0) * s) / 2 - x0 * s;
    const double oy = (height - (y1 - y0) * s) / 2 - y0 * s;
    for (qsizetype i = 0; i < m_monitors.size(); ++i)
        out.append({m_monitors[i].desc, ox + m_monitors[i].x * s, oy + m_monitors[i].y * s, sizes[i].w * s, sizes[i].h * s});
    return out;
}

QVariantList ThemeWallpaperBackend::rects(double width, double height)
{
    QVariantList out;   // the screens read when the page was shown (refresh); drawing runs no command
    const QList<Rect> r = layout(width, height);
    for (qsizetype i = 0; i < r.size(); ++i) {
        QVariantMap m = i < m_screens.size() ? m_screens[i].toMap() : QVariantMap();
        m.insert(QStringLiteral("desc"), r[i].desc);
        m.insert(QStringLiteral("x"), r[i].x);
        m.insert(QStringLiteral("y"), r[i].y);
        m.insert(QStringLiteral("w"), r[i].w);
        m.insert(QStringLiteral("h"), r[i].h);
        m.insert(QStringLiteral("number"), int(i + 1));
        out.append(m);
    }
    return out;
}

QString ThemeWallpaperBackend::screenAt(double x, double y, double width, double height)
{
    ensureScreens();
    const QList<Rect> r = layout(width, height);
    for (qsizetype i = r.size() - 1; i >= 0; --i) {
        const Rect& q = r[i];
        if (q.x <= x && x <= q.x + q.w && q.y <= y && y <= q.y + q.h) {
#ifdef RILEY_PLANT_FAULT_PAGES
            // planted fault (M3 gate): the screen after the one under the point
            return i + 1 < r.size() ? r[i + 1].desc : QString();
#else
            return q.desc;
#endif
        }
    }
    return QString();
}

bool ThemeWallpaperBackend::dropPicture(const QString& path, const QString& desc)
{
    if (desc.isEmpty())
        return refuse(QStringLiteral("Wallpaper: drop the picture on a screen"));
    if (!isPicture(path))
        return refuse(QStringLiteral("That is not a picture (.jpg, .jpeg, .png, .webp, .avif or .jxl)"));
    OptionItem* s = option(ROLE_SCREENS);
    QString screenField, pathField;
    if (!s || !panel() || !screenFields(&screenField, &pathField))
        return refuse(QStringLiteral("Wallpaper: the page has no option for one screen's picture"));
    Change change;
    change.option = s->bound();
    bool replaced = false;
    for (const QVariant& v : panel()->items(*s->bound())) {
        const QVariantMap item = v.toMap();
        config::ItemFields fields;
        for (auto it = item.cbegin(); it != item.cend(); ++it)
            fields.insert(it.key(), it.value().toString());
        if (!replaced && fields.value(screenField) == desc) {
            fields.insert(pathField, path);
            replaced = true;
        }
        change.items.append(fields);
    }
    if (!replaced)
        change.items.append(config::ItemFields{{screenField, desc}, {pathField, path}});
    return panel()->commitStep({change});
}

bool ThemeWallpaperBackend::useEverywhere(const QString& path)
{
    if (!isPicture(path))
        return refuse(QStringLiteral("That is not a picture (.jpg, .jpeg, .png, .webp, .avif or .jxl)"));
    OptionItem* p = option(ROLE_PICTURE);
    OptionItem* s = option(ROLE_SCREENS);
    if (!p || !s || !panel())
        return refuse(QStringLiteral("Wallpaper: the page has no option for the picture"));
    Change picture;
    picture.option = p->bound();
    picture.text = path;
    Change screens;
    screens.option = s->bound();
    return panel()->commitStep({picture, screens});
}

QString ThemeWallpaperBackend::localPath(const QVariant& urlOrPath) const
{
    const QUrl url = urlOrPath.toUrl();
    if (url.isLocalFile())
        return url.toLocalFile();
    const QString text = urlOrPath.toString();
    const QUrl parsed(text);
    return parsed.isLocalFile() ? parsed.toLocalFile() : text;
}

QUrl ThemeWallpaperBackend::pictureUrl(const QString& path) const
{
    return !path.isEmpty() && QFileInfo(path).isFile() ? QUrl::fromLocalFile(path) : QUrl();
}

}  // namespace riley::panel::pages
