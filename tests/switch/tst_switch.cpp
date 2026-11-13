// The M4 gate's harness (tests/run-switch.sh): the parts of the switch that need the panel drawn. It drives the
// panel the way docs/switch-contract.md says and compares the values file, the target files, the action log and the
// commands run (through fakes first on PATH) with tests/switch/expected/, worked out by hand from Riley's code
// before the code existed. Every check's name starts with its group ("layout: ...").
//
// Groups:
//   pages    every page of the real description shown once, from an empty home: no QML warning, nothing written
//   layout   Riley's keyboard layout picker (keyboard_mouse.py on_layout): several keys in one step
//   printers the printer options Riley's printers page offers (printers_core.py OPTIONS, option_choices)
//
// Environment (set by run-switch.sh): HOME (= $RILEY_SWITCH_RUN/home, wiped and seeded before each group),
// XDG_CONFIG_HOME, XDG_CACHE_HOME, RILEY_SETTINGS_SHARE, RILEY_SETTINGS_ACTION_LOG, RILEY_FAKE_DIR, RILEY_SWITCH_RUN
// (also holds layouts.txt and variants-<code>.txt, which the gate reads from the system's evdev.lst with Riley's
// rule), RILEY_SWITCH_EXPECTED, RILEY_SWITCH_SEEDS, RILEY_SWITCH_RESULTS (one "PASS name" or "FAIL name" line per
// check, a failure's detail on the next line, indented; the last line is "DONE <n>").
#include "panel/panel.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

#include <cstdio>
#include <functional>

namespace {

QStringList g_warnings;
QtMessageHandler g_previous = nullptr;

// Every warning the QML engine prints (a binding or JavaScript error in a .qml file) is kept for the group's
// "no QML warning" check, then passed on.
void keepQmlWarnings(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString category = QString::fromLatin1(context.category ? context.category : "");
    if (type != QtDebugMsg && type != QtInfoMsg
        && (message.contains(QLatin1String(".qml")) || category.startsWith(QLatin1String("qml"))
            || category.startsWith(QLatin1String("js")) || message.contains(QLatin1String("TypeError"))
            || message.contains(QLatin1String("ReferenceError"))))
        g_warnings.append(message.left(300));
    if (g_previous)
        g_previous(type, context, message);
}

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

QStringList lines(const QByteArray& text)
{
    QStringList out;
    for (const QByteArray& l : text.split('\n'))
        if (!l.isEmpty())
            out.append(QString::fromUtf8(l));
    return out;
}

}  // namespace

class TestSwitch : public QObject {
    Q_OBJECT

private:
    QQmlApplicationEngine* m_engine = nullptr;
    QQuickWindow* m_window = nullptr;
    QString m_run, m_home, m_config, m_cache, m_values, m_log, m_fake, m_expected, m_seeds;
    QFile m_results;
    int m_checks = 0;
    int m_failed = 0;
    int m_failedBefore = 0;
    int m_wait = 5000;
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
        report((ok ? QStringLiteral("PASS ") : QStringLiteral("FAIL ")) + m_group + QStringLiteral(": ") + name);
        if (!ok && !detail.isEmpty())
            report(QStringLiteral("     ") + detail.left(400));
        return ok;
    }

    QByteArray expected(const QString& name) const
    {
        return readAll(m_expected + QLatin1Char('/') + name).replace("@HOME@", m_home.toUtf8());
    }

    bool sameAs(const QString& what, const QString& path, const QString& expectedFile)
    {
        bool exists = false;
        const QByteArray got = readAll(path, &exists);
        const QByteArray want = expected(expectedFile);
        const QString name = what + QStringLiteral(" is expected/") + expectedFile;
        if (!exists)
            return check(name, false, QStringLiteral("no file ") + path);
        return check(name, got == want, firstDifference(got, want));
    }

    QString cfg(const QString& rel) const
    {
        return m_config + QLatin1Char('/') + rel;
    }

    QByteArray log() const
    {
        return readAll(m_log);
    }

    bool logIs(const QString& expectedFile, int n)
    {
        (void)QTest::qWaitFor([&] { return lineCount(log()) >= n; }, m_wait);
        QTest::qWait(50);
        const QByteArray want = firstLines(expected(expectedFile), n);
        return check(QStringLiteral("action log is the first %1 lines of expected/%2").arg(n).arg(expectedFile),
                     log() == want, firstDifference(log(), want));
    }

    bool logStays(int n)
    {
        QTest::qWait(300);
        return check(QStringLiteral("action log still has %1 lines").arg(n), lineCount(log()) == n,
                     QStringLiteral("%1 lines: %2").arg(lineCount(log())).arg(QString::fromUtf8(log().right(120))));
    }

    QList<QByteArray> calls() const
    {
        QList<QByteArray> out;
        for (const QByteArray& l : readAll(m_fake + QStringLiteral("/calls.log")).split('\n'))
            if (!l.isEmpty())
                out.append(l);
        return out;
    }

    bool noUnexpectedCall()
    {
        QByteArray first;
        for (const QByteArray& l : calls())
            if (l.startsWith("UNEXPECTED ") && first.isEmpty())
                first = l;
        return check(QStringLiteral("no command outside the contract's list was run"), first.isEmpty(), QString::fromUtf8(first));
    }

    bool noQmlWarning()
    {
        return check(QStringLiteral("no QML warning while the group ran"), g_warnings.isEmpty(),
                     g_warnings.isEmpty() ? QString() : QStringLiteral("%1 warnings, the first: %2").arg(g_warnings.size()).arg(g_warnings.first()));
    }

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
            *why = QStringLiteral("%1 has its centre at %2,%3, outside the window").arg(item->objectName()).arg(c.x()).arg(c.y());
            return false;
        }
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, c.toPoint());
        return true;
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

    bool valueIs(const QString& key, const QString& value)
    {
        QString why;
        QQuickItem* i = visible(QStringLiteral("option:") + key, &why, 1000);
        const QString got = i ? i->property("valueText").toString() : why;
        return check(QStringLiteral("option:") + key + QStringLiteral(" shows \"") + value + QLatin1Char('"'), i && got == value,
                     QStringLiteral("valueText \"%1\"").arg(got));
    }

    // Calls a QML function of the contract on an item, with untyped arguments (up to 5). A missing function is one
    // failed check, unless `quiet` (a wait that polls it): then it only returns an invalid QVariant.
    QVariant call(QQuickItem* item, const char* fn, const QVariantList& a = {}, bool quiet = false)
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
            case 5:
                ok = QMetaObject::invokeMethod(item, fn, Q_RETURN_ARG(QVariant, r), Q_ARG(QVariant, a[0]), Q_ARG(QVariant, a[1]),
                                               Q_ARG(QVariant, a[2]), Q_ARG(QVariant, a[3]), Q_ARG(QVariant, a[4]));
                break;
            default: break;
            }
        }
        if (!ok && !quiet) {
            ++m_checks;
            ++m_failed;
            report(QStringLiteral("FAIL ") + m_group + QStringLiteral(": ") + (item ? item->objectName() : QStringLiteral("<no item>"))
                   + QStringLiteral(" has a function ") + QString::fromLatin1(fn) + QStringLiteral("(%1 arguments)").arg(a.size()));
            return QVariant();
        }
        return r;
    }

    QStringList strings(const QVariant& v) const
    {
        QStringList out;
        for (const QVariant& x : v.toList())
            out.append(x.toString());
        return out;
    }

    bool listIs(const QString& what, const QStringList& got, const QStringList& want)
    {
        QString detail = QStringLiteral("got %1 items, want %2").arg(got.size()).arg(want.size());
        for (qsizetype i = 0; i < qMax(got.size(), want.size()); ++i) {
            const QString g = i < got.size() ? got[i] : QStringLiteral("<none>");
            const QString w = i < want.size() ? want[i] : QStringLiteral("<none>");
            if (g != w) {
                detail += QStringLiteral("; item %1: got \"%2\", want \"%3\"").arg(i + 1).arg(g, w);
                break;
            }
        }
        return check(what + QStringLiteral(" is ") + (want.size() <= 6 ? show(want) : QStringLiteral("%1 items").arg(want.size())),
                     got == want, detail);
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
        QTest::qWait(200);
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

    bool begin(const QString& group)
    {
        close();
        m_group = group;
        m_wait = 5000;
        g_warnings.clear();
        if (m_home != m_run + QStringLiteral("/home") || !QFile::exists(m_run + QStringLiteral("/.riley-switch-run"))) {
            check(QStringLiteral("harness: the throwaway home is $RILEY_SWITCH_RUN/home"), false, m_home);
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
        return true;
    }

    bool seed(const QString& seedFile, const QString& to)
    {
        return check(QStringLiteral("harness: seed ") + seedFile, writeAll(to, readAll(m_seeds + QLatin1Char('/') + seedFile)));
    }

    // One step of the layout picker that must be refused: false, a reason, nothing written, nothing recorded.
    void refused(QQuickItem* w, const QVariantList& args, const QString& what, int logLines)
    {
        clearMessage();
        const QVariant r = call(w, "chooseLayouts", args);
        check(what + QStringLiteral(": chooseLayouts returns false"), r.isValid() && !r.toBool(),
              QStringLiteral("returned %1").arg(r.toString()));
        messageShown(what);
        sameAs(what + QStringLiteral(": values file"), m_values, QStringLiteral("layout-values-3.xml"));
        logStays(logLines);
    }

    // Waits for a QML function's answer to become `want` (the page reads the queue's options again after a change).
    bool answerIs(QQuickItem* w, const char* fn, const QVariantList& args, const QString& want, const QString& what)
    {
        QString got;
        (void)QTest::qWaitFor([&] {
            const QVariant v = call(w, fn, args, true);
            got = v.toString();
            return !v.isValid() || got == want;
        }, m_wait);
        return check(what + QStringLiteral(" is \"") + want + QLatin1Char('"'), got == want, QStringLiteral("got \"%1\"").arg(got));
    }

private Q_SLOTS:
    void initTestCase()
    {
        m_run = env("RILEY_SWITCH_RUN");
        m_home = env("HOME");
        m_config = env("XDG_CONFIG_HOME");
        m_cache = env("XDG_CACHE_HOME");
        m_values = m_config + QStringLiteral("/riley/settings.xml");
        m_log = env("RILEY_SETTINGS_ACTION_LOG");
        m_fake = env("RILEY_FAKE_DIR");
        m_expected = env("RILEY_SWITCH_EXPECTED");
        m_seeds = env("RILEY_SWITCH_SEEDS");
        m_results.setFileName(env("RILEY_SWITCH_RESULTS"));
        QVERIFY2(m_results.open(QIODevice::WriteOnly | QIODevice::Truncate), "RILEY_SWITCH_RESULTS cannot be written");
        g_previous = qInstallMessageHandler(keepQmlWarnings);
    }

    // FontControl.qml's "Cannot read property 'family' of undefined" (report 3) and any other QML warning: every page
    // of the real description is shown once (the system launcher entry is left out: it launches, see M3).
    void pages()
    {
        if (!begin(QStringLiteral("pages")) || !open())
            return;
        QStringList ids;
        for (QQuickItem* e : visibleWithPrefix(QStringLiteral("page:")))
            ids.append(e->objectName().mid(5));
        // 12: Riley's 10 pages, the system launcher, and the Keys page (keys brief 2026-11-13).
        check(QStringLiteral("the menu has 12 entries"), ids.size() == 12, show(ids));
        for (const QString& id : ids)
            if (id != QLatin1String("system")) {
                showPage(id);
                QTest::qWait(150);
            }
        noQmlWarning();
        check(QStringLiteral("nothing recorded in the action log"), log().isEmpty(), QString::fromUtf8(log().left(120)));
        noUnexpectedCall();
        homeHolds({});
        close();
    }

    // Riley keyboard_mouse.py on_layout: first layout, optional second with a switch key, kb_variant and kb_options
    // set with kb_layout in one step. Seed: fixture-m1's screens, kb_layout de, kb_options caps:escape,grp:caps_toggle.
    void layout()
    {
        if (!begin(QStringLiteral("layout")))
            return;
        seed(QStringLiteral("layout-values.xml"), m_values);
        if (!open())
            return;
        QQuickItem* w = openWidget(QStringLiteral("input"), QStringLiteral("keyboard-mouse"));
        listIs(QStringLiteral("layoutCodes (evdev.lst's layouts by name, Riley's layouts())"), strings(w ? w->property("layoutCodes") : QVariant()),
               lines(readAll(m_run + QStringLiteral("/layouts.txt"))));
        listIs(QStringLiteral("switchKeys (Riley's SWITCH_KEYS)"), strings(w ? w->property("switchKeys") : QVariant()),
               {QStringLiteral("grp:alt_shift_toggle"), QStringLiteral("grp:ctrl_shift_toggle"), QStringLiteral("grp:caps_toggle")});
        for (const QString& code : {QStringLiteral("ro"), QStringLiteral("de")})
            listIs(QStringLiteral("variantsFor(\"%1\")").arg(code), strings(call(w, "variantsFor", {code})),
                   QStringList{QString()} + lines(readAll(m_run + QStringLiteral("/variants-%1.txt").arg(code))));
        const QString L = QStringLiteral("layout-actions.log");

        // Step 1: English (US) and Romanian (standard), Alt+Shift switches; caps:escape is kept, grp:caps_toggle goes.
        QVariant r = call(w, "chooseLayouts", {QStringLiteral("us"), QString(), QStringLiteral("ro"), QStringLiteral("std"),
                                               QStringLiteral("grp:alt_shift_toggle")});
        check(QStringLiteral("step 1: chooseLayouts returns true"), r.toBool(), r.toString());
        sameAs(QStringLiteral("step 1: values file"), m_values, QStringLiteral("layout-values-1.xml"));
        sameAs(QStringLiteral("step 1: settings.lua"), cfg(QStringLiteral("hypr/settings.lua")), QStringLiteral("layout-settings-1.lua"));
        sameAs(QStringLiteral("step 1: settings.json"), cfg(QStringLiteral("hypr/settings.json")), QStringLiteral("layout-model-1.json"));
        logIs(L, 2);
        valueIs(QStringLiteral("input.kb_layout"), QStringLiteral("us,ro"));
        valueIs(QStringLiteral("input.kb_variant"), QStringLiteral(",std"));
        valueIs(QStringLiteral("input.kb_options"), QStringLiteral("caps:escape,grp:alt_shift_toggle"));

        // Step 2: back to one layout: kb_layout and kb_variant at their defaults (removed), the switch key goes.
        r = call(w, "chooseLayouts", {QStringLiteral("us"), QString(), QString(), QString(), QStringLiteral("grp:alt_shift_toggle")});
        check(QStringLiteral("step 2: chooseLayouts returns true"), r.toBool(), r.toString());
        sameAs(QStringLiteral("step 2: values file"), m_values, QStringLiteral("layout-values-2.xml"));
        sameAs(QStringLiteral("step 2: settings.lua"), cfg(QStringLiteral("hypr/settings.lua")), QStringLiteral("layout-settings-2.lua"));
        sameAs(QStringLiteral("step 2: settings.json"), cfg(QStringLiteral("hypr/settings.json")), QStringLiteral("layout-model-2.json"));
        logIs(L, 4);

        // Step 3: the same layout with another switch key: with no second layout nothing changes.
        r = call(w, "chooseLayouts", {QStringLiteral("us"), QString(), QString(), QString(), QStringLiteral("grp:caps_toggle")});
        check(QStringLiteral("step 3: chooseLayouts returns true"), r.toBool(), r.toString());
        sameAs(QStringLiteral("step 3: values file unchanged"), m_values, QStringLiteral("layout-values-2.xml"));
        logStays(4);

        // Step 4: German (no dead keys) and French, Ctrl+Shift switches.
        r = call(w, "chooseLayouts", {QStringLiteral("de"), QStringLiteral("nodeadkeys"), QStringLiteral("fr"), QString(),
                                      QStringLiteral("grp:ctrl_shift_toggle")});
        check(QStringLiteral("step 4: chooseLayouts returns true"), r.toBool(), r.toString());
        sameAs(QStringLiteral("step 4: values file"), m_values, QStringLiteral("layout-values-3.xml"));
        logIs(L, 6);
        valueIs(QStringLiteral("input.kb_variant"), QStringLiteral("nodeadkeys,"));

        refused(w, {QStringLiteral("zz"), QString(), QString(), QString(), QString()}, QStringLiteral("an unknown layout"), 6);
        refused(w, {QStringLiteral("us"), QString(), QStringLiteral("ro"), QString(), QStringLiteral("grp:bogus")},
                QStringLiteral("an unknown switch key"), 6);
        refused(w, {QStringLiteral("us"), QStringLiteral("nosuchvariant"), QString(), QString(), QString()},
                QStringLiteral("a variant the layout does not have"), 6);
        noQmlWarning();
        noUnexpectedCall();
        homeHolds({QStringLiteral(".config/riley/settings.xml"), QStringLiteral(".config/hypr/settings.lua"),
                   QStringLiteral(".config/hypr/settings.json")});
        close();
    }

    // Riley printers_core.py option_choices and printers.py printer_group: paper size, colour, quality and two-sided,
    // each when the queue offers more than one; your value, or the printer's default, or the first choice.
    void printers()
    {
        if (!begin(QStringLiteral("printers")))
            return;
        writeAll(m_fake + QStringLiteral("/state/options-Office"), "print-color-mode=auto\n");   // not one of Office's choices
        if (!open())
            return;
        QQuickItem* w = openWidget(QStringLiteral("printers"), QStringLiteral("printers"));
        const QString O = QStringLiteral("Office"), G = QStringLiteral("RileyGate");
        const QString media = QStringLiteral("media"), colour = QStringLiteral("print-color-mode"),
                      quality = QStringLiteral("print-quality"), sides = QStringLiteral("sides");
        (void)QTest::qWaitFor([&] {
            const QVariant v = call(w, "optionNames", {O}, true);
            return !v.isValid() || strings(v).size() == 3;
        }, m_wait);
        listIs(QStringLiteral("optionNames(Office)"), strings(call(w, "optionNames", {O})), {media, colour, quality});
        listIs(QStringLiteral("optionNames(RileyGate)"), strings(call(w, "optionNames", {G})), {media, colour, sides});
        struct Want { QString queue, option; QStringList choices, labels; QString value; };
        const QList<Want> table = {
            {O, media, {QStringLiteral("iso_a4_210x297mm"), QStringLiteral("na_letter_8.5x11in"), QStringLiteral("iso_a5_148x210mm")},
             {QStringLiteral("A4"), QStringLiteral("Letter"), QStringLiteral("A5")}, QStringLiteral("iso_a4_210x297mm")},
            {O, colour, {QStringLiteral("monochrome"), QStringLiteral("color")}, {QStringLiteral("Black and white"), QStringLiteral("Colour")},
             QStringLiteral("monochrome")},
            {O, quality, {QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5")},
             {QStringLiteral("Draft"), QStringLiteral("Normal"), QStringLiteral("Best")}, QStringLiteral("4")},
            {G, media, {QStringLiteral("na_letter_8.5x11in"), QStringLiteral("iso_a4_210x297mm")}, {QStringLiteral("Letter"), QStringLiteral("A4")},
             QStringLiteral("na_letter_8.5x11in")},
            {G, colour, {QStringLiteral("monochrome"), QStringLiteral("color")}, {QStringLiteral("Black and white"), QStringLiteral("Colour")},
             QStringLiteral("monochrome")},
            {G, sides, {QStringLiteral("one-sided"), QStringLiteral("two-sided-long-edge"), QStringLiteral("two-sided-short-edge")},
             {QStringLiteral("One-sided"), QStringLiteral("Two-sided, long edge"), QStringLiteral("Two-sided, short edge")},
             QStringLiteral("one-sided")},
        };
        for (const Want& t : table) {
            const QString at = QStringLiteral("(%1, %2)").arg(t.queue, t.option);
            listIs(QStringLiteral("optionChoices") + at, strings(call(w, "optionChoices", {t.queue, t.option})), t.choices);
            listIs(QStringLiteral("optionLabels") + at, strings(call(w, "optionLabels", {t.queue, t.option})), t.labels);
            const QString v = call(w, "optionValue", {t.queue, t.option}).toString();
            check(QStringLiteral("optionValue") + at + QStringLiteral(" is \"") + t.value + QLatin1Char('"'), v == t.value,
                  QStringLiteral("got \"%1\"").arg(v));
            QString why;
            check(QStringLiteral("printer-option:%1:%2 is shown").arg(t.queue, t.option),
                  visible(QStringLiteral("printer-option:%1:%2").arg(t.queue, t.option), &why, 1000), why);
        }
        const int shown = int(visibleWithPrefix(QStringLiteral("printer-option:")).size());
        check(QStringLiteral("6 printer-option items in all"), shown == 6, QStringLiteral("%1 items").arg(shown));

        call(w, "setOption", {O, media, QStringLiteral("na_letter_8.5x11in")});
        answerIs(w, "optionValue", {O, media}, QStringLiteral("na_letter_8.5x11in"), QStringLiteral("Office's paper size, read back,"));
        call(w, "setOption", {O, quality, QStringLiteral("5")});
        answerIs(w, "optionValue", {O, quality}, QStringLiteral("5"), QStringLiteral("Office's quality, read back,"));
        call(w, "setOption", {G, sides, QStringLiteral("two-sided-long-edge")});
        answerIs(w, "optionValue", {G, sides}, QStringLiteral("two-sided-long-edge"), QStringLiteral("RileyGate's two-sided, read back,"));
        QByteArray changing;
        for (const QByteArray& l : calls())
            if (l.startsWith("lpoptions ") && l.contains(" -o "))
                changing += l + '\n';
        check(QStringLiteral("the option changes ran expected/printers-option-calls.log"),
              changing == expected(QStringLiteral("printers-option-calls.log")),
              firstDifference(changing, expected(QStringLiteral("printers-option-calls.log"))));
        check(QStringLiteral("nothing recorded in the action log"), log().isEmpty(), QString::fromUtf8(log().left(120)));
        noQmlWarning();
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
        QVERIFY2(m_failed == m_failedBefore, "checks of this group failed (see the FAIL lines)");
    }

    void cleanupTestCase()
    {
        close();
        m_group = QStringLiteral("harness");
        if (m_results.isOpen())
            report(QStringLiteral("DONE %1").arg(m_checks));
    }
};

QTEST_MAIN(TestSwitch)
#include "tst_switch.moc"
