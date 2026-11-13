// The keys gate's recorder harness (tests/run-keys.sh): opens the panel with the real description, shows the Keys
// page and records keys.overview three times with real key events, the way a user presses them. One "PASS name" or
// "FAIL name" line per check on stdout, the detail of a failure on the next line, indented; the last line is
// "DONE <n>".
//
// The sequence and its results, worked out by hand from the brief (Super pressed alone is "SUPER+SUPER_L", as
// Riley's hypr/keys.lua default, riley 2eb4c04, and Hyprland's key name Super_L):
//   1. press and release the right Super key alone           -> "SUPER+SUPER_R", and the values file holds it
//   2. hold the left Super key, press K, release both          -> "SUPER+K" (Super was not alone)
//   3. press and release the left Super key alone             -> "SUPER+SUPER_L"
//
// Environment (set by run-keys.sh): RILEY_SETTINGS_SHARE and XDG_CONFIG_HOME (a throwaway home), read by the panel.
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

// A click 20 px from the left edge, on the recorder's button (the Default button sits to its right).
void clickAt(QQuickWindow* w, QQuickItem* item, qreal x)
{
    const QPointF p = item->mapToScene(QPointF(x, item->height() / 2));
    QTest::mouseClick(w, Qt::LeftButton, Qt::NoModifier, p.toPoint());
}

}  // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    QString error;
    riley::panel::LoadOptions options;   // applyActions stays false: the reload is recorded, never run
    const bool loaded = riley::panel::load(engine, options, &error);
    QQuickWindow* w = loaded && !engine.rootObjects().isEmpty() ? qobject_cast<QQuickWindow*>(engine.rootObjects().first())
                                                                : nullptr;
    check(QStringLiteral("recorder: the panel opens with the real description"), w != nullptr, error);
    if (!w) {
        std::printf("DONE %d\n", g_checks);
        return 1;
    }
    w->resize(1280, 1000);
    w->show();
    w->requestActivate();
    (void)QTest::qWaitForWindowActive(w);

    QQuickItem* entry = waitFor(w, QStringLiteral("page:keys"));
    if (entry)
        clickAt(w, entry, entry->width() / 2);
    QQuickItem* rec = waitFor(w, QStringLiteral("option:keys.overview"));
    check(QStringLiteral("recorder: the Keys page shows keys.overview at its default SUPER+SUPER_L"),
          rec && rec->property("valueText").toString() == QStringLiteral("SUPER+SUPER_L"),
          rec ? QStringLiteral("valueText \"%1\"").arg(rec->property("valueText").toString())
              : QStringLiteral("no visible option:keys.overview"));
    if (!rec) {
        std::printf("DONE %d\n", g_checks);
        return 1;
    }

    const auto shown = [&](const char* want) {
        const QString got = rec->property("valueText").toString();
        return std::make_pair(got == QString::fromLatin1(want), QStringLiteral("valueText \"%1\"").arg(got));
    };

    // 1. the right Super key alone
    clickAt(w, rec, 20);
    QTest::keyPress(w, Qt::Key_Super_R, Qt::NoModifier);
    QTest::keyRelease(w, Qt::Key_Super_R, Qt::MetaModifier);
    auto [ok1, why1] = shown("SUPER+SUPER_R");
    check(QStringLiteral("recorder 1: right Super pressed alone records SUPER+SUPER_R"), ok1, why1);
    QFile values(qEnvironmentVariable("XDG_CONFIG_HOME") + QStringLiteral("/riley/settings.xml"));
    const QByteArray text = values.open(QIODevice::ReadOnly) ? values.readAll() : QByteArray();
    check(QStringLiteral("recorder 1: the values file holds keys.overview = SUPER+SUPER_R"),
          text.contains("<value key=\"keys.overview\">SUPER+SUPER_R</value>"),
          text.isEmpty() ? QStringLiteral("no values file") : QString::fromUtf8(text.right(200)).simplified());

    // 2. Super held, then K: Super is a modifier here, not the key
    clickAt(w, rec, 20);
    QTest::keyPress(w, Qt::Key_Super_L, Qt::NoModifier);
    QTest::keyPress(w, Qt::Key_K, Qt::MetaModifier);
    QTest::keyRelease(w, Qt::Key_K, Qt::MetaModifier);
    QTest::keyRelease(w, Qt::Key_Super_L, Qt::MetaModifier);
    auto [ok2, why2] = shown("SUPER+K");
    check(QStringLiteral("recorder 2: left Super held with K records SUPER+K"), ok2, why2);

    // 3. the left Super key alone
    clickAt(w, rec, 20);
    QTest::keyPress(w, Qt::Key_Super_L, Qt::NoModifier);
    QTest::keyRelease(w, Qt::Key_Super_L, Qt::MetaModifier);
    auto [ok3, why3] = shown("SUPER+SUPER_L");
    check(QStringLiteral("recorder 3: left Super pressed alone records SUPER+SUPER_L"), ok3, why3);

    w->close();
    std::printf("DONE %d\n", g_checks);
    return 0;
}
