// The overview gate's panel harness (tests/run-overview.sh): opens the real panel with the real description and
// --apply-actions, shows the Overview page and changes its options the way docs/panel-contract.md says for their
// types. One "PASS name" or "FAIL name" line per check on stdout, the detail of a failure on the next line,
// indented; the last line is "DONE <n>".
//
// The sequence and its results, worked out by hand before the page existed (description defaults: click
// fullscreen, choices fullscreen, maximize, focus; delay slider 150, step 10):
//   0. opening the panel writes nothing, records nothing, runs nothing
//   1. page:overview shows option:overview.click at "fullscreen"
//   2. Key_Down on it      -> "maximize"; values file holds it; config.xml = expected/p1.config.xml;
//                             action log "overview-restart"; systemctl run once as "--user restart riley-overview.service"
//   3. Key_Right on overview.hot_corner_delay -> "160"; config.xml = expected/p2.config.xml; 2 restarts
//   4. type "DP-1 #2" in overview.hot_corner_screens -> refused: the panel's message is not empty, config.xml still
//                             p2, no third action, no third restart
//
// Environment (set by run-overview.sh): RILEY_SETTINGS_SHARE, XDG_CONFIG_HOME (a throwaway home),
// RILEY_SETTINGS_ACTION_LOG, PATH with a fake systemctl first, FAKE_SYSTEMCTL_LOG (where it appends its argv),
// OVERVIEW_EXPECTED (tests/overview/expected).
#include "panel/panel.h"

#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

#include <cstdio>

namespace {

int g_checks = 0;

void check(const QString& name, bool ok, const QString& detail = QString())
{
    ++g_checks;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name.toUtf8().constData());
    if (!ok && !detail.isEmpty())
        std::printf("     %s\n", detail.toUtf8().constData());
    std::fflush(stdout);
}

QQuickItem* find(QQuickItem* root, const QString& name)
{
    if (root->objectName() == name && root->isVisible())
        return root;
    for (QQuickItem* c : root->childItems())
        if (QQuickItem* f = find(c, name))
            return f;
    return nullptr;
}

QQuickItem* waitFor(QQuickWindow* w, const QString& name)
{
    QQuickItem* item = nullptr;
    (void)QTest::qWaitFor([&] { return (item = find(w->contentItem(), name)) != nullptr; }, 5000);
    return item;
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

bool focus(QQuickWindow* w, QQuickItem* item)
{
    item->forceActiveFocus();
    return QTest::qWaitFor([&] {
        QQuickItem* f = w->activeFocusItem();
        return f && (f == item || item->isAncestorOf(f));
    }, 2000);
}

QString oneLine(const QByteArray& text)
{
    return QString::fromUtf8(text).simplified().left(240);
}

}  // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    const QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
    const QString valuesPath = config + QStringLiteral("/riley/settings.xml");
    const QString overviewPath = config + QStringLiteral("/riley-overview/config.xml");
    const QString actionLog = qEnvironmentVariable("RILEY_SETTINGS_ACTION_LOG");
    const QString systemctlLog = qEnvironmentVariable("FAKE_SYSTEMCTL_LOG");
    const QString expected = qEnvironmentVariable("OVERVIEW_EXPECTED");
    const QByteArray restart = "--user restart riley-overview.service\n";

    QQmlApplicationEngine engine;
    QString error;
    riley::panel::LoadOptions options;
    options.applyActions = true;   // the restart is run; the fake systemctl on PATH records it
    const bool loaded = riley::panel::load(engine, options, &error);
    QQuickWindow* w = loaded && !engine.rootObjects().isEmpty() ? qobject_cast<QQuickWindow*>(engine.rootObjects().first())
                                                                : nullptr;
    check(QStringLiteral("panel: opens with the real description"), w != nullptr, error);
    if (!w) {
        std::printf("DONE %d\n", g_checks);
        return 1;
    }
    w->resize(1280, 1000);
    w->show();
    w->requestActivate();
    (void)QTest::qWaitForWindowActive(w);
    QObject* panel = w->property("panel").value<QObject*>();

    // 0.
    QTest::qWait(200);
    check(QStringLiteral("0: opening writes no values file and no config.xml, records and runs nothing"),
          !QFile::exists(valuesPath) && !QFile::exists(overviewPath) && readAll(actionLog).isEmpty()
              && readAll(systemctlLog).isEmpty());

    // 1.
    QQuickItem* entry = waitFor(w, QStringLiteral("page:overview"));
    if (entry)
        QTest::mouseClick(w, Qt::LeftButton, Qt::NoModifier,
                          entry->mapToScene(QPointF(entry->width() / 2, entry->height() / 2)).toPoint());
    QQuickItem* click = waitFor(w, QStringLiteral("option:overview.click"));
    check(QStringLiteral("1: page:overview shows overview.click at its default fullscreen"),
          click && click->property("valueText").toString() == QStringLiteral("fullscreen"),
          click ? QStringLiteral("valueText \"%1\"").arg(click->property("valueText").toString())
                : QString::fromLatin1(entry ? "no visible option:overview.click" : "no visible page:overview"));
    if (!click) {
        std::printf("DONE %d\n", g_checks);
        return 1;
    }

    // 2.
    if (focus(w, click))
        QTest::keyClick(w, Qt::Key_Down);
    check(QStringLiteral("2: Key_Down on overview.click gives maximize"),
          click->property("valueText").toString() == QStringLiteral("maximize"),
          QStringLiteral("valueText \"%1\"").arg(click->property("valueText").toString()));
    const QByteArray values = readAll(valuesPath);
    check(QStringLiteral("2: the values file holds overview.click = maximize"),
          values.contains("<value key=\"overview.click\">maximize</value>"), oneLine(values.right(200)));
    const QByteArray p1 = readAll(expected + QStringLiteral("/p1.config.xml"));
    check(QStringLiteral("2: config.xml = expected/p1.config.xml byte for byte"),
          !p1.isEmpty() && readAll(overviewPath) == p1, oneLine(readAll(overviewPath)));
    check(QStringLiteral("2: the action log is overview-restart"), readAll(actionLog) == "overview-restart\n",
          oneLine(readAll(actionLog)));
    const bool ran1 = QTest::qWaitFor([&] { return readAll(systemctlLog) == restart; }, 5000);
    check(QStringLiteral("2: systemctl ran once: --user restart riley-overview.service"), ran1,
          oneLine(readAll(systemctlLog)));

    // 3.
    QQuickItem* delay = waitFor(w, QStringLiteral("option:overview.hot_corner_delay"));
    if (delay && focus(w, delay))
        QTest::keyClick(w, Qt::Key_Right);
    check(QStringLiteral("3: Key_Right on overview.hot_corner_delay gives 160"),
          delay && delay->property("valueText").toString() == QStringLiteral("160"),
          delay ? QStringLiteral("valueText \"%1\"").arg(delay->property("valueText").toString())
                : QStringLiteral("no visible option:overview.hot_corner_delay"));
    const QByteArray p2 = readAll(expected + QStringLiteral("/p2.config.xml"));
    check(QStringLiteral("3: config.xml = expected/p2.config.xml byte for byte"),
          !p2.isEmpty() && readAll(overviewPath) == p2, oneLine(readAll(overviewPath)));
    const bool ran2 = QTest::qWaitFor([&] { return readAll(systemctlLog) == restart + restart; }, 5000);
    check(QStringLiteral("3: systemctl ran twice in all"), ran2, oneLine(readAll(systemctlLog)));

    // 4.
    QQuickItem* screens = waitFor(w, QStringLiteral("option:overview.hot_corner_screens"));
    if (screens && focus(w, screens)) {
        QTest::keyClick(w, Qt::Key_A, Qt::ControlModifier);
        for (const char c : QByteArray("DP-1 #2"))
            QTest::keyClick(w, c);
        QTest::keyClick(w, Qt::Key_Return);
    }
    QTest::qWait(300);
    const QString message = panel ? panel->property("message").toString() : QString();
    check(QStringLiteral("4: screens \"DP-1 #2\" is refused with a message"), screens && !message.isEmpty(),
          screens ? QStringLiteral("message empty") : QStringLiteral("no visible option:overview.hot_corner_screens"));
    check(QStringLiteral("4: config.xml is still expected/p2.config.xml"), readAll(overviewPath) == p2,
          oneLine(readAll(overviewPath)));
    QTest::qWait(700);
    check(QStringLiteral("4: no third action recorded and no third restart"),
          readAll(actionLog) == "overview-restart\noverview-restart\n" && readAll(systemctlLog) == restart + restart,
          oneLine(readAll(actionLog)) + QStringLiteral(" | ") + oneLine(readAll(systemctlLog)));

    w->close();
    std::printf("DONE %d\n", g_checks);
    return 0;
}
