#include "desktop.h"

#include <QFileInfo>
#include <QJsonArray>

#include <cmath>
#include <utility>

namespace riley::panel::pages {

namespace layout = riley::config::desktop;

QString DesktopBackend::jsonPath() const
{
    return panel() ? panel()->paths().configHome + QStringLiteral("/riley/desktop.json") : QString();
}

QString DesktopBackend::desktopDir() const
{
    return panel() ? panel()->paths().home + QStringLiteral("/Desktop") : QString();
}

QString DesktopBackend::localPath(const QUrl& url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

// Riley's desktop_page.screens(): the enabled screens' logical sizes (core.logical_size: rotated when the transform
// is odd, divided by the scale, rounded half to even as Python's round), the main one by its serial.
bool DesktopBackend::readScreens()
{
    QList<Monitor> monitors;
    QString error;
    if (!hyprMonitors(true, &monitors, &error))
        return refuse(QStringLiteral("Desktop: ") + error);
    layout::Screens screens;
    QStringList descs;
    for (const Monitor& m : std::as_const(monitors)) {
        if (!m.enabled || m.scale <= 0)
            continue;
        int w = m.width, h = m.height;
        if (m.transform % 2)
            std::swap(w, h);
        screens.sizes.insert(m.desc, QSize(int(std::nearbyint(w / m.scale)), int(std::nearbyint(h / m.scale))));
        descs.append(m.desc);
    }
    screens.main = layout::mainOf(descs);
    m_screens = screens;
    if (!m_screens.known())
        return refuse(QStringLiteral("Desktop: Hyprland reports no screen in use"));
    return true;
}

void DesktopBackend::watchOptions()
{
    if (m_watching || !page())
        return;
    m_watching = true;
    // A change of an M2 control of the page (the switch, the icon size) is written after the controls refresh:
    // look again once it is done.
    for (QObject* o : page()->options()) {
        if (auto* item = qobject_cast<OptionItem*>(o))
            connect(item, &OptionItem::valueChanged, this, &DesktopBackend::recompute, Qt::QueuedConnection);
    }
}

void DesktopBackend::recompute()
{
    if (!panel())
        return;
    QJsonObject cfg = layout::load(jsonPath());
    const QList<layout::Shown> shown = layout::resolve(&cfg, desktopDir(), m_screens);
    QStringList shownText;
    for (const layout::Shown& s : shown)
        shownText.append(QStringLiteral("%1 %2 %3 %4 %5").arg(s.title).arg(s.x).arg(s.y).arg(s.w).arg(s.h));

    const QString desk = layout::cleanFolder(desktopDir());
    QVariantList boxes;
    for (const QJsonValue& v : cfg.value(QStringLiteral("boxes")).toArray()) {
        const QJsonObject b = v.toObject();
        const QString folder = b.value(QStringLiteral("folder")).toString();
        QVariantMap row;
        row.insert(QStringLiteral("folder"), folder);
        row.insert(QStringLiteral("title"), folder == desk ? QStringLiteral("Desktop") : QFileInfo(folder).fileName());
        row.insert(QStringLiteral("hidden"), b.value(QStringLiteral("hidden")).toBool(false));
        row.insert(QStringLiteral("auto"), b.value(QStringLiteral("auto")).toBool(false));
        row.insert(QStringLiteral("missing"), !QFileInfo(folder).isDir());
        row.insert(QStringLiteral("onScreen"), m_screens.sizes.contains(b.value(QStringLiteral("screen")).toString()));
        boxes.append(row);
    }
    if (shownText == m_shown && boxes == m_boxes)
        return;
    m_shown = shownText;
    m_boxes = boxes;
    Q_EMIT changed();
}

bool DesktopBackend::reload()
{
    if (!panel() || !page())
        return refuse(QStringLiteral("Desktop: the page is not set up"));
    watchOptions();
    const bool ok = readScreens();
    recompute();
    return ok;
}

bool DesktopBackend::step(const std::function<bool(QJsonObject*)>& change, const QString& refusal)
{
    if (!panel() || !page())
        return refuse(QStringLiteral("Desktop: the page is not set up"));
    watchOptions();
    if (!m_screens.known() && !readScreens())
        return false;
    OptionItem* boxesItem = page()->optionByRole(QStringLiteral("boxes"));
    if (!boxesItem)
        return refuse(QStringLiteral("Desktop: the description has no option with role \"boxes\""));

    QJsonObject cfg = layout::load(jsonPath());
    layout::resolve(&cfg, desktopDir(), m_screens);   // boxes for folders made since the desktop last looked
    if (!change(&cfg))
        return refuse(refusal);

    Change c;
    c.option = boxesItem->shown();
    for (const layout::ValueBox& vb : layout::valueBoxes(cfg)) {
        config::ItemFields item;
        item.insert(QStringLiteral("folder"), vb.folder);
        item.insert(QStringLiteral("screen"), vb.screen);
        item.insert(QStringLiteral("hidden"), vb.hidden ? QStringLiteral("true") : QStringLiteral("false"));
        c.items.append(item);
    }
    const bool ok = panel()->commitStep({c}, m_screens.toPageData());
    recompute();
    return ok;
}

bool DesktopBackend::addFolder(const QString& path)
{
    const QString folder = layout::cleanFolder(path);
    if (path.isEmpty() || !QFileInfo(folder).isDir())
        return refuse(QStringLiteral("Desktop: %1 is not a folder").arg(path));
    return step([&](QJsonObject* cfg) { return layout::addFolder(cfg, folder, m_screens); },
                QStringLiteral("Desktop: %1 already has a box").arg(QFileInfo(folder).fileName()));
}

bool DesktopBackend::setHidden(const QString& folder, bool hidden)
{
    return step([&](QJsonObject* cfg) { return layout::setHidden(cfg, folder, hidden); },
                QStringLiteral("Desktop: %1 has no box").arg(folder));
}

bool DesktopBackend::removeBox(const QString& folder)
{
    return step([&](QJsonObject* cfg) { return layout::remove(cfg, folder); },
                QStringLiteral("Desktop: the box of %1 comes with its folder in ~/Desktop; it can only be hidden")
                    .arg(QFileInfo(folder).fileName()));
}

}  // namespace riley::panel::pages
