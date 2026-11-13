// The M2 gate's harness (tests/run-panel.sh): loads the panel with the test description (tests/panel/settings.xml),
// checks that every option became the right control, then makes one change per option the way a user would and
// compares the values file, the action log and the target files byte for byte with tests/panel/expected/.
// How it finds and drives the controls: docs/panel-contract.md.
//
// Environment (set by run-panel.sh): RILEY_SETTINGS_SHARE, XDG_CONFIG_HOME, RILEY_SETTINGS_ACTION_LOG (read by the
// panel), RILEY_PANEL_EXPECTED (the expected directory), RILEY_PANEL_RESULTS (one "PASS name" or "FAIL name" line
// per check is appended there, the detail of a failure on the next line, indented; the last line is "DONE <n>").
#include "panel/panel.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

#include <cstdio>
#include <vector>

namespace {

QString env(const char* name)
{
    return qEnvironmentVariable(name);
}

enum class How { Click, KeyDown, KeyRight, Type, Commit, Items };

struct Option {
    const char* page;
    const char* key;
    const char* type;
    const char* defaultText;   // valueText at load; nullptr for a list (itemCount 0 instead)
};

// The drawn options of tests/panel/settings.xml, in description order.
const std::vector<Option> kOptions = {
    {"basic", "test.switch", "switch", "false"},
    {"basic", "power.lock_after", "choice", "0"},
    {"basic", "test.slider", "slider", "5"},
    {"basic", "test.number", "number", "1.5"},
    {"basic", "test.text", "text", "hello"},
    {"looks", "test.colour", "colour", "#336699"},
    {"looks", "test.font", "font", "Sans 11"},
    {"looks", "wallpaper.picture", "file", "/usr/share/backgrounds/riley/default.png"},
    {"looks", "test.folder", "folder", "/srv/test/default"},
    {"looks", "test.keybinding", "keybinding", "SUPER+K"},
    {"looks", "wallpaper.screens", "list", nullptr},
};

// The widget page's options: no control anywhere.
const char* const kWidgetKeys[] = {"display.monitors", "power.screensaver_after", "power.screens_off_after",
                                   "power.sleep_after"};

struct Target {
    const char* file;       // under $XDG_CONFIG_HOME
    const char* expected;   // under $RILEY_PANEL_EXPECTED
};

struct Step {
    const char* page;
    const char* key;
    How how;
    const char* input;
    const char* valueText;   // nullptr for a list
    int itemCount;           // list only
    int logLines;            // lines of expected/actions.log after this step
    std::vector<Target> targets;
};

// The gate's sequence (docs/panel-contract.md, "The gate's sequence"). The results were worked out by hand.
const std::vector<Step> kSteps = {
    {"basic", "test.switch", How::Click, "", "true", 0, 1, {{"hypr/idle.conf", "idle-01.conf"}}},
    {"basic", "power.lock_after", How::KeyDown, "", "300", 0, 2, {{"hypr/idle.conf", "idle-02.conf"}}},
    {"basic", "test.slider", How::KeyRight, "", "6", 0, 3, {}},
    {"basic", "test.number", How::Type, "42.25", "42.25", 0, 4, {{"hypr/hyprpaper.conf", "hyprpaper-04.conf"}}},
    {"basic", "test.text", How::Type, "a & b", "a & b", 0, 5, {}},
    {"looks", "test.colour", How::Commit, "#1a2b3c", "#1a2b3c", 0, 6, {}},
    {"looks", "test.font", How::Commit, "Noto Sans 12", "Noto Sans 12", 0, 7, {}},
    {"looks", "wallpaper.picture", How::Commit, "/usr/share/backgrounds/riley/sea.png",
     "/usr/share/backgrounds/riley/sea.png", 0, 8, {{"hypr/hyprpaper.conf", "hyprpaper-08.conf"}}},
    {"looks", "test.folder", How::Commit, "/srv/test/photos", "/srv/test/photos", 0, 10, {}},
    {"looks", "test.keybinding", How::Commit, "SUPER+SHIFT+K", "SUPER+SHIFT+K", 0, 12, {}},
    {"looks", "wallpaper.screens", How::Items, "", nullptr, 1, 14, {{"hypr/hyprpaper.conf", "hyprpaper-11.conf"}}},
    {"basic", "test.switch", How::Click, "", "false", 0, 15, {{"hypr/idle.conf", "idle-02.conf"}}},
};

QByteArray readAll(const QString& path, bool* exists = nullptr)
{
    QFile f(path);
    const bool ok = f.open(QIODevice::ReadOnly);
    if (exists)
        *exists = ok;
    return ok ? f.readAll() : QByteArray();
}

// The first line where two texts differ, for a failure's detail.
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

// The first n lines of a text, each with its newline.
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

int lineCount(const QByteArray& text)
{
    return int(text.count('\n'));
}

void collect(QQuickItem* item, const QString& name, QList<QQuickItem*>* out)
{
    if (!item)
        return;
    if (item->objectName() == name)
        out->append(item);
    for (QQuickItem* c : item->childItems())
        collect(c, name, out);
}

}  // namespace

class TestPanel : public QObject {
    Q_OBJECT

private:
    QQmlApplicationEngine* m_engine = nullptr;
    QQuickWindow* m_window = nullptr;
    QString m_values;
    QString m_log;
    QString m_config;
    QString m_expected;
    QFile m_results;
    int m_checks = 0;
    int m_failed = 0;
    QString m_page;   // the page shown, as far as the harness knows

    void report(const QString& line)
    {
        m_results.write(line.toUtf8() + '\n');
        m_results.flush();
        std::fprintf(stdout, "%s\n", line.toUtf8().constData());
    }

    bool check(const QString& name, bool ok, const QString& detail = QString())
    {
        ++m_checks;
        if (ok) {
            report(QStringLiteral("PASS ") + name);
        } else {
            ++m_failed;
            report(QStringLiteral("FAIL ") + name);
            if (!detail.isEmpty())
                report(QStringLiteral("     ") + detail);
        }
        return ok;
    }

    bool sameBytes(const QString& name, const QString& path, const QByteArray& want)
    {
        bool exists = false;
        const QByteArray got = readAll(path, &exists);
        if (!exists)
            return check(name, false, QStringLiteral("no file ") + path);
        return check(name, got == want, firstDifference(got, want));
    }

    QList<QQuickItem*> all(const QString& name) const
    {
        QList<QQuickItem*> out;
        if (m_window)
            collect(m_window->contentItem(), name, &out);
        return out;
    }

    // The one visible item with this objectName, waiting up to 5 s for it; nullptr and *why otherwise.
    QQuickItem* visible(const QString& name, QString* why)
    {
        QList<QQuickItem*> shown;
        (void)QTest::qWaitFor(
            [&] {
                shown.clear();
                for (QQuickItem* i : all(name))
                    if (i->isVisible())
                        shown.append(i);
                return shown.size() == 1;
            },
            5000);
        if (shown.size() == 1)
            return shown.first();
        *why = QStringLiteral("%1 visible items named %2 (want 1)").arg(shown.size()).arg(name);
        return nullptr;
    }

    QPoint centre(QQuickItem* item, QString* why) const
    {
        const QPointF c = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
        if (item->width() <= 0 || item->height() <= 0 || c.x() < 0 || c.y() < 0 || c.x() >= m_window->width()
            || c.y() >= m_window->height()) {
            *why = QStringLiteral("%1 has its centre at %2,%3 (size %4x%5), outside the %6x%7 window")
                       .arg(item->objectName())
                       .arg(c.x())
                       .arg(c.y())
                       .arg(item->width())
                       .arg(item->height())
                       .arg(m_window->width())
                       .arg(m_window->height());
            return QPoint(-1, -1);
        }
        return c.toPoint();
    }

    bool click(QQuickItem* item, QString* why)
    {
        const QPoint p = centre(item, why);
        if (p.x() < 0)
            return false;
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, p);
        return true;
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

    bool showPage(const QString& id, QString* why)
    {
        QQuickItem* entry = visible(QStringLiteral("page:") + id, why);
        if (!entry || !click(entry, why))
            return false;
        m_page = id;
        return true;
    }

    // Makes one change, the way docs/panel-contract.md says for its type.
    bool drive(const Step& s, QQuickItem* item, QString* why)
    {
        const QString input = QString::fromUtf8(s.input);
        switch (s.how) {
        case How::Click:
            return click(item, why);
        case How::KeyDown:
            if (!focus(item, why))
                return false;
            QTest::keyClick(m_window, Qt::Key_Down);
            return true;
        case How::KeyRight:
            if (!focus(item, why))
                return false;
            QTest::keyClick(m_window, Qt::Key_Right);
            return true;
        case How::Type:
            if (!focus(item, why))
                return false;
            QTest::keyClick(m_window, Qt::Key_A, Qt::ControlModifier);
            for (const QChar c : input)   // QTest has no keyClicks for a QWindow: one key per character
                QTest::keyClick(m_window, c.toLatin1());
            QTest::keyClick(m_window, Qt::Key_Return);
            return true;
        case How::Commit:
            if (!QMetaObject::invokeMethod(item, "commitValue", Q_ARG(QVariant, QVariant(input)))) {
                *why = item->objectName() + QStringLiteral(" has no commitValue(value)");
                return false;
            }
            return true;
        case How::Items: {
            QVariantMap one;
            one.insert(QStringLiteral("screen"), QStringLiteral("DP-1"));
            one.insert(QStringLiteral("path"), QStringLiteral("/usr/share/backgrounds/riley/dp1.png"));
            const QVariantList items{one};
            if (!QMetaObject::invokeMethod(item, "commitItems", Q_ARG(QVariant, QVariant(items)))) {
                *why = item->objectName() + QStringLiteral(" has no commitItems(items)");
                return false;
            }
            return true;
        }
        }
        return false;
    }

private Q_SLOTS:
    void initTestCase()
    {
        m_config = env("XDG_CONFIG_HOME");
        m_values = m_config + QStringLiteral("/riley/settings.xml");
        m_log = env("RILEY_SETTINGS_ACTION_LOG");
        m_expected = env("RILEY_PANEL_EXPECTED");
        m_results.setFileName(env("RILEY_PANEL_RESULTS"));
        QVERIFY2(!m_config.isEmpty() && !m_log.isEmpty() && !m_expected.isEmpty() && !env("RILEY_SETTINGS_SHARE").isEmpty(),
                 "run through tests/run-panel.sh: XDG_CONFIG_HOME, RILEY_SETTINGS_ACTION_LOG, RILEY_PANEL_EXPECTED, "
                 "RILEY_SETTINGS_SHARE must be set");
        QVERIFY2(m_results.open(QIODevice::WriteOnly | QIODevice::Append), "RILEY_PANEL_RESULTS is not writable");

        check(QStringLiteral("harness: no values file before the panel opens"), !QFile::exists(m_values), m_values);

        m_engine = new QQmlApplicationEngine(this);
        QString error;
        riley::panel::LoadOptions options;   // applyActions stays false: actions are recorded, never run
        const bool loaded = riley::panel::load(*m_engine, options, &error);
        if (loaded && !m_engine->rootObjects().isEmpty())
            m_window = qobject_cast<QQuickWindow*>(m_engine->rootObjects().first());
        if (!check(QStringLiteral("panel: load() opens a QQuickWindow from the test description"), loaded && m_window,
                   loaded ? QStringLiteral("the root object is not a QQuickWindow") : error))
            return;
        m_window->resize(1280, 1600);
        m_window->show();
        m_window->requestActivate();
        check(QStringLiteral("panel: the window is exposed and active offscreen"),
              QTest::qWaitForWindowExposed(m_window) && QTest::qWaitForWindowActive(m_window));
        QTest::qWait(100);
        check(QStringLiteral("panel: opening it writes no values file"), !QFile::exists(m_values), m_values);
        check(QStringLiteral("panel: opening it runs no action"), readAll(m_log).isEmpty(),
              QString::fromUtf8(readAll(m_log).left(160)));
    }

    void pages()
    {
        QVERIFY2(m_window, "no panel window");
        const int before = m_failed;
        const char* const ids[] = {"basic", "looks", "custom"};
        qreal lastY = -1;
        for (const char* id : ids) {
            const QString name = QStringLiteral("page:") + QString::fromLatin1(id);
            QString why;
            QQuickItem* e = visible(name, &why);
            if (!check(QStringLiteral("pages: menu entry ") + name, e, why))
                continue;
            const qreal y = e->mapToScene(QPointF(0, 0)).y();
            check(QStringLiteral("pages: ") + name + QStringLiteral(" is below the entry before it (order)"), y > lastY,
                  QStringLiteral("y %1, the entry before at %2").arg(y).arg(lastY));
            lastY = y;
        }
        QString why;
        bool shown = showPage(QStringLiteral("custom"), &why);
        QQuickItem* ph = shown ? visible(QStringLiteral("placeholder:displays"), &why) : nullptr;
        check(QStringLiteral("pages: the widget page custom shows placeholder:displays"), ph, why);
        QVERIFY(m_failed == before);
    }

    void controls()
    {
        QVERIFY2(m_window, "no panel window");
        const int before = m_failed;
        for (const Option& o : kOptions) {
            const QString page = QString::fromLatin1(o.page);
            const QString name = QStringLiteral("option:") + QString::fromLatin1(o.key);
            QString why;
            QQuickItem* item = nullptr;
            if (m_page == page || showPage(page, &why))
                item = visible(name, &why);
            if (!check(QStringLiteral("controls: ") + name + QStringLiteral(" is shown on page ") + page, item, why))
                continue;
            const QString type = item->property("controlType").toString();
            check(QStringLiteral("controls: ") + name + QStringLiteral(" has controlType ") + QString::fromLatin1(o.type),
                  type == QLatin1String(o.type), QStringLiteral("controlType \"") + type + QLatin1Char('"'));
            if (o.defaultText) {
                const QString got = item->property("valueText").toString();
                check(QStringLiteral("controls: ") + name + QStringLiteral(" shows its default \"")
                          + QString::fromUtf8(o.defaultText) + QLatin1Char('"'),
                      got == QString::fromUtf8(o.defaultText), QStringLiteral("valueText \"") + got + QLatin1Char('"'));
            } else {
                const QVariant n = item->property("itemCount");
                check(QStringLiteral("controls: ") + name + QStringLiteral(" shows 0 items"), n.isValid() && n.toInt() == 0,
                      QStringLiteral("itemCount ") + (n.isValid() ? n.toString() : QStringLiteral("missing")));
            }
        }
        for (const char* key : kWidgetKeys) {
            const QString name = QStringLiteral("option:") + QString::fromLatin1(key);
            const qsizetype n = all(name).size();
            check(QStringLiteral("controls: no control for the widget page's ") + QString::fromLatin1(key), n == 0,
                  QStringLiteral("%1 items named %2").arg(n).arg(name));
        }
        QVERIFY(m_failed == before);
    }

    void changes()
    {
        QVERIFY2(m_window, "no panel window");
        const int before = m_failed;
        const QByteArray log = readAll(m_expected + QStringLiteral("/actions.log"));
        QVERIFY2(lineCount(log) == kSteps.back().logLines, "expected/actions.log does not have 15 lines");
        int n = 0;
        for (const Step& s : kSteps) {
            ++n;
            const QString nn = QStringLiteral("%1").arg(n, 2, 10, QLatin1Char('0'));
            const QString key = QString::fromLatin1(s.key);
            const QString at = QStringLiteral("step ") + nn + QLatin1Char(' ') + key + QStringLiteral(": ");
            const QString name = QStringLiteral("option:") + key;
            QString why;
            QQuickItem* item = nullptr;
            const QString page = QString::fromLatin1(s.page);
            if (m_page == page || showPage(page, &why))
                item = visible(name, &why);
            const bool driven = item && drive(s, item, &why);
            check(at + QStringLiteral("changed the way a user would"), driven, why);

            const QByteArray wantLog = firstLines(log, s.logLines);
            (void)QTest::qWaitFor([&] { return lineCount(readAll(m_log)) >= s.logLines; }, 5000);
            QTest::qWait(50);

            sameBytes(at + QStringLiteral("values file is expected/values-") + nn + QStringLiteral(".xml"), m_values,
                      readAll(m_expected + QStringLiteral("/values-") + nn + QStringLiteral(".xml")));
            sameBytes(at + QStringLiteral("action log is the first %1 lines of expected/actions.log").arg(s.logLines),
                      m_log, wantLog);
            for (const Target& t : s.targets)
                sameBytes(at + QString::fromLatin1(t.file) + QStringLiteral(" is expected/") + QString::fromLatin1(t.expected),
                          m_config + QLatin1Char('/') + QString::fromLatin1(t.file),
                          readAll(m_expected + QLatin1Char('/') + QString::fromLatin1(t.expected)));
            if (s.valueText) {
                const QString got = item ? item->property("valueText").toString() : QStringLiteral("<no control>");
                check(at + QStringLiteral("control shows \"") + QString::fromUtf8(s.valueText) + QLatin1Char('"'),
                      got == QString::fromUtf8(s.valueText), QStringLiteral("valueText \"") + got + QLatin1Char('"'));
            } else {
                const QVariant c = item ? item->property("itemCount") : QVariant();
                check(at + QStringLiteral("control shows %1 item").arg(s.itemCount), c.isValid() && c.toInt() == s.itemCount,
                      QStringLiteral("itemCount ") + (c.isValid() ? c.toString() : QStringLiteral("missing")));
            }
        }
        QVERIFY(m_failed == before);
    }

    void cleanupTestCase()
    {
        if (m_window)
            m_window->close();
        delete m_engine;
        m_engine = nullptr;
        if (m_results.isOpen())
            report(QStringLiteral("DONE %1").arg(m_checks));
    }
};

QTEST_MAIN(TestPanel)
#include "tst_panel.moc"
