#include "displays.h"

#include "config/atomic_file.h"
#include "config/config.h"
#include "config/values.h"
#include "config/writer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

#include <cmath>
#include <limits>

namespace riley::panel::pages {

namespace {

// Riley's core.py SCALES: what the scale box offers.
constexpr double kScales[] = {1.0, 1.25, 1.5, 1.75, 2.0};
// Riley's displays.py SNAP: a dragged screen snaps to a neighbour's edge nearer than this (logical pixels).
constexpr double kSnap = 80;

const QString kDesc = QStringLiteral("desc");
const QString kEnabled = QStringLiteral("enabled");
const QString kMode = QStringLiteral("mode");
const QString kX = QStringLiteral("x");
const QString kY = QStringLiteral("y");
const QString kScale = QStringLiteral("scale");
const QString kTransform = QStringLiteral("transform");

bool isOn(const QString& text)
{
    return text.trimmed() == QLatin1String("true");
}

bool readInt(const QString& text, int* out)
{
    double v = 0;
    if (!config::parseNumber(text.trimmed(), &v) || !std::isfinite(v))
        return false;
    *out = int(std::lround(v));
    return true;
}

std::optional<QByteArray> readFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;
    return f.readAll();
}

}  // namespace

DisplaysBackend::DisplaysBackend(QObject* parent) : PageBackend(parent)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &DisplaysBackend::tick);
}

const config::Option* DisplaysBackend::option() const
{
    if (!page())
        return nullptr;
    OptionItem* item = page()->optionByRole(QStringLiteral("screens"));
    if (!item && !page()->options().isEmpty())
        item = qobject_cast<OptionItem*>(page()->options().first());
    return item ? item->shown() : nullptr;
}

QStringList DisplaysBackend::scales() const
{
    QStringList out;
    for (double s : kScales)
        out.append(config::formatNumber(s));
    return out;
}

bool DisplaysBackend::logicalSize(const Screen& s, int* w, int* h) const
{
    // core.py logical_size: the mode's size, swapped when rotated a quarter turn, divided by the scale, rounded.
    const QString res = s.fields.value(kMode).section(QLatin1Char('@'), 0, 0);
    const QStringList wh = res.split(QLatin1Char('x'));
    if (wh.size() != 2)
        return false;
    bool okW = false, okH = false;
    int mw = wh[0].trimmed().toInt(&okW), mh = wh[1].trimmed().toInt(&okH);
    double scale = 0;
    int transform = 0;
    if (!okW || !okH || !config::parseNumber(s.fields.value(kScale).trimmed(), &scale) || scale <= 0
        || !readInt(s.fields.value(kTransform), &transform))
        return false;
    if (transform % 2)
        std::swap(mw, mh);
    *w = int(std::lround(mw / scale));
    *h = int(std::lround(mh / scale));
    return true;
}

QVariantList DisplaysBackend::screens() const
{
    QVariantList out;
    for (const Screen& s : m_list) {
        QVariantMap m;
        for (auto it = s.fields.cbegin(); it != s.fields.cend(); ++it)
            m.insert(it.key(), it.value());
        int w = 0, h = 0;
        if (!logicalSize(s, &w, &h))
            w = h = 0;
        int x = 0, y = 0;
        readInt(s.fields.value(kX), &x);
        readInt(s.fields.value(kY), &y);
        m.insert(QStringLiteral("px"), x);
        m.insert(QStringLiteral("py"), y);
        m.insert(QStringLiteral("w"), w);
        m.insert(QStringLiteral("h"), h);
        m.insert(QStringLiteral("modes"), s.modes);
        m.insert(QStringLiteral("shown"), isOn(s.fields.value(kEnabled)));
        out.append(m);
    }
    return out;
}

QVariantMap DisplaysBackend::screen(const QString& desc) const
{
    const int i = indexOf(desc);
    return i < 0 ? QVariantMap() : screens().at(i).toMap();
}

int DisplaysBackend::indexOf(const QString& desc) const
{
    for (int i = 0; i < m_list.size(); ++i) {
        if (m_list[i].fields.value(kDesc) == desc)
            return i;
    }
    return -1;
}

QList<config::ItemFields> DisplaysBackend::items() const
{
    QList<config::ItemFields> out;
    for (const Screen& s : m_list)
        out.append(s.fields);
    return out;
}

void DisplaysBackend::updateDerived()
{
    // dirty: the screens being edited differ from the values file (always, while the key is not set there).
    bool dirty = true;
    const config::Option* opt = option();
    if (panel() && opt && panel()->values().value(opt->key)) {
        const QVariantList saved = panel()->items(*opt);
        QVariantList edited;
        for (const Screen& s : m_list) {
            QVariantMap m;
            for (const config::Field& f : opt->fields)
                m.insert(f.name, s.fields.contains(f.name) ? s.fields.value(f.name) : f.defaultValue.value_or(QString()));
            edited.append(m);
        }
        dirty = saved != edited;
    }
    m_dirty = dirty;

    QStringList shown, all;
    for (const Screen& s : m_list) {
        all.append(s.fields.value(kDesc));
        if (isOn(s.fields.value(kEnabled)))
            shown.append(s.fields.value(kDesc));
    }
    const bool descs = shown != m_shownDescs || all != m_allDescs;
    m_shownDescs = shown;
    m_allDescs = all;
    if (descs)
        Q_EMIT descsChanged();
    Q_EMIT changed();
}

bool DisplaysBackend::load()
{
    const config::Option* opt = option();
    if (!panel() || !opt)
        return refuse(QStringLiteral("Displays: the page has no list of screens in the description"));
    m_dragDesc.clear();

    // The live screens: their modes are offered in the controls, and they are the list when none is saved.
    QList<Monitor> live;
    QString error;
    const bool haveLive = hyprMonitors(true, &live, &error);

    QList<Screen> list;
    if (panel()->values().value(opt->key)) {
        for (const QVariant& v : panel()->items(*opt)) {
            const QVariantMap m = v.toMap();
            Screen s;
            for (auto it = m.cbegin(); it != m.cend(); ++it)
                s.fields.insert(it.key(), it.value().toString());
            list.append(s);
        }
    } else if (haveLive) {
        for (const Monitor& m : std::as_const(live)) {
            Screen s;
            s.fields.insert(kDesc, m.desc);
            s.fields.insert(kEnabled, m.enabled ? QStringLiteral("true") : QStringLiteral("false"));
            s.fields.insert(kMode, m.mode());
            s.fields.insert(kX, QString::number(m.x));
            s.fields.insert(kY, QString::number(m.y));
            s.fields.insert(kScale, config::formatNumber(std::round(m.scale * 100) / 100));
            s.fields.insert(kTransform, QString::number(m.transform));
            list.append(s);
        }
    }
    for (Screen& s : list) {
        for (const Monitor& m : std::as_const(live)) {
            if (m.desc == s.fields.value(kDesc)) {
                for (const QString& mode : m.modes)
                    s.modes.append(mode.endsWith(QLatin1String("Hz")) ? mode.chopped(2) : mode);
            }
        }
    }
    m_list = list;
    m_loaded = true;
    updateDerived();
    if (!haveLive && !panel()->values().value(opt->key))
        return refuse(QStringLiteral("Displays: ") + error);
    return true;
}

bool DisplaysBackend::reset()
{
    // Riley's Reset: back to what the values file says.
    return load();
}

bool DisplaysBackend::dragMove(const QString& desc, double dx, double dy)
{
    const int i = indexOf(desc);
    if (i < 0)
        return refuse(QStringLiteral("Displays: no screen ") + desc);
    Screen& m = m_list[i];
    if (!isOn(m.fields.value(kEnabled)))
        return refuse(QStringLiteral("Displays: ") + desc + QStringLiteral(" is off"));
    int w = 0, h = 0;
    if (!logicalSize(m, &w, &h))
        return refuse(QStringLiteral("Displays: ") + desc + QStringLiteral(": its mode or scale is not valid"));
    if (m_dragDesc != desc) {   // a new drag: remember where the screen was when it began
        int x = 0, y = 0;
        readInt(m.fields.value(kX), &x);
        readInt(m.fields.value(kY), &y);
        m_dragDesc = desc;
        m_dragX = x;
        m_dragY = y;
    }
    // displays.py on_update: snap x and y separately to the nearest edge candidate of each other shown screen.
    const double nx = m_dragX + dx, ny = m_dragY + dy;
    double bestX = kSnap, bestY = kSnap;
    std::optional<double> snapX, snapY;
    for (int j = 0; j < m_list.size(); ++j) {
        const Screen& o = m_list[j];
        if (j == i || !isOn(o.fields.value(kEnabled)))
            continue;
        int ow = 0, oh = 0, ox = 0, oy = 0;
        if (!logicalSize(o, &ow, &oh) || !readInt(o.fields.value(kX), &ox) || !readInt(o.fields.value(kY), &oy))
            continue;
        for (double cx : {double(ox - w), double(ox + ow), double(ox), double(ox + ow - w)}) {
            if (std::abs(cx - nx) < bestX) {
                bestX = std::abs(cx - nx);
                snapX = cx;
            }
        }
        for (double cy : {double(oy - h), double(oy + oh), double(oy), double(oy + oh - h)}) {
            if (std::abs(cy - ny) < bestY) {
                bestY = std::abs(cy - ny);
                snapY = cy;
            }
        }
    }
    // Python's round(): halves to even.
    m.fields.insert(kX, QString::number(qint64(std::nearbyint(snapX.value_or(nx)))));
    m.fields.insert(kY, QString::number(qint64(std::nearbyint(snapY.value_or(ny)))));
    updateDerived();
    return true;
}

bool DisplaysBackend::dragEnd(const QString& desc)
{
    const int i = indexOf(desc);
    if (i < 0)
        return refuse(QStringLiteral("Displays: no screen ") + desc);
    if (m_dragDesc != desc)
        return true;   // no drag of that screen in progress
    m_dragDesc.clear();
    Screen& m = m_list[i];
    int mx = 0, my = 0, mw = 0, mh = 0;
    readInt(m.fields.value(kX), &mx);
    readInt(m.fields.value(kY), &my);
    logicalSize(m, &mw, &mh);
    // displays.py rects_overlap: screens may touch, never overlap.
    for (int j = 0; j < m_list.size(); ++j) {
        const Screen& o = m_list[j];
        if (j == i || !isOn(o.fields.value(kEnabled)))
            continue;
        int ox = 0, oy = 0, ow = 0, oh = 0;
        if (!logicalSize(o, &ow, &oh) || !readInt(o.fields.value(kX), &ox) || !readInt(o.fields.value(kY), &oy))
            continue;
        if (mx < ox + ow && ox < mx + mw && my < oy + oh && oy < my + mh) {
            m.fields.insert(kX, QString::number(m_dragX));
            m.fields.insert(kY, QString::number(m_dragY));
            updateDerived();
            return refuse(QStringLiteral("Screens can't overlap: moved it back"));
        }
    }
    // DisplaysPage.normalise: shift every screen so the shown ones start at 0, 0.
    int x0 = std::numeric_limits<int>::max(), y0 = std::numeric_limits<int>::max();
    for (const Screen& s : std::as_const(m_list)) {
        int x = 0, y = 0;
        if (isOn(s.fields.value(kEnabled)) && readInt(s.fields.value(kX), &x) && readInt(s.fields.value(kY), &y)) {
            x0 = std::min(x0, x);
            y0 = std::min(y0, y);
        }
    }
    if (x0 != std::numeric_limits<int>::max() && (x0 != 0 || y0 != 0)) {
        for (Screen& s : m_list) {
            int x = 0, y = 0;
            if (readInt(s.fields.value(kX), &x))
                s.fields.insert(kX, QString::number(x - x0));
            if (readInt(s.fields.value(kY), &y))
                s.fields.insert(kY, QString::number(y - y0));
        }
    }
    updateDerived();
    return true;
}

bool DisplaysBackend::setScreenField(const QString& desc, const QString& field, const QString& text)
{
    const int i = indexOf(desc);
    if (i < 0)
        return refuse(QStringLiteral("Displays: no screen ") + desc);
    if (field != kEnabled && field != kMode && field != kScale && field != kTransform)
        return refuse(QStringLiteral("Displays: a screen has no setting ") + field + QStringLiteral(" to change here"));
    m_list[i].fields.insert(field, text);
    updateDerived();
    return true;
}

bool DisplaysBackend::apply()
{
    const config::Option* opt = option();
    if (!panel() || !opt)
        return refuse(QStringLiteral("Displays: the page has no list of screens in the description"));
    if (m_pending)
        return refuse(QStringLiteral("Displays: keep or go back first"));

    // A field that is not valid is refused before anything runs.
    for (const Screen& s : std::as_const(m_list)) {
        for (const config::Field& f : opt->fields) {
            if (!s.fields.contains(f.name))
                continue;
            const QString why = config::checkScalar(f, s.fields.value(f.name));
            if (!why.isEmpty())
                return refuse(QStringLiteral("Displays: %1: %2: %3").arg(s.fields.value(kDesc), f.label, why));
        }
        int w = 0, h = 0;
        if (isOn(s.fields.value(kEnabled)) && !logicalSize(s, &w, &h))
            return refuse(QStringLiteral("Displays: ") + s.fields.value(kDesc)
                          + QStringLiteral(": its mode or scale is not valid"));
    }

    // The candidate: the values as they would be, rendered by the hyprland-lua writer, checked by Hyprland.
    const QString schemaDir = panel()->paths().schemaDir;
    const std::optional<QByteArray> current = readFile(panel()->paths().values);
    QString error;
    const QByteArray candidateValues
        = config::editListValue(current.value_or(config::newValuesFileText()), *opt, items(), &error);
    if (candidateValues.isEmpty())
        return refuse(QStringLiteral("Displays: ") + error);
    const std::unique_ptr<config::Values> values
        = config::Values::parse(candidateValues, panel()->paths().values, schemaDir, &error);
    if (!values)
        return refuse(QStringLiteral("Displays: ") + error);
    const config::Writer* writer = config::findWriter(QStringLiteral("hyprland-lua"));
    if (!writer)
        return refuse(QStringLiteral("Displays: no writer for hyprland-lua"));
    QByteArray candidate;
    if (!writer->render(config::Config(panel()->description(), *values), nullptr, &candidate, &error))
        return refuse(QStringLiteral("Displays: ") + error);
    QTemporaryFile file(QDir::tempPath() + QStringLiteral("/riley-settings-candidate-XXXXXX.lua"));
    if (!file.open() || file.write(candidate) != candidate.size() || !file.flush())
        return refuse(QStringLiteral("Displays: cannot write the candidate for Hyprland's check"));
    file.close();
    const CommandResult r = runCommand({QStringLiteral("Hyprland"), QStringLiteral("--verify-config"),
                                        QStringLiteral("-c"), file.fileName()});
    const QStringList lines = QString::fromUtf8(r.out).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const bool ok = r.ok() && !lines.isEmpty() && lines.last().trimmed() == QLatin1String("config ok");
    if (!ok) {
        QString why = r.started ? (lines.isEmpty() ? QString::fromUtf8(r.err.trimmed()) : lines.first().trimmed())
                                : QStringLiteral("Hyprland's check did not run");
        return refuse(QStringLiteral("Hyprland refused these settings; nothing was changed: ") + why.left(200));
    }

    // Remember what is there now: the values file and every target of the screens.
    QList<Remembered> before;
    before.append({panel()->paths().values, current, false});
    for (const QString& target : opt->targets) {
        const QString path = panel()->targetPath(target);
        if (path.isEmpty())
            continue;
        before.append({path, readFile(path), target == QLatin1String("hyprland-lua")});
    }

    Change change;
    change.option = opt;
    change.items = items();
    if (!panel()->commitStep({change}))
        return false;
    m_before = before;
    m_pending = true;
    m_secondsLeft = m_keepSeconds;
    m_timer.start();
    updateDerived();
    return true;
}

bool DisplaysBackend::keep()
{
    if (!m_pending)
        return refuse(QStringLiteral("Displays: nothing to keep"));
    m_timer.stop();
    m_pending = false;
    m_secondsLeft = 0;
    m_before.clear();
    updateDerived();
    return true;
}

bool DisplaysBackend::revert()
{
    if (!m_pending)
        return refuse(QStringLiteral("Displays: nothing to go back from"));
    return goBack();
}

void DisplaysBackend::tick()
{
    if (!m_pending) {
        m_timer.stop();
        return;
    }
    --m_secondsLeft;
    if (m_secondsLeft <= 0) {
        goBack();
        return;
    }
    Q_EMIT changed();
}

bool DisplaysBackend::goBack()
{
    // core.py restore: put back exactly the bytes that were there (remove what was not), then reload.
    m_timer.stop();
    m_pending = false;
    m_secondsLeft = 0;
    bool ok = true;
    QString failed;
    for (const Remembered& r : std::as_const(m_before)) {
        // The M3 gate's planted fault: the live Hyprland file is not put back.
        if (r.isLiveConfig && Controller::plantFaultPages())
            continue;
        QString error;
        if (r.bytes) {
            QDir().mkpath(QFileInfo(r.path).absolutePath());
            if (!config::writeFileAtomic(r.path, *r.bytes, &error)) {
                ok = false;
                failed = error;
            }
        } else if (QFile::exists(r.path) && !QFile::remove(r.path)) {
            ok = false;
            failed = r.path + QStringLiteral(": cannot remove");
        }
    }
    m_before.clear();
    const config::Option* opt = option();
    if (panel()) {
        panel()->reloadFromDisk();
        if (opt)
            panel()->recordActions(opt->actions);
    }
    load();
    if (!ok)
        return refuse(QStringLiteral("Displays: going back: ") + failed);
    return true;
}

}  // namespace riley::panel::pages
