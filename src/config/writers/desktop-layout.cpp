#include "desktop-layout.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <algorithm>
#include <variant>

namespace riley::config::desktop {
namespace {

const QSize DESKTOP_SIZE(440, 320);
const QSize FOLDER_SIZE(340, 240);
const QPoint START(48, 72);
constexpr int GAP = 16;
constexpr int DOCK = 90;   // keep new boxes out of the bottom strip where the dock sits

// The gap below the box above a new one. The M3 gate's planted fault (docs/pages-contract.md): 24 instead of 16.
constexpr int belowGap()
{
#ifdef RILEY_PLANT_FAULT_PAGES
    return 24;
#else
    return GAP;
#endif
}

QString s(const char* key)
{
    return QString::fromLatin1(key);
}

QJsonArray boxesOf(const QJsonObject& cfg)
{
    return cfg.value(s("boxes")).toArray();
}

QString folderOf(const QJsonValue& b)
{
    return b.toObject().value(s("folder")).toString();
}

bool isAuto(const QJsonObject& b)
{
    return b.value(s("auto")).toBool(false);
}

QRect rectOf(const QJsonObject& b)
{
    return QRect(b.value(s("x")).toInt(), b.value(s("y")).toInt(), b.value(s("w")).toInt(), b.value(s("h")).toInt());
}

bool isDir(const QString& path)
{
    return QFileInfo(path).isDir();
}

// Riley's core.natural_key: case folded, runs of digits compared as numbers.
using Part = std::variant<QString, qulonglong>;
QList<Part> naturalKey(const QString& text)
{
    static const QRegularExpression digits(s("(\\d+)"));
    QList<Part> out;
    const QString t = text.toCaseFolded();
    qsizetype at = 0;
    auto it = digits.globalMatch(t);
    while (it.hasNext()) {
        const auto m = it.next();
        out.append(t.mid(at, m.capturedStart() - at));
        out.append(m.captured().toULongLong());
        at = m.capturedEnd();
    }
    out.append(t.mid(at));
    return out;
}

bool naturalLess(const QString& a, const QString& b)
{
    const QList<Part> ka = naturalKey(a), kb = naturalKey(b);
    for (qsizetype i = 0; i < qMin(ka.size(), kb.size()); ++i) {
        if (ka[i] == kb[i])
            continue;
        if (ka[i].index() != kb[i].index())   // never happens: both alternate text, number, text...
            return ka[i].index() < kb[i].index();
        if (const auto* x = std::get_if<QString>(&ka[i]))
            return *x < std::get<QString>(kb[i]);
        return std::get<qulonglong>(ka[i]) < std::get<qulonglong>(kb[i]);
    }
    return ka.size() < kb.size();
}

// The folders inside `desktop` that are not hidden, in Riley's natural order.
QStringList subfolders(const QString& desktop)
{
    const QFileInfoList list = QDir(desktop).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList out;
    for (const QFileInfo& fi : list) {
        if (!fi.fileName().startsWith(QLatin1Char('.')))
            out.append(cleanFolder(fi.absoluteFilePath()));
    }
    std::stable_sort(out.begin(), out.end(), [](const QString& a, const QString& b) {
        return naturalLess(QFileInfo(a).fileName(), QFileInfo(b).fileName());
    });
    return out;
}

QString onScreen(const QJsonObject& b, const Screens& screens)
{
    const QString screen = b.value(s("screen")).toString();
    return screens.sizes.contains(screen) ? screen : screens.main;
}

QJsonObject newBox(const QString& folder, const QString& screen, QPoint at, QSize size)
{
    QJsonObject b;
    b.insert(s("folder"), folder);
    b.insert(s("screen"), screen);
    b.insert(s("x"), at.x());
    b.insert(s("y"), at.y());
    b.insert(s("w"), size.width());
    b.insert(s("h"), size.height());
    return b;
}

QList<QRect> takenOnMain(const QJsonArray& boxes, const Screens& screens)
{
    QList<QRect> taken;
    for (const QJsonValue& v : boxes) {
        const QJsonObject b = v.toObject();
        if (onScreen(b, screens) == screens.main)
            taken.append(rectOf(b));
    }
    return taken;
}

}  // namespace

QVariantMap Screens::toPageData() const
{
    QVariantMap sizesMap;
    for (auto it = sizes.cbegin(); it != sizes.cend(); ++it)
        sizesMap.insert(it.key(), QVariantList{it.value().width(), it.value().height()});
    return {{s("desktopScreens"), sizesMap}, {s("desktopMain"), main}};
}

Screens Screens::fromPageData(const QVariantMap& data)
{
    Screens out;
    const QVariantMap sizesMap = data.value(s("desktopScreens")).toMap();
    for (auto it = sizesMap.cbegin(); it != sizesMap.cend(); ++it) {
        const QVariantList wh = it.value().toList();
        if (wh.size() == 2)
            out.sizes.insert(it.key(), QSize(wh[0].toInt(), wh[1].toInt()));
    }
    out.main = data.value(s("desktopMain")).toString();
    return out;
}

QString cleanFolder(const QString& folder)
{
    return QDir::cleanPath(QFileInfo(folder).absoluteFilePath());
}

QString mainOf(const QStringList& descs)
{
    for (const QString& d : descs) {
        if (d.contains(QLatin1String(MAIN_SERIAL)))
            return d;
    }
    return descs.isEmpty() ? QString() : descs.first();
}

QJsonObject load(const QString& path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject() && doc.object().value(s("boxes")).isArray())
            return doc.object();
    }
    QJsonObject empty;
    empty.insert(s("boxes"), QJsonArray());
    return empty;
}

QByteArray toText(const QJsonObject& cfg)
{
    // Riley writes json.dumps(indent=2); Qt indents by 4. Both read alike; keep Riley's look.
    const QByteArray four = QJsonDocument(cfg).toJson(QJsonDocument::Indented);
    QByteArray out;
    for (const QByteArray& line : four.split('\n')) {
        qsizetype n = 0;
        while (n < line.size() && line[n] == ' ')
            ++n;
        out += QByteArray(n / 2, ' ') + line.mid(n) + '\n';
    }
    while (out.endsWith("\n\n"))
        out.chop(1);
    return out;
}

QPoint freeSpot(int w, int h, const QList<QRect>& taken, QSize screen)
{
    const int sw = screen.width(), sh = screen.height();
    int x = START.x();
    while (x + w <= sw) {
        QList<QRect> col;
        for (const QRect& r : taken) {
            if (r.x() < x + w && r.x() + r.width() > x)
                col.append(r);
        }
        int y = START.y();
        if (!col.isEmpty()) {
            y = col.first().y() + col.first().height() + belowGap();
            for (const QRect& r : col)
                y = std::max(y, r.y() + r.height() + belowGap());
        }
        if (y + h <= sh - DOCK)
            return QPoint(x, y);
        if (col.isEmpty())   // the box does not fit this screen at all
            return START;
        int right = col.first().x() + col.first().width();
        for (const QRect& r : col)
            right = std::max(right, r.x() + r.width());
        x = right + GAP;
    }
    return START;
}

QList<Shown> resolve(QJsonObject* cfg, const QString& desktopDir, const Screens& screens)
{
    QList<Shown> out;
    if (!screens.known())
        return out;
    const QString desktop = cleanFolder(desktopDir);
    QJsonArray boxes;
    for (const QJsonValue& v : boxesOf(*cfg)) {
        const QJsonObject b = v.toObject();
        if (!isAuto(b) || isDir(folderOf(b)))
            boxes.append(b);
    }
    QStringList have;
    for (const QJsonValue& v : boxes)
        have.append(folderOf(v));
    for (const QString& folder : QStringList{desktop} + subfolders(desktop)) {
        if (have.contains(folder))
            continue;
        const QSize size = folder == desktop ? DESKTOP_SIZE : FOLDER_SIZE;
        const QList<QRect> taken = takenOnMain(boxes, screens);
        const QPoint at = folder == desktop && taken.isEmpty()
            ? START
            : freeSpot(size.width(), size.height(), taken, screens.sizes.value(screens.main));
        QJsonObject b = newBox(folder, screens.main, at, size);
        b.insert(s("auto"), true);
        boxes.append(b);
    }
    cfg->insert(s("boxes"), boxes);

    if (!cfg->value(s("show")).toBool(true))
        return out;
    for (const QJsonValue& v : boxes) {
        const QJsonObject b = v.toObject();
        const QString folder = folderOf(b);
        if (!isDir(folder) || b.value(s("hidden")).toBool(false))
            continue;
        const QString on = onScreen(b, screens);
        const QSize sz = screens.sizes.value(on);
        Shown sh;
        sh.folder = folder;
        sh.shownOn = on;
        sh.w = std::min(b.value(s("w")).toInt(), sz.width());
        sh.h = std::min(b.value(s("h")).toInt(), sz.height());
        sh.x = std::max(0, std::min(b.value(s("x")).toInt(), sz.width() - sh.w));
        sh.y = std::max(0, std::min(b.value(s("y")).toInt(), sz.height() - sh.h));
        sh.title = folder == desktop ? s("Desktop") : QFileInfo(folder).fileName();
        out.append(sh);
    }
    return out;
}

bool addFolder(QJsonObject* cfg, const QString& folderIn, const Screens& screens)
{
    const QString folder = cleanFolder(folderIn);
    QJsonArray boxes = boxesOf(*cfg);
    for (const QJsonValue& v : boxes) {
        if (folderOf(v) == folder)
            return false;
    }
    if (!screens.known())
        return false;
    const QPoint at = freeSpot(FOLDER_SIZE.width(), FOLDER_SIZE.height(), takenOnMain(boxes, screens),
                               screens.sizes.value(screens.main));
    boxes.append(newBox(folder, screens.main, at, FOLDER_SIZE));
    cfg->insert(s("boxes"), boxes);
    return true;
}

bool setHidden(QJsonObject* cfg, const QString& folderIn, bool hidden)
{
    const QString folder = cleanFolder(folderIn);
    QJsonArray boxes = boxesOf(*cfg);
    bool found = false;
    for (qsizetype i = 0; i < boxes.size(); ++i) {
        QJsonObject b = boxes[i].toObject();
        if (b.value(s("folder")).toString() != folder)
            continue;
        found = true;
        if (hidden)
            b.insert(s("hidden"), true);
        else
            b.remove(s("hidden"));
        boxes[i] = b;
    }
    cfg->insert(s("boxes"), boxes);
    return found;
}

bool remove(QJsonObject* cfg, const QString& folderIn)
{
    const QString folder = cleanFolder(folderIn);
    const QJsonArray boxes = boxesOf(*cfg);
    QJsonArray kept;
    for (const QJsonValue& v : boxes) {
        const QJsonObject b = v.toObject();
        if (folderOf(b) != folder || isAuto(b))
            kept.append(b);
    }
    cfg->insert(s("boxes"), kept);
    return kept.size() < boxes.size();
}

QList<ValueBox> valueBoxes(const QJsonObject& cfg)
{
    QList<ValueBox> out;
    for (const QJsonValue& v : boxesOf(cfg)) {
        const QJsonObject b = v.toObject();
        const bool hidden = b.value(s("hidden")).toBool(false);
        if (isAuto(b) && !hidden)
            continue;
        out.append({folderOf(b), b.value(s("screen")).toString(), hidden});
    }
    return out;
}

void applyValueBoxes(QJsonObject* cfg, const QList<ValueBox>& wanted, const Screens& screens)
{
    QHash<QString, const ValueBox*> byFolder;
    for (const ValueBox& vb : wanted)
        byFolder.insert(cleanFolder(vb.folder), &vb);
    QJsonArray boxes;
    QStringList have;
    for (const QJsonValue& v : boxesOf(*cfg)) {
        QJsonObject b = v.toObject();
        const QString folder = folderOf(b);
        const ValueBox* vb = byFolder.value(folder, nullptr);
        if (!isAuto(b) && !vb)
            continue;   // a box you added that the values no longer hold
        if (vb && vb->hidden)
            b.insert(s("hidden"), true);
        else
            b.remove(s("hidden"));
        boxes.append(b);
        have.append(folder);
    }
    cfg->insert(s("boxes"), boxes);
    for (const ValueBox& vb : wanted) {
        const QString folder = cleanFolder(vb.folder);
        if (have.contains(folder) || !screens.known())
            continue;
        addFolder(cfg, folder, screens);
        if (vb.hidden)
            setHidden(cfg, folder, true);
        have.append(folder);
    }
}

}  // namespace riley::config::desktop
