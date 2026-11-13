#include "dock.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QTextStream>

#include <algorithm>

namespace riley::panel::pages {
namespace {

const QString ROLE_PINS = QStringLiteral("pins");
const QString ROLE_SCREENS = QStringLiteral("screens");

struct DesktopEntry {
    QString name;
    QString icon;
    bool shown = true;
};

// The [Desktop Entry] group of a .desktop file: Name, Icon, and whether it is shown (NoDisplay, Hidden).
bool readDesktopEntry(const QString& path, DesktopEntry* out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    bool inEntry = false;
    bool haveName = false;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            inEntry = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!inEntry)
            continue;
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        if (key == QLatin1String("Name")) {
            out->name = value;
            haveName = true;
        } else if (key == QLatin1String("Icon")) {
            out->icon = value;
        } else if ((key == QLatin1String("NoDisplay") || key == QLatin1String("Hidden"))
                   && value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
            out->shown = false;
        }
    }
    return haveName;
}

// Riley's dock_core.py move_pin: moved to `index` if pinned already (one place less when it moves right), else
// added there.
QStringList movePin(QStringList pins, const QString& app, int index)
{
    const qsizetype old = pins.indexOf(app);
    if (old >= 0) {
        pins.removeAt(old);
        if (old < index)
            --index;
    }
    pins.insert(std::clamp<qsizetype>(index, 0, pins.size()), app);
    return pins;
}

bool splitValue(const QString& value, QString* kind, QString* id)
{
    const qsizetype colon = value.indexOf(QLatin1Char(':'));
    if (colon < 0)
        return false;
    *kind = value.left(colon);
    *id = value.mid(colon + 1);
    return !id->isEmpty();
}

}  // namespace

DockBackend::DockBackend(QObject* parent) : PageBackend(parent)
{
    connect(this, &PageBackend::panelChanged, this, [this] { watchOptions(); readValues(); });
    connect(this, &PageBackend::pageChanged, this, [this] { watchOptions(); readValues(); });
}

OptionItem* DockBackend::option(const QString& role) const
{
    return page() ? page()->optionByRole(role) : nullptr;
}

void DockBackend::watchOptions()
{
    for (const QMetaObject::Connection& c : std::as_const(m_watch))
        disconnect(c);
    m_watch.clear();
    for (const QString& role : {ROLE_PINS, ROLE_SCREENS}) {
        if (OptionItem* o = option(role))
            m_watch.append(connect(o, &OptionItem::valueChanged, this, &DockBackend::readValues));
    }
}

// A one-field list option's values, in order.
QStringList DockBackend::listValue(const QString& role) const
{
    QStringList out;
    OptionItem* o = option(role);
    if (!o || !panel() || o->bound()->fields.isEmpty())
        return out;
    const QString field = o->bound()->fields.first().name;
    for (const QVariant& item : panel()->items(*o->bound()))
        out.append(item.toMap().value(field).toString());
    return out;
}

bool DockBackend::saveList(const QString& role, const QStringList& values)
{
    OptionItem* o = option(role);
    if (!o || !panel() || o->bound()->fields.isEmpty())
        return refuse((page() ? page()->title() : QStringLiteral("Dock")) + QStringLiteral(": no option with role ") + role);
    const QString field = o->bound()->fields.first().name;
    Change change;
    change.option = o->bound();
    for (const QString& v : values)
        change.items.append(config::ItemFields{{field, v}});
    const bool ok = panel()->commitStep({change});
    readValues();
    return ok;
}

void DockBackend::readValues()
{
    m_pins = listValue(ROLE_PINS);
    m_filters = listValue(ROLE_SCREENS);
    rebuildApps();
    rebuildScreens();
    Q_EMIT changed();
}

// Riley's all_apps: every app the launcher shows (not NoDisplay, not Hidden), by name ignoring case; the id is the
// file name without .desktop, the first file of an id winning ($XDG_DATA_HOME before $XDG_DATA_DIRS).
void DockBackend::readApps()
{
    m_appList.clear();
    if (!panel())
        return;
    QStringList dirs{panel()->paths().dataHome};
    dirs += panel()->paths().dataDirs;
    QSet<QString> seen;
    for (const QString& base : std::as_const(dirs)) {
        if (base.isEmpty())
            continue;
        const QDir dir(base + QStringLiteral("/applications"));
        for (const QString& file : dir.entryList({QStringLiteral("*.desktop")}, QDir::Files, QDir::Name)) {
            const QString id = file.chopped(int(sizeof(".desktop") - 1));
            if (seen.contains(id))
                continue;
            seen.insert(id);
            DesktopEntry e;
            if (!readDesktopEntry(dir.filePath(file), &e) || !e.shown)
                continue;
            m_appList.append({id, e.name, e.icon});
        }
    }
    std::stable_sort(m_appList.begin(), m_appList.end(),
                     [](const App& a, const App& b) { return a.name.toLower() < b.name.toLower(); });
}

void DockBackend::rebuildApps()
{
    m_appIds.clear();
    m_apps.clear();
    for (const App& a : std::as_const(m_appList)) {
        m_appIds.append(a.id);
        m_apps.append(QVariantMap{{QStringLiteral("id"), a.id},
                                  {QStringLiteral("name"), a.name},
                                  {QStringLiteral("icon"), a.icon},
                                  {QStringLiteral("pinned"), m_pins.contains(a.id)}});
    }
}

// Riley's connected_screens: the screens in use, sorted by y, x, port.
void DockBackend::readScreens()
{
    QString error;
    QList<Monitor> monitors;
    if (!hyprMonitors(false, &monitors, &error)) {
        m_monitors.clear();
        refuse(error);
        return;
    }
    std::stable_sort(monitors.begin(), monitors.end(), [](const Monitor& a, const Monitor& b) {
        if (a.y != b.y)
            return a.y < b.y;
        if (a.x != b.x)
            return a.x < b.x;
        return a.name < b.name;
    });
    m_monitors = monitors;
}

void DockBackend::rebuildScreens()
{
    m_screens.clear();
    for (const Monitor& m : std::as_const(m_monitors)) {
        bool on = false;
        for (const QString& part : std::as_const(m_filters))
            on |= !part.isEmpty() && m.desc.contains(part);
        m_screens.append(QVariantMap{{QStringLiteral("desc"), m.desc},
                                     {QStringLiteral("name"), m.name},
                                     {QStringLiteral("checked"), on}});
    }
}

bool DockBackend::refresh()
{
    readApps();
    readScreens();
    readValues();
    return true;
}

int DockBackend::insertIndex(double x) const
{
    int n = 0;
    for (qsizetype i = 0; i < m_pins.size(); ++i) {
        if (pinWidth() * (i + 0.5) < x)
            ++n;
    }
    return n;
}

bool DockBackend::dropOnDock(const QString& value, double x)
{
    QString kind, id;
    if (!splitValue(value, &kind, &id) || (kind != QLatin1String("pin") && kind != QLatin1String("app")))
        return refuse(QStringLiteral("Dock: only an app or a dock icon can be dropped on the dock"));
    int index = insertIndex(x);
#ifdef RILEY_PLANT_FAULT_PAGES
    ++index;   // planted fault (M3 gate): the drop lands one place too far right
#endif
    return saveList(ROLE_PINS, movePin(m_pins, id, index));
}

bool DockBackend::dropOnRemove(const QString& value)
{
    QString kind, id;
    if (!splitValue(value, &kind, &id) || kind != QLatin1String("pin"))
        return refuse(QStringLiteral("Dock: only a dock icon can be removed"));
    QStringList pins = m_pins;
    pins.removeAll(id);
    return saveList(ROLE_PINS, pins);
}

// Riley's set_screens: no commas or newlines; trimmed, empty ones and repeats left out.
bool DockBackend::setScreens(const QStringList& descs)
{
    QStringList values;
    for (QString v : descs) {
        if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('\r')) || v.contains(QLatin1Char('\n')))
            return refuse(QStringLiteral("Dock: screen descriptions cannot contain commas or newlines"));
        v = v.trimmed();
        if (!v.isEmpty() && !values.contains(v))
            values.append(v);
    }
    return saveList(ROLE_SCREENS, values);
}

// Riley's screen_changed: keep the choices for screens not in use, replace the ones that match a screen in use by
// the full descriptions of the boxes that are on.
bool DockBackend::setScreenChecked(const QString& desc, bool checked)
{
    QStringList filters;
    for (const QString& part : std::as_const(m_filters)) {
        bool matches = false;
        for (const Monitor& m : std::as_const(m_monitors))
            matches |= m.desc.contains(part);
        if (!matches)
            filters.append(part);
    }
    for (const QVariant& v : std::as_const(m_screens)) {
        const QVariantMap s = v.toMap();
        const QString d = s.value(QStringLiteral("desc")).toString();
        const bool on = d == desc ? checked : s.value(QStringLiteral("checked")).toBool();
        if (on && !filters.contains(d))
            filters.append(d);
    }
    return setScreens(filters);
}

}  // namespace riley::panel::pages
