// The M3 gate's harness (tests/run-pages.sh): drives each custom page of share/riley-settings/settings.xml the way
// docs/pages-contract.md says, and compares the values file, the target files, the action log and the commands run
// (through fakes first on PATH) with tests/pages/expected/, worked out by hand from Riley's gates before the pages
// existed. One group of checks per page; every check's name starts with its group ("dock: ...").
//
// Environment (set by run-pages.sh): HOME (= $RILEY_PAGES_RUN/home, wiped and seeded before each group),
// XDG_CONFIG_HOME, XDG_CACHE_HOME, RILEY_SETTINGS_SHARE, RILEY_SETTINGS_ACTION_LOG (read by the panel),
// RILEY_FAKE_DIR (the fakes' canned answers, state and call log), RILEY_PAGES_RUN, RILEY_PAGES_EXPECTED,
// RILEY_PAGES_SEEDS, RILEY_PAGES_RESULTS (one "PASS name" or "FAIL name" line per check, a failure's detail on the
// next line, indented; the last line is "DONE <n>").
#include "panel/panel.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

#include <cstdio>
#include <functional>

namespace {

const QString DELL = QStringLiteral("Dell Inc. DELL U2515H YKFWP4CC09VL");
const QString S237 = QStringLiteral("Samsung Electric Company S34C65xU HCNYB00237");
const QString S019 = QStringLiteral("Samsung Electric Company S34C65xU HCNYB00019");
const QString AR37 = QStringLiteral("GWD ARZOPA 0x37330206");
const QString AR15 = QStringLiteral("GWD ARZOPA 0000001504150");
const QString FF = QStringLiteral("firefox"), PT = QStringLiteral("org.gnome.Ptyxis"),
              DO = QStringLiteral("org.kde.dolphin"), SE = QStringLiteral("riley.settings"),
              ZED = QStringLiteral("dev.zed.Zed");

QString env(const char* name)
{
    return qEnvironmentVariable(name);
}

QByteArray readAll(const QString& path, bool* exists = nullptr)
{
    QFile f(path);
    const bool ok = f.open(QIODevice::ReadOnly);
    if (exists)
        *exists = ok;
    return ok ? f.readAll() : QByteArray();
}

bool writeAll(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(data) == data.size();
}

QString firstDifference(const QByteArray& got, const QByteArray& want)
{
    const QList<QByteArray> g = got.split('\n');
    const QList<QByteArray> w = want.split('\n');
    for (qsizetype i = 0; i < qMax(g.size(), w.size()); ++i) {
        const QByteArray a = i < g.size() ? g[i] : QByteArray("<end of file>");
        const QByteArray b = i < w.size() ? w[i] : QByteArray("<end of file>");
        if (a != b)
            return QStringLiteral("line %1: got \"%2\", want \"%3\"")
                .arg(i + 1)
                .arg(QString::fromUtf8(a.left(160)), QString::fromUtf8(b.left(160)));
    }
    return QStringLiteral("same lines, different bytes (line ends?)");
}

int lineCount(const QByteArray& text)
{
    return int(text.count('\n'));
}

QByteArray firstLines(const QByteArray& text, int n)
{
    qsizetype at = 0;
    for (int i = 0; i < n; ++i) {
        const qsizetype nl = text.indexOf('\n', at);
        if (nl < 0)
            return text;
        at = nl + 1;
    }
    return text.left(at);
}

void collect(QQuickItem* item, const std::function<bool(const QString&)>& match, QList<QQuickItem*>* out)
{
    if (!item)
        return;
    if (match(item->objectName()))
        out->append(item);
    for (QQuickItem* c : item->childItems())
        collect(c, match, out);
}

QString show(const QStringList& l)
{
    return QLatin1Char('[') + l.join(QStringLiteral(", ")) + QLatin1Char(']');
}

}  // namespace

class TestPages : public QObject {
    Q_OBJECT

private:
    QQmlApplicationEngine* m_engine = nullptr;
    QQuickWindow* m_window = nullptr;
    QString m_run, m_home, m_config, m_cache, m_values, m_log, m_fake, m_expected, m_seeds;
    QFile m_results;
    int m_checks = 0;
    int m_failed = 0;
    int m_failedBefore = 0;
    int m_wait = 5000;   // how long a check waits for something to show; short once a group's page is missing
    QString m_group;

    // ---- reporting ------------------------------------------------------------------------------------------------
    void report(const QString& line)
    {
        m_results.write(line.toUtf8() + '\n');
        m_results.flush();
        std::fprintf(stdout, "%s\n", line.toUtf8().constData());
    }

    bool check(const QString& name, bool ok, const QString& detail = QString())
    {
        ++m_checks;
        if (!ok)
            ++m_failed;
        const QString full = m_group + QStringLiteral(": ") + name;
        report((ok ? QStringLiteral("PASS ") : QStringLiteral("FAIL ")) + full);
        if (!ok && !detail.isEmpty())
            report(QStringLiteral("     ") + detail.left(400));
        return ok;
    }

    QByteArray expected(const QString& name) const
    {
        return readAll(m_expected + QLatin1Char('/') + name).replace("@HOME@", m_home.toUtf8());
    }

    bool sameBytes(const QString& name, const QString& path, const QByteArray& want)
    {
        bool exists = false;
        const QByteArray got = readAll(path, &exists);
        if (!exists)
            return check(name, false, QStringLiteral("no file ") + path);
        return check(name, got == want, firstDifference(got, want));
    }

    bool sameAs(const QString& what, const QString& path, const QString& expectedFile)
    {
        return sameBytes(what + QStringLiteral(" is expected/") + expectedFile, path, expected(expectedFile));
    }

    bool absent(const QString& name, const QString& path)
    {
        return check(name, !QFileInfo::exists(path), QStringLiteral("%1 exists").arg(path));
    }

    QString cfg(const QString& rel) const
    {
        return m_config + QLatin1Char('/') + rel;
    }

    QByteArray log() const
    {
        return readAll(m_log);
    }

    // Waits up to 5 s for the action log to hold n lines, then compares it with the first n lines of an expected log.
    bool logIs(const QString& expectedFile, int n)
    {
        (void)QTest::qWaitFor([&] { return lineCount(log()) >= n; }, m_wait);
        QTest::qWait(50);
        return check(QStringLiteral("action log is the first %1 lines of expected/%2").arg(n).arg(expectedFile),
                     log() == firstLines(expected(expectedFile), n), firstDifference(log(), firstLines(expected(expectedFile), n)));
    }

    // For a step that must record nothing: a short wait, then the log still has n lines.
    bool logStays(int n)
    {
        QTest::qWait(300);
        return check(QStringLiteral("action log still has %1 lines").arg(n), lineCount(log()) == n,
                     QStringLiteral("%1 lines: %2").arg(lineCount(log())).arg(QString::fromUtf8(log().right(120))));
    }

    QList<QByteArray> calls(const QByteArray& prefix = QByteArray()) const
    {
        QList<QByteArray> out;
        for (const QByteArray& l : readAll(m_fake + QStringLiteral("/calls.log")).split('\n'))
            if (!l.isEmpty() && l.startsWith(prefix))
                out.append(l);
        return out;
    }

    bool noUnexpectedCall()
    {
        const QList<QByteArray> u = calls("UNEXPECTED ");
        return check(QStringLiteral("no command outside the contract's list was run"), u.isEmpty(),
                     u.isEmpty() ? QString() : QString::fromUtf8(u.first()));
    }

    // The files (not directories) under the throwaway home, relative to it, sorted.
    bool homeHolds(QStringList want)
    {
        QStringList got;
        QDirIterator it(m_home, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext())
            got.append(QDir(m_home).relativeFilePath(it.next()));
        got.sort();
        want.sort();
        return check(QStringLiteral("the home holds only ") + (want.isEmpty() ? QStringLiteral("its folders") : want.join(QStringLiteral(", "))),
                     got == want, QStringLiteral("found ") + show(got));
    }

    QString message() const
    {
        QObject* panel = m_window ? m_window->property("panel").value<QObject*>() : nullptr;
        return panel ? panel->property("message").toString() : QStringLiteral("<no panel property on the window>");
    }

    bool messageShown(const QString& what)
    {
        const QString m = message();
        return check(what + QStringLiteral(": the panel shows a reason"), !m.isEmpty() && !m.startsWith(QLatin1Char('<')),
                     QStringLiteral("message \"%1\"").arg(m));
    }

    void clearMessage()
    {
        if (QObject* panel = m_window ? m_window->property("panel").value<QObject*>() : nullptr)
            QMetaObject::invokeMethod(panel, "clearMessage");
    }

    // ---- the panel ---------------------------------------------------------------------------------------------------
    QQuickItem* root() const
    {
        QQuickItem* r = m_window ? m_window->contentItem() : nullptr;
        while (r && r->parentItem())
            r = r->parentItem();
        return r;
    }

    QList<QQuickItem*> named(const QString& name) const
    {
        QList<QQuickItem*> out;
        collect(root(), [&](const QString& n) { return n == name; }, &out);
        return out;
    }

    QList<QQuickItem*> visibleWithPrefix(const QString& prefix) const
    {
        QList<QQuickItem*> all, out;
        collect(root(), [&](const QString& n) { return n.startsWith(prefix); }, &all);
        for (QQuickItem* i : all)
            if (i->isVisible())
                out.append(i);
        return out;
    }

    QQuickItem* visible(const QString& name, QString* why, int ms = -1)
    {
        if (ms < 0)
            ms = m_wait;
        QList<QQuickItem*> shown;
        (void)QTest::qWaitFor(
            [&] {
                shown.clear();
                for (QQuickItem* i : named(name))
                    if (i->isVisible())
                        shown.append(i);
                return shown.size() == 1;
            },
            ms);
        if (shown.size() == 1)
            return shown.first();
        *why = QStringLiteral("%1 visible items named %2 (want 1)").arg(shown.size()).arg(name);
        return nullptr;
    }

    QQuickItem* need(const QString& name)
    {
        QString why;
        QQuickItem* i = visible(name, &why);
        check(name + QStringLiteral(" is shown"), i, why);
        return i;
    }

    bool click(QQuickItem* item, QString* why)
    {
        const QPointF c = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
        if (item->width() <= 0 || item->height() <= 0 || c.x() < 0 || c.y() < 0 || c.x() >= m_window->width()
            || c.y() >= m_window->height()) {
            *why = QStringLiteral("%1 has its centre at %2,%3 (size %4x%5), outside the window")
                       .arg(item->objectName())
                       .arg(c.x())
                       .arg(c.y())
                       .arg(item->width())
                       .arg(item->height());
            return false;
        }
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, c.toPoint());
        return true;
    }

    bool clickNamed(const QString& name)
    {
        QString why;
        QQuickItem* i = visible(name, &why);
        const bool ok = i && click(i, &why);
        return check(QStringLiteral("clicked ") + name, ok, why);
    }

    bool focus(QQuickItem* item, QString* why)
    {
        item->forceActiveFocus();
        const bool ok = QTest::qWaitFor(
            [&] {
                QQuickItem* f = m_window->activeFocusItem();
                return f && (f == item || item->isAncestorOf(f));
            },
            2000);
        if (!ok)
            *why = QStringLiteral("%1 did not take the active focus").arg(item->objectName());
        return ok;
    }

    bool showPage(const QString& id)
    {
        QString why;
        QQuickItem* entry = visible(QStringLiteral("page:") + id, &why);
        const bool ok = entry && click(entry, &why);
        return check(QStringLiteral("menu entry page:") + id + QStringLiteral(" clicked"), ok, why);
    }

    QQuickItem* openWidget(const QString& page, const QString& widget)
    {
        QQuickItem* w = showPage(page) ? need(QStringLiteral("widget:") + widget) : nullptr;
        if (!w)
            m_wait = 300;   // no page: the group's other checks fail at once instead of waiting
        return w;
    }

    // An M2 control of a widget page: its type and value.
    QQuickItem* control(const QString& key, const QString& type, const QString& value)
    {
        QString why;
        QQuickItem* i = visible(QStringLiteral("option:") + key, &why);
        if (!check(QStringLiteral("option:") + key + QStringLiteral(" is shown"), i, why))
            return nullptr;
        const QString t = i->property("controlType").toString();
        check(QStringLiteral("option:") + key + QStringLiteral(" has controlType ") + type, t == type,
              QStringLiteral("controlType \"%1\"").arg(t));
        valueIs(i, key, value);
        return i;
    }

    bool valueIs(QQuickItem* i, const QString& key, const QString& value)
    {
        const QString got = i ? i->property("valueText").toString() : QStringLiteral("<no control>");
        return check(QStringLiteral("option:") + key + QStringLiteral(" shows \"") + value + QLatin1Char('"'), got == value,
                     QStringLiteral("valueText \"%1\"").arg(got));
    }

    bool keys(QQuickItem* i, Qt::Key key, int times)
    {
        QString why;
        if (!i || !focus(i, &why))
            return check(QStringLiteral("focus for key presses"), false, i ? why : QStringLiteral("no control"));
        for (int n = 0; n < times; ++n) {
            QTest::keyClick(m_window, key);
            QTest::qWait(20);
        }
        return true;
    }

    bool type(QQuickItem* i, const QString& text)
    {
        QString why;
        if (!i || !focus(i, &why))
            return check(QStringLiteral("focus for typing"), false, i ? why : QStringLiteral("no control"));
        QTest::keyClick(m_window, Qt::Key_A, Qt::ControlModifier);
        for (const QChar c : text)
            QTest::keyClick(m_window, c.toLatin1());
        QTest::keyClick(m_window, Qt::Key_Return);
        return true;
    }

    // Calls a QML function of the contract on an item, with untyped arguments.
    QVariant call(QQuickItem* item, const char* fn, const QVariantList& a = {})
    {
        QVariant r;
        bool ok = false;
        if (item) {
            switch (a.size()) {
            case 0: ok = QMetaObject::invokeMethod(item, fn, Q_RETURN_ARG(QVariant, r)); break;
            case 1: ok = QMetaObject::invokeMethod(item, fn, Q_RETURN_ARG(QVariant, r), Q_ARG(QVariant, a[0])); break;
            case 2:
                ok = QMetaObject::invokeMethod(item, fn, Q_RETURN_ARG(QVariant, r), Q_ARG(QVariant, a[0]), Q_ARG(QVariant, a[1]));
                break;
            case 3:
                ok = QMetaObject::invokeMethod(item, fn, Q_RETURN_ARG(QVariant, r), Q_ARG(QVariant, a[0]), Q_ARG(QVariant, a[1]),
                                               Q_ARG(QVariant, a[2]));
                break;
            case 4:
                ok = QMetaObject::invokeMethod(item, fn, Q_RETURN_ARG(QVariant, r), Q_ARG(QVariant, a[0]), Q_ARG(QVariant, a[1]),
                                               Q_ARG(QVariant, a[2]), Q_ARG(QVariant, a[3]));
                break;
            default: break;
            }
        }
        if (!ok) {
            ++m_checks;
            ++m_failed;
            report(QStringLiteral("FAIL ") + m_group + QStringLiteral(": ") + (item ? item->objectName() : QStringLiteral("<no item>"))
                   + QStringLiteral(" has a function ") + QString::fromLatin1(fn) + QStringLiteral("(%1 arguments)").arg(a.size()));
            return QVariant();
        }
        return r;
    }

    bool open()
    {
        m_engine = new QQmlApplicationEngine(this);
        QString error;
        riley::panel::LoadOptions options;   // applyActions stays false: actions and launches are recorded, never run
        const bool loaded = riley::panel::load(*m_engine, options, &error);
        m_window = loaded && !m_engine->rootObjects().isEmpty() ? qobject_cast<QQuickWindow*>(m_engine->rootObjects().first())
                                                                : nullptr;
        if (!check(QStringLiteral("the panel opens"), m_window, loaded ? QStringLiteral("no QQuickWindow") : error))
            return false;
        m_window->resize(1280, 1600);
        m_window->show();
        m_window->requestActivate();
        const bool up = QTest::qWaitForWindowExposed(m_window) && QTest::qWaitForWindowActive(m_window);
        QTest::qWait(100);
        return check(QStringLiteral("the window is exposed and active offscreen"), up);
    }

    void close()
    {
        if (m_window)
            m_window->close();
        delete m_engine;
        m_engine = nullptr;
        m_window = nullptr;
        QTest::qWait(20);
    }

    // A fresh start for one page group: panel closed, home wiped and seeded, empty logs and fake state.
    bool begin(const QString& group)
    {
        close();
        m_group = group;
        m_wait = 5000;
        if (m_home != m_run + QStringLiteral("/home") || !QFile::exists(m_run + QStringLiteral("/.riley-pages-run"))) {
            check(QStringLiteral("harness: the throwaway home is $RILEY_PAGES_RUN/home"), false, m_home);
            return false;
        }
        QDir(m_home).removeRecursively();
        QDir().mkpath(m_config);
        QFile::remove(m_cache + QStringLiteral("/nwg-dock-pinned"));
        QFile::remove(m_log);
        writeAll(m_log, QByteArray());
        QDir state(m_fake + QStringLiteral("/state"));
        state.removeRecursively();
        QDir().mkpath(state.path());
        QFile::remove(m_fake + QStringLiteral("/calls.log"));
        for (const QString& f : QDir(m_fake).entryList({QStringLiteral("verify-*.lua")}, QDir::Files))
            QFile::remove(m_fake + QLatin1Char('/') + f);
        return true;
    }

    bool seed(const QString& seedFile, const QString& to)
    {
        return check(QStringLiteral("harness: seed ") + seedFile, writeAll(to, readAll(m_seeds + QLatin1Char('/') + seedFile)));
    }

    QStringList strings(const QVariant& v) const
    {
        QStringList out;
        for (const QVariant& x : v.toList())
            out.append(x.toString());
        return out;
    }

    // ---- displays helpers ----------------------------------------------------------------------------------------------
    bool at(const QString& desc, int x, int y)
    {
        QString why;
        QQuickItem* s = visible(QStringLiteral("screen:") + desc, &why);
        const int gx = s ? s->property("screenX").toInt() : -99999, gy = s ? s->property("screenY").toInt() : -99999;
        return check(QStringLiteral("screen:%1 at (%2, %3)").arg(desc).arg(x).arg(y), s && gx == x && gy == y,
                     s ? QStringLiteral("at (%1, %2)").arg(gx).arg(gy) : why);
    }

    void displaysUntouched(const QString& when)
    {
        sameBytes(when + QStringLiteral(": values file is the seed"), m_values, readAll(m_seeds + QStringLiteral("/displays-values.xml")));
        sameBytes(when + QStringLiteral(": settings.lua is the live file"), cfg(QStringLiteral("hypr/settings.lua")),
                  QByteArrayLiteral("-- the live file\n"));
        absent(when + QStringLiteral(": no greeter file"), cfg(QStringLiteral("riley/greeter-sway.conf")));
        absent(when + QStringLiteral(": no hyprpaper.conf"), cfg(QStringLiteral("hypr/hyprpaper.conf")));
    }

    void displaysApplied(const QString& when)
    {
        sameAs(when + QStringLiteral(": settings.lua"), cfg(QStringLiteral("hypr/settings.lua")), QStringLiteral("expected-m1.lua"));
        sameAs(when + QStringLiteral(": greeter file"), cfg(QStringLiteral("riley/greeter-sway.conf")), QStringLiteral("expected-greeter.conf"));
        sameAs(when + QStringLiteral(": hyprpaper.conf"), cfg(QStringLiteral("hypr/hyprpaper.conf")), QStringLiteral("displays-hyprpaper.conf"));
        sameAs(when + QStringLiteral(": values file"), m_values, QStringLiteral("displays-values-applied.xml"));
    }

    bool verifyCalls(int n)
    {
        (void)QTest::qWaitFor([&] { return calls("Hyprland ").size() >= n; }, m_wait);
        const QList<QByteArray> c = calls("Hyprland ");
        bool shape = true;
        for (const QByteArray& l : c)
            shape &= l.startsWith("Hyprland --verify-config -c ");
        return check(QStringLiteral("Hyprland --verify-config -c <candidate> ran %1 times").arg(n), c.size() == n && shape,
                     QStringLiteral("%1 calls%2").arg(c.size()).arg(c.isEmpty() ? QString() : QStringLiteral(", last: ") + QString::fromUtf8(c.last())));
    }

    bool prop(QQuickItem* i, const char* name, const QVariant& want)
    {
        const QVariant got = i ? i->property(name) : QVariant();
        return check(QStringLiteral("%1.%2 is %3").arg(i ? i->objectName() : QStringLiteral("<none>"), QString::fromLatin1(name), want.toString()),
                     got.isValid() && got == want, QStringLiteral("got \"%1\"").arg(got.isValid() ? got.toString() : QStringLiteral("missing")));
    }

    bool waitProp(QQuickItem* i, const char* name, const QVariant& want)
    {
        (void)QTest::qWaitFor([&] { return i && i->property(name) == want; }, m_wait);
        return prop(i, name, want);
    }

    // ---- dock helpers ------------------------------------------------------------------------------------------------
    void dockStep(QQuickItem* w, int n, const QString& valuesFile, const QString& pinnedFile, int logLines)
    {
        const QString at = QStringLiteral("step %1: ").arg(n);
        logIs(QStringLiteral("dock-actions.log"), logLines);
        sameAs(at + QStringLiteral("values file"), m_values, valuesFile);
        sameAs(at + QStringLiteral("pins file"), m_cache + QStringLiteral("/nwg-dock-pinned"), pinnedFile);
        const QStringList want = QString::fromUtf8(expected(pinnedFile)).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        const QStringList got = w ? strings(w->property("pins")) : QStringList();
        check(at + QStringLiteral("the strip shows ") + show(want), got == want, QStringLiteral("pins ") + show(got));
    }

    // ---- desktop helpers -----------------------------------------------------------------------------------------------
    bool shownIs(QQuickItem* w, const QString& what, const QStringList& want)
    {
        const QVariant v = w ? w->property("shown") : QVariant();
        const QStringList got = strings(v);
        return check(what + QStringLiteral(": shown boxes ") + show(want), v.isValid() && got == want,
                     v.isValid() ? QStringLiteral("shown ") + show(got) : QStringLiteral("no property shown"));
    }

    bool desktopJsonIs(const QString& what, const QString& expectedFile)
    {
        const QString path = cfg(QStringLiteral("riley/desktop.json"));
        bool exists = false;
        const QJsonObject got = QJsonDocument::fromJson(readAll(path, &exists)).object();
        const QJsonObject want = QJsonDocument::fromJson(expected(expectedFile)).object();
        const bool same = got.value(QStringLiteral("boxes")) == want.value(QStringLiteral("boxes"))
            && got.value(QStringLiteral("show")).toBool(true) == want.value(QStringLiteral("show")).toBool(true)
            && got.value(QStringLiteral("icon_size")).toInt(48) == want.value(QStringLiteral("icon_size")).toInt(48);
        return check(what + QStringLiteral(": desktop.json is expected/") + expectedFile + QStringLiteral(" (as JSON)"), exists && same,
                     exists ? QString::fromUtf8(QJsonDocument(got).toJson(QJsonDocument::Compact).left(380)) : QStringLiteral("no file ") + path);
    }

private Q_SLOTS:
    void initTestCase()
    {
        m_run = env("RILEY_PAGES_RUN");
        m_home = env("HOME");
        m_config = env("XDG_CONFIG_HOME");
        m_cache = env("XDG_CACHE_HOME");
        m_values = m_config + QStringLiteral("/riley/settings.xml");
        m_log = env("RILEY_SETTINGS_ACTION_LOG");
        m_fake = env("RILEY_FAKE_DIR");
        m_expected = env("RILEY_PAGES_EXPECTED");
        m_seeds = env("RILEY_PAGES_SEEDS");
        m_results.setFileName(env("RILEY_PAGES_RESULTS"));
        QVERIFY2(!m_run.isEmpty() && !m_config.isEmpty() && !m_cache.isEmpty() && !m_log.isEmpty() && !m_fake.isEmpty()
                     && !m_expected.isEmpty() && !m_seeds.isEmpty() && !env("RILEY_SETTINGS_SHARE").isEmpty(),
                 "run through tests/run-pages.sh");
        QVERIFY2(m_results.open(QIODevice::WriteOnly | QIODevice::Append), "RILEY_PAGES_RESULTS is not writable");
        QVERIFY2(m_config == m_home + QStringLiteral("/.config"), "XDG_CONFIG_HOME must be $HOME/.config");
    }

    // Riley gate_m1.py (render, broken model refused, apply then restore) with the page's drag rules.
    void displays()
    {
        if (!begin(QStringLiteral("displays")))
            return;
        seed(QStringLiteral("displays-values.xml"), m_values);
        seed(QStringLiteral("displays-settings.lua"), cfg(QStringLiteral("hypr/settings.lua")));
        if (!open())
            return;
        QQuickItem* w = openWidget(QStringLiteral("displays"), QStringLiteral("displays"));
        displaysUntouched(QStringLiteral("opened"));
        check(QStringLiteral("opened: no action"), log().isEmpty(), QString::fromUtf8(log().left(120)));
        at(DELL, 0, 0);
        at(S237, 1440, 407);
        at(S019, 4880, 407);
        at(AR37, 1440, 1847);
        check(QStringLiteral("no screen item for the screen that is off"), visibleWithPrefix(QStringLiteral("screen:") + AR15).isEmpty());
        prop(w, "dirty", true);
        prop(w, "keepSeconds", 15);

        // Riley check 3: a broken setting is refused, the live file untouched, no reload.
        call(w, "setScreenField", {DELL, QStringLiteral("scale"), QStringLiteral("abc")});
        clickNamed(QStringLiteral("displays:apply"));
        QTest::qWait(300);
        displaysUntouched(QStringLiteral("scale abc"));
        check(QStringLiteral("scale abc: no action"), log().isEmpty(), QString::fromUtf8(log().left(120)));
        check(QStringLiteral("scale abc: refused before any command"), calls("Hyprland ").isEmpty());
        messageShown(QStringLiteral("scale abc"));
        prop(w, "pending", false);
        call(w, "reset");
        clearMessage();

        // Hyprland's own checker refuses: nothing written.
        writeAll(m_fake + QStringLiteral("/state/verify-fail"), "1\n");
        clickNamed(QStringLiteral("displays:apply"));
        verifyCalls(1);
        QTest::qWait(300);
        displaysUntouched(QStringLiteral("checker refuses"));
        check(QStringLiteral("checker refuses: no action"), log().isEmpty(), QString::fromUtf8(log().left(120)));
        messageShown(QStringLiteral("checker refuses"));
        prop(w, "pending", false);
        QFile::remove(m_fake + QStringLiteral("/state/verify-fail"));
        clearMessage();

        // Riley check 1, 2, 4: apply writes expected-m1.lua (checked first), Go back restores exactly what was there.
        clickNamed(QStringLiteral("displays:apply"));
        logIs(QStringLiteral("displays-actions.log"), 2);
        verifyCalls(2);
        sameAs(QStringLiteral("apply: the checked candidate"), m_fake + QStringLiteral("/verify-2.lua"), QStringLiteral("expected-m1.lua"));
        displaysApplied(QStringLiteral("apply"));
        prop(w, "pending", true);
        prop(w, "secondsLeft", 15);
        clickNamed(QStringLiteral("displays:revert"));
        logIs(QStringLiteral("displays-actions.log"), 4);
        displaysUntouched(QStringLiteral("go back"));
        prop(w, "pending", false);

        if (!w) {
            check(QStringLiteral("the timed checks (no Keep within 15 s, Keep) need the page"), false);
            return;
        }
        // Riley: no Keep within 15 s goes back by itself.
        QElapsedTimer clock;
        clickNamed(QStringLiteral("displays:apply"));
        clock.start();
        logIs(QStringLiteral("displays-actions.log"), 6);
        verifyCalls(3);
        displaysApplied(QStringLiteral("apply again"));
        QTest::qWait(int(qMax<qint64>(0, 13000 - clock.elapsed())));
        prop(w, "pending", true);
        sameAs(QStringLiteral("13 s later: still applied, settings.lua"), cfg(QStringLiteral("hypr/settings.lua")), QStringLiteral("expected-m1.lua"));
        (void)QTest::qWaitFor([&] { return lineCount(log()) >= 8; }, int(qMax<qint64>(0, 18000 - clock.elapsed())));
        check(QStringLiteral("no Keep: gone back within 17 s"), lineCount(log()) >= 8 && clock.elapsed() <= 18000,
              QStringLiteral("%1 log lines after %2 ms").arg(lineCount(log())).arg(clock.elapsed()));
        logIs(QStringLiteral("displays-actions.log"), 8);
        displaysUntouched(QStringLiteral("no Keep"));
        prop(w, "pending", false);

        // Keep: stays applied, the timer stopped.
        clickNamed(QStringLiteral("displays:apply"));
        logIs(QStringLiteral("displays-actions.log"), 10);
        verifyCalls(4);
        clickNamed(QStringLiteral("displays:keep"));
        prop(w, "pending", false);
        QTest::qWait(16000);
        displaysApplied(QStringLiteral("keep, 16 s later"));
        logIs(QStringLiteral("displays-actions.log"), 10);
        prop(w, "dirty", false);

        // Drags (displays.py on_update, on_end), worked out by hand in docs/pages-contract.md.
        call(w, "dragMove", {AR37, 1680, 0});
        check(QStringLiteral("drag ARZOPA by (+1680, 0): dragEnd is true"), call(w, "dragEnd", {AR37}).toBool());
        at(AR37, 3173, 1847);
        prop(w, "dirty", true);
        call(w, "dragMove", {AR37, -1733, -500});
        check(QStringLiteral("drag ARZOPA by (-1733, -500) onto the ultrawide: dragEnd is false"), !call(w, "dragEnd", {AR37}).toBool());
        at(AR37, 3173, 1847);
        messageShown(QStringLiteral("overlap"));
        clearMessage();
        call(w, "reset");
        at(AR37, 1440, 1847);
        prop(w, "dirty", false);
        call(w, "dragMove", {DELL, -500, 0});
        check(QStringLiteral("drag the Dell by (-500, 0): dragEnd is true"), call(w, "dragEnd", {DELL}).toBool());
        at(DELL, 0, 0);
        at(S237, 1940, 407);
        call(w, "reset");
        at(S237, 1440, 407);
        sameAs(QStringLiteral("drags save nothing: values file"), m_values, QStringLiteral("displays-values-applied.xml"));
        logIs(QStringLiteral("displays-actions.log"), 10);
        noUnexpectedCall();
        homeHolds({QStringLiteral(".config/riley/settings.xml"), QStringLiteral(".config/hypr/settings.lua"),
                   QStringLiteral(".config/riley/greeter-sway.conf"), QStringLiteral(".config/hypr/hyprpaper.conf")});
        close();
    }

    // Riley gate_m2.py, with canned queues instead of a real one.
    void printers()
    {
        if (!begin(QStringLiteral("printers")) || !open())
            return;
        QQuickItem* w = openWidget(QStringLiteral("printers"), QStringLiteral("printers"));
        QString why;
        QQuickItem* gate = visible(QStringLiteral("printer:RileyGate"), &why);
        check(QStringLiteral("check 1: printer:RileyGate is listed"), gate, why);
        QQuickItem* office = visible(QStringLiteral("printer:Office"), &why);
        check(QStringLiteral("printer:Office is listed"), office, why);
        prop(gate, "printerState", QStringLiteral("idle"));
        prop(gate, "accepting", true);
        prop(gate, "printerInfo", QStringLiteral("Riley gate queue"));
        prop(gate, "printerLocation", QStringLiteral("Riley gate"));
        prop(gate, "isDefault", false);
        prop(office, "isDefault", true);

        call(w, "printTestPage", {QStringLiteral("RileyGate")});
        QQuickItem* job = visible(QStringLiteral("job:RileyGate-12"), &why);
        check(QStringLiteral("check 2: the test page is queued as job:RileyGate-12"), job, why);
        prop(job, "jobState", QStringLiteral("held"));
        check(QStringLiteral("check 2: it is the only job"), visibleWithPrefix(QStringLiteral("job:")).size() == 1,
              QStringLiteral("%1 job items").arg(visibleWithPrefix(QStringLiteral("job:")).size()));
        call(w, "cancelJob", {QStringLiteral("RileyGate-12")});
        (void)QTest::qWaitFor([&] { return visibleWithPrefix(QStringLiteral("job:")).isEmpty(); }, m_wait);
        check(QStringLiteral("check 2: cancelled, the queue is empty"), job && visibleWithPrefix(QStringLiteral("job:")).isEmpty());

        call(w, "setOption", {QStringLiteral("RileyGate"), QStringLiteral("print-color-mode"), QStringLiteral("monochrome")});
        QString back;
        (void)QTest::qWaitFor([&] {
            const QVariant v = call(w, "userOption", {QStringLiteral("RileyGate"), QStringLiteral("print-color-mode")});
            back = v.toString();
            return !v.isValid() || back == QLatin1String("monochrome");
        }, m_wait);
        check(QStringLiteral("check 3: black and white saved and read back"), back == QLatin1String("monochrome"),
              QStringLiteral("userOption \"%1\"").arg(back));

        call(w, "setDefault", {QStringLiteral("RileyGate")});
        gate = visible(QStringLiteral("printer:RileyGate"), &why);
        waitProp(gate, "isDefault", true);
        office = visible(QStringLiteral("printer:Office"), &why);
        prop(office, "isDefault", false);
        call(w, "setDefault", {QStringLiteral("Office")});
        office = visible(QStringLiteral("printer:Office"), &why);
        waitProp(office, "isDefault", true);

        call(w, "openSetupTool");
        logIs(QStringLiteral("printers-actions.log"), 1);
        QByteArray changing;
        for (const QByteArray& l : calls())
            if (l.startsWith("lp ") || l.startsWith("cancel ") || l.startsWith("lpoptions -d ")
                || (l.startsWith("lpoptions ") && l.contains(" -o ")))
                changing += l + '\n';
        check(QStringLiteral("check 4: the commands that change something are expected/printers-calls.log"),
              changing == expected(QStringLiteral("printers-calls.log")), firstDifference(changing, expected(QStringLiteral("printers-calls.log"))));
        noUnexpectedCall();
        absent(QStringLiteral("check 4: no ~/.cups in the home"), m_home + QStringLiteral("/.cups"));
        homeHolds({});
        close();
    }

    // Riley gate_m3.py check 1 (keyboard part): fixture-m3's input values set on the page give expected-m3.lua.
    void keyboardMouse()
    {
        if (!begin(QStringLiteral("keyboard-mouse")))
            return;
        seed(QStringLiteral("keyboard-values.xml"), m_values);
        if (!open())
            return;
        openWidget(QStringLiteral("input"), QStringLiteral("keyboard-mouse"));
        QQuickItem* layout = control(QStringLiteral("input.kb_layout"), QStringLiteral("text"), QStringLiteral("us"));
        QQuickItem* variant = control(QStringLiteral("input.kb_variant"), QStringLiteral("text"), QString());
        QQuickItem* options = control(QStringLiteral("input.kb_options"), QStringLiteral("text"), QString());
        QQuickItem* delay = control(QStringLiteral("input.repeat_delay"), QStringLiteral("slider"), QStringLiteral("600"));
        QQuickItem* rate = control(QStringLiteral("input.repeat_rate"), QStringLiteral("slider"), QStringLiteral("25"));
        QQuickItem* sens = control(QStringLiteral("input.sensitivity"), QStringLiteral("slider"), QStringLiteral("0"));
        QQuickItem* accel = control(QStringLiteral("input.accel_profile"), QStringLiteral("choice"), QStringLiteral("adaptive"));
        control(QStringLiteral("input.natural_scroll"), QStringLiteral("switch"), QStringLiteral("false"));
        control(QStringLiteral("input.follow_mouse"), QStringLiteral("choice"), QStringLiteral("1"));
        const QString L = QStringLiteral("keyboard-actions.log");

        type(layout, QStringLiteral("us,ro"));
        logIs(L, 2);
        type(variant, QStringLiteral(",std"));
        logIs(L, 3);
        type(options, QStringLiteral("grp:alt_shift_toggle"));
        logIs(L, 4);
        keys(rate, Qt::Key_Right, 5);
        logIs(L, 9);
        keys(delay, Qt::Key_Left, 20);
        logIs(L, 29);
        keys(sens, Qt::Key_Left, 4);
        logIs(L, 33);
        keys(accel, Qt::Key_Down, 1);
        logIs(L, 34);
        valueIs(layout, QStringLiteral("input.kb_layout"), QStringLiteral("us,ro"));
        valueIs(variant, QStringLiteral("input.kb_variant"), QStringLiteral(",std"));
        valueIs(options, QStringLiteral("input.kb_options"), QStringLiteral("grp:alt_shift_toggle"));
        valueIs(rate, QStringLiteral("input.repeat_rate"), QStringLiteral("30"));
        valueIs(delay, QStringLiteral("input.repeat_delay"), QStringLiteral("400"));
        valueIs(sens, QStringLiteral("input.sensitivity"), QStringLiteral("-0.2"));
        valueIs(accel, QStringLiteral("input.accel_profile"), QStringLiteral("flat"));
        sameAs(QStringLiteral("values file"), m_values, QStringLiteral("keyboard-values.xml"));
        sameAs(QStringLiteral("check 1: settings.lua"), cfg(QStringLiteral("hypr/settings.lua")), QStringLiteral("expected-m3.lua"));
        sameAs(QStringLiteral("greeter file"), cfg(QStringLiteral("riley/greeter-sway.conf")), QStringLiteral("keyboard-greeter.conf"));
        noUnexpectedCall();
        homeHolds({QStringLiteral(".config/riley/settings.xml"), QStringLiteral(".config/hypr/settings.lua"),
                   QStringLiteral(".config/riley/greeter-sway.conf")});
        close();
    }

    // Riley gate_m5.py checks 1 and 2 (the new m5 case from fixture-m1) and gate_m4.py check 4.
    void wallpapers()
    {
        if (!begin(QStringLiteral("wallpapers")))
            return;
        seed(QStringLiteral("wallpapers-values.xml"), m_values);
        if (!open())
            return;
        QQuickItem* w = openWidget(QStringLiteral("theme"), QStringLiteral("theme-wallpaper"));
        struct Probe {
            int x, y;
            QString desc;
        };
        const Probe probes[] = {{90, 148, DELL}, {328, 133, S237}, {700, 100, S019}, {244, 257, AR37}, {508, 267, QString()}, {5, 5, QString()}};
        for (const Probe& p : probes) {
            const QVariant v = call(w, "screenAt", {p.x, p.y, 852, 331});
            const QString got = v.toString();
            check(QStringLiteral("check 1: (%1, %2) on 852 x 331 is \"%3\"").arg(p.x).arg(p.y).arg(p.desc), v.isValid() && got == p.desc,
                  QStringLiteral("got \"%1\"").arg(got));
        }
        const QString L = QStringLiteral("wallpapers-actions.log");
        const QVariant notPicture = call(w, "dropPicture", {QStringLiteral("/srv/pictures/notes.txt"), call(w, "screenAt", {328, 133, 852, 331})});
        check(QStringLiteral("a .txt dropped on a screen is refused"), notPicture.isValid() && !notPicture.toBool());
        logStays(0);
        sameBytes(QStringLiteral("refused: values file is the seed"), m_values, readAll(m_seeds + QStringLiteral("/wallpapers-values.xml")));
        absent(QStringLiteral("refused: no hyprpaper.conf"), cfg(QStringLiteral("hypr/hyprpaper.conf")));
        messageShown(QStringLiteral("not a picture"));
        clearMessage();

        call(w, "dropPicture", {QStringLiteral("/srv/pictures/a.jpg"), call(w, "screenAt", {328, 133, 852, 331})});
        logIs(L, 1);
        sameAs(QStringLiteral("a.jpg on (328, 133): values file"), m_values, QStringLiteral("wallpapers-values-1.xml"));
        sameAs(QStringLiteral("a.jpg on (328, 133): hyprpaper.conf"), cfg(QStringLiteral("hypr/hyprpaper.conf")), QStringLiteral("wallpapers-hyprpaper-1.conf"));
        call(w, "dropPicture", {QStringLiteral("/srv/pictures/b.jpg"), call(w, "screenAt", {90, 148, 852, 331})});
        logIs(L, 2);
        sameAs(QStringLiteral("check 2: b.jpg on (90, 148): values file"), m_values, QStringLiteral("wallpapers-values-2.xml"));
        sameAs(QStringLiteral("check 2: two drops: hyprpaper.conf"), cfg(QStringLiteral("hypr/hyprpaper.conf")), QStringLiteral("wallpapers-hyprpaper-2.conf"));
        call(w, "useEverywhere", {QStringLiteral("/srv/pictures/c.jpg")});
        logIs(L, 3);
        sameAs(QStringLiteral("gate_m4 check 4: c.jpg everywhere: values file"), m_values, QStringLiteral("wallpapers-values-3.xml"));
        sameAs(QStringLiteral("gate_m4 check 4: c.jpg everywhere: hyprpaper.conf"), cfg(QStringLiteral("hypr/hyprpaper.conf")), QStringLiteral("wallpapers-hyprpaper-3.conf"));
        noUnexpectedCall();
        homeHolds({QStringLiteral(".config/riley/settings.xml"), QStringLiteral(".config/hypr/hyprpaper.conf")});
        close();
    }

    // Riley gate_m6.py (the 6 hand-worked drops), gate_m4.py check 3 (pins saved and loaded back), gate_dock_*.py.
    void dock()
    {
        if (!begin(QStringLiteral("dock")))
            return;
        seed(QStringLiteral("dock-values.xml"), m_values);
        if (!open())
            return;
        QQuickItem* w = openWidget(QStringLiteral("dock"), QStringLiteral("dock"));
        const QStringList start{FF, PT, DO, SE};
        check(QStringLiteral("the strip shows ") + show(start), w && strings(w->property("pins")) == start,
              QStringLiteral("pins ") + show(w ? strings(w->property("pins")) : QStringList()));
        QQuickItem* strip = need(QStringLiteral("dock:strip"));
        for (int i = 0; i < start.size(); ++i) {
            QString why;
            QQuickItem* pin = visible(QStringLiteral("pin:") + start[i], &why);
            const qreal c = pin && strip ? pin->mapToItem(strip, QPointF(pin->width() / 2, 0)).x() : -1;
            check(QStringLiteral("pin:%1 centred at x = %2 in the strip").arg(start[i]).arg(24 + 48 * i), pin && qAbs(c - (24 + 48 * i)) < 0.5,
                  pin ? QStringLiteral("centre %1").arg(c) : why);
        }
        const QStringList apps{DO, FF, PT, SE, ZED};
        check(QStringLiteral("the app list is ") + show(apps) + QStringLiteral(" (by name, NoDisplay and Hidden left out)"),
              w && strings(w->property("appIds")) == apps, QStringLiteral("appIds ") + show(w ? strings(w->property("appIds")) : QStringList()));
        check(QStringLiteral("no item for the NoDisplay or Hidden app"),
              visibleWithPrefix(QStringLiteral("app:riley.gate-")).isEmpty());

        call(w, "dropOnDock", {QStringLiteral("pin:") + PT, 200});
        dockStep(w, 1, QStringLiteral("dock-values-1.xml"), QStringLiteral("dock-pinned-1"), 1);
        call(w, "dropOnDock", {QStringLiteral("pin:") + SE, 10});
        dockStep(w, 2, QStringLiteral("dock-values-2.xml"), QStringLiteral("dock-pinned-2"), 2);
        call(w, "dropOnDock", {QStringLiteral("pin:") + FF, 60});
        call(w, "dropOnDock", {QStringLiteral("pin:") + FF, 90});
        logStays(2);
        dockStep(w, 3, QStringLiteral("dock-values-2.xml"), QStringLiteral("dock-pinned-2"), 2);
        call(w, "dropOnDock", {QStringLiteral("app:") + ZED, 100});
        dockStep(w, 4, QStringLiteral("dock-values-4.xml"), QStringLiteral("dock-pinned-4"), 3);
        call(w, "dropOnDock", {QStringLiteral("app:") + DO, 0});
        dockStep(w, 5, QStringLiteral("dock-values-5.xml"), QStringLiteral("dock-pinned-5"), 4);
        call(w, "dropOnRemove", {QStringLiteral("pin:") + FF});
        dockStep(w, 6, QStringLiteral("dock-values-6.xml"), QStringLiteral("dock-pinned-6"), 5);

        close();   // gate_m4 check 3: saved, then read back unchanged
        if (!open())
            return;
        w = openWidget(QStringLiteral("dock"), QStringLiteral("dock"));
        const QStringList last{DO, SE, ZED, PT};
        check(QStringLiteral("reopened: the pins read back as ") + show(last), w && strings(w->property("pins")) == last,
              QStringLiteral("pins ") + show(w ? strings(w->property("pins")) : QStringList()));

        const QString conf = cfg(QStringLiteral("nwg-dock-hyprland/riley.conf"));
        const QString L = QStringLiteral("dock-actions.log");
        // gate_dock_screens.py: size and screens keep every other byte; the file is replaced, not rewritten.
        seed(QStringLiteral("dock-conf-screens.conf"), conf);
        QFile original(conf);
        original.open(QIODevice::ReadOnly);
        QQuickItem* size = control(QStringLiteral("dock.icon_size"), QStringLiteral("slider"), QStringLiteral("44"));
        keys(size, Qt::Key_Right, 1);
        logIs(L, 6);
        sameAs(QStringLiteral("icon size 46: riley.conf"), conf, QStringLiteral("dock-conf-size46.conf"));
        keys(size, Qt::Key_Right, 1);
        logIs(L, 7);
        sameAs(QStringLiteral("icon size 48: riley.conf"), conf, QStringLiteral("dock-conf-sized.conf"));
        check(QStringLiteral("the old riley.conf was replaced, not truncated (an open copy still reads the seed)"),
              readAll(conf) == expected(QStringLiteral("dock-conf-sized.conf"))
                  && original.readAll() == readAll(m_seeds + QStringLiteral("/dock-conf-screens.conf")));
        call(w, "setScreens", {QStringList{S019}});
        logIs(L, 8);
        sameAs(QStringLiteral("one screen: riley.conf"), conf, QStringLiteral("dock-conf-one.conf"));
        call(w, "setScreens", {QStringList()});
        logIs(L, 9);
        sameAs(QStringLiteral("no screen: riley.conf keeps ICON_SIZE"), conf, QStringLiteral("dock-conf-empty.conf"));
        // gate_dock_reflections.py
        seed(QStringLiteral("dock-conf-reflections.conf"), conf);
        QQuickItem* refl = control(QStringLiteral("dock.reflections"), QStringLiteral("switch"), QStringLiteral("true"));
        QString why;
        if (refl)
            click(refl, &why);
        logIs(L, 10);
        sameAs(QStringLiteral("reflections off: riley.conf"), conf, QStringLiteral("dock-conf-reflections.conf"));
        // gate_dock_launcher.py
        seed(QStringLiteral("dock-conf-launcher.conf"), conf);
        QQuickItem* launcher = control(QStringLiteral("dock.launcher"), QStringLiteral("choice"), QStringLiteral("left"));
        keys(launcher, Qt::Key_Down, 1);
        logIs(L, 11);
        sameAs(QStringLiteral("launcher right: riley.conf"), conf, QStringLiteral("dock-conf-launcher.conf"));
        valueIs(launcher, QStringLiteral("dock.launcher"), QStringLiteral("right"));
        // gate_dock_autohide.py: only AUTOHIDE changes, and no dock restart
        seed(QStringLiteral("dock-conf-autohide.conf"), conf);
        QQuickItem* hide = control(QStringLiteral("dock.autohide"), QStringLiteral("switch"), QStringLiteral("false"));
        if (hide)
            click(hide, &why);
        QTest::qWait(300);
        const bool on = sameAs(QStringLiteral("auto-hide on: riley.conf"), conf, QStringLiteral("dock-conf-autohide.conf"));
        if (hide)
            click(hide, &why);
        QTest::qWait(300);
        check(QStringLiteral("auto-hide off: riley.conf is the seed again"),
              on && readAll(conf) == readAll(m_seeds + QStringLiteral("/dock-conf-autohide.conf")),
              on ? firstDifference(readAll(conf), readAll(m_seeds + QStringLiteral("/dock-conf-autohide.conf")))
                 : QStringLiteral("auto-hide on did not change riley.conf"));
        logStays(11);
        logIs(L, 11);
        noUnexpectedCall();
        homeHolds({QStringLiteral(".config/riley/settings.xml"), QStringLiteral(".config/nwg-dock-hyprland/riley.conf")});
        close();
    }

    // Riley desktop/tests/gate_d4.py checks 1 to 3, then the icon size and the switch for all boxes.
    void desktop()
    {
        if (!begin(QStringLiteral("desktop")))
            return;
        const QString desk = m_home + QStringLiteral("/Desktop"), projects = desk + QStringLiteral("/Projects"),
                      downloads = m_home + QStringLiteral("/Downloads");
        QDir().mkpath(projects);
        QDir().mkpath(downloads);
        if (!open())
            return;
        QQuickItem* w = openWidget(QStringLiteral("desktop"), QStringLiteral("desktop"));
        const QString D = QStringLiteral("Desktop 48 72 440 320"), P = QStringLiteral("Projects 48 408 340 240"),
                      DL = QStringLiteral("Downloads 48 664 340 240"), Z = QStringLiteral("Zeta 48 920 340 240");
        const QString L = QStringLiteral("desktop-actions.log");
        shownIs(w, QStringLiteral("opened"), {D, P});
        absent(QStringLiteral("opened: no desktop.json"), cfg(QStringLiteral("riley/desktop.json")));
        absent(QStringLiteral("opened: no values file"), m_values);

        const QVariant first = call(w, "addFolder", {downloads});
        logIs(L, 1);
        const QVariant again = call(w, "addFolder", {downloads});
        logStays(1);
        check(QStringLiteral("check 1: Downloads gets a box; the same folder twice is refused"),
              first.toBool() && again.isValid() && !again.toBool(), QStringLiteral("first %1, again %2").arg(first.toString(), again.toString()));
        shownIs(w, QStringLiteral("check 1"), {D, P, DL});
        desktopJsonIs(QStringLiteral("check 1"), QStringLiteral("desktop-json-1.json"));
        sameAs(QStringLiteral("check 1: values file"), m_values, QStringLiteral("desktop-values-1.xml"));

        call(w, "setHidden", {projects, true});
        logIs(L, 2);
        desktopJsonIs(QStringLiteral("check 2: Projects hidden"), QStringLiteral("desktop-json-2.json"));
        sameAs(QStringLiteral("check 2: Projects hidden: values file"), m_values, QStringLiteral("desktop-values-2.xml"));
        QDir().mkpath(desk + QStringLiteral("/Zeta"));
        call(w, "reload");
        shownIs(w, QStringLiteral("check 2: hidden keeps its spot, Zeta goes below Downloads"), {D, DL, Z});
        logStays(2);
        desktopJsonIs(QStringLiteral("check 2: reload writes nothing"), QStringLiteral("desktop-json-2.json"));
        call(w, "setHidden", {projects, false});
        logIs(L, 3);
        shownIs(w, QStringLiteral("check 2: shown again where it was"), {D, P, DL, Z});
        desktopJsonIs(QStringLiteral("check 2: shown again"), QStringLiteral("desktop-json-3.json"));
        sameAs(QStringLiteral("check 2: shown again: values file"), m_values, QStringLiteral("desktop-values-1.xml"));

        const QVariant gone = call(w, "removeBox", {downloads});
        logIs(L, 4);
        const QVariant kept = call(w, "removeBox", {projects});
        logStays(4);
        check(QStringLiteral("check 3: an added box goes; a ~/Desktop folder's box cannot be removed"),
              gone.toBool() && kept.isValid() && !kept.toBool(), QStringLiteral("gone %1, kept %2").arg(gone.toString(), kept.toString()));
        shownIs(w, QStringLiteral("check 3"), {D, P, Z});
        desktopJsonIs(QStringLiteral("check 3"), QStringLiteral("desktop-json-4.json"));
        sameAs(QStringLiteral("check 3: values file"), m_values, QStringLiteral("desktop-values-4.xml"));

        QQuickItem* size = control(QStringLiteral("desktop.icon_size"), QStringLiteral("slider"), QStringLiteral("48"));
        keys(size, Qt::Key_Right, 1);
        logIs(L, 5);
        desktopJsonIs(QStringLiteral("icon size 56"), QStringLiteral("desktop-json-5.json"));
        sameAs(QStringLiteral("icon size 56: values file"), m_values, QStringLiteral("desktop-values-5.xml"));
        QQuickItem* showAll = control(QStringLiteral("desktop.show"), QStringLiteral("switch"), QStringLiteral("true"));
        QString why;
        if (showAll)
            click(showAll, &why);
        logIs(L, 6);
        desktopJsonIs(QStringLiteral("all boxes off"), QStringLiteral("desktop-json-6.json"));
        sameAs(QStringLiteral("all boxes off: values file"), m_values, QStringLiteral("desktop-values-6.xml"));
        shownIs(w, QStringLiteral("all boxes off"), {});
        noUnexpectedCall();
        homeHolds({QStringLiteral(".config/riley/settings.xml"), QStringLiteral(".config/riley/desktop.json")});
        close();
    }

    // Riley app.py: the System entry opens hypr-settings and stays on the page you were on.
    void systemLauncher()
    {
        if (!begin(QStringLiteral("system-launcher")) || !open())
            return;
        showPage(QStringLiteral("power"));
        need(QStringLiteral("option:power.lock_after"));
        showPage(QStringLiteral("system"));
        logIs(QStringLiteral("system-actions.log"), 1);
        QTest::qWait(200);
        QString why;
        check(QStringLiteral("the Power page is still shown"), visible(QStringLiteral("option:power.lock_after"), &why, 1000), why);
        noUnexpectedCall();
        homeHolds({});
        close();
    }

    void init()
    {
        m_failedBefore = m_failed;
    }

    void cleanup()
    {
        close();
        QVERIFY2(m_failed == m_failedBefore, "checks of this page failed (see the FAIL lines)");
    }

    void cleanupTestCase()
    {
        close();
        m_group = QStringLiteral("harness");
        if (m_results.isOpen())
            report(QStringLiteral("DONE %1").arg(m_checks));
    }
};

QTEST_MAIN(TestPages)
#include "tst_pages.moc"
