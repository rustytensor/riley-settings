// The bit-depth gate's Displays harness (tests/run-bitdepth.sh): opens the panel offscreen on the Displays page with
// a fake hyprctl (tests/bitdepth/inputs/monitors-all.json) and fake EDIDs ($RILEY_DRM_DIR), checks what the page
// shows and offers, presses Apply and Keep, and compares the files written with tests/bitdepth/expected/.
//
//   tst_displays b1   values file inputs/b1 (two screens at 10 bits), then the switched-off ARZOPA's scale set to
//                     1.5 (Apply writes only a change): Apply keeps the 10 bits -> expected/b1.settings.lua,
//                     expected/b1-edit.model.json
//   tst_displays b2   values file inputs/b2 (no bitdepth): the screens that run 10-bit stay 10 -> expected/b1.*
//   tst_displays b3   values file inputs/b1, then Samsung HCNYB00019 set to 10-bit on the page -> expected/b3.*
//
// What the page must show, worked out by hand from the fixtures: the bit depth from the values file, or 10 for a
// screen that hyprctl says runs XRGB2101010 when the values file has none; "10" offered where the fake EDID gives
// 10 or more bits per colour (DP-1, DP-3: dp-10; HDMI-A-2: hdmi-dc30) or where the screen runs 10-bit now (DP-2,
// whose fake EDID says 8), and only "8" for HDMI-A-1 (hdmi-8).
//
// Environment (set by run-bitdepth.sh): XDG_CONFIG_HOME (a throwaway home holding the values file),
// RILEY_SETTINGS_SHARE, RILEY_SETTINGS_ACTION_LOG, RILEY_DRM_DIR, RILEY_BITDEPTH_EXPECTED. One "PASS name" or
// "FAIL name" line per check on stdout, the detail on the next line, indented; the last line is "DONE <n>".
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
QString g_case;

void check(const QString& name, bool ok, const QString& detail = QString())
{
    ++g_checks;
    std::printf("%s %s: %s\n", ok ? "PASS" : "FAIL", g_case.toUtf8().constData(), name.toUtf8().constData());
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
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("<no file>");
}

QString firstDifference(const QByteArray& got, const QByteArray& want)
{
    const QList<QByteArray> g = got.split('\n'), w = want.split('\n');
    for (qsizetype i = 0; i < qMax(g.size(), w.size()); ++i) {
        const QByteArray a = i < g.size() ? g[i] : QByteArray("<end>"), b = i < w.size() ? w[i] : QByteArray("<end>");
        if (a != b)
            return QStringLiteral("line %1: got \"%2\", want \"%3\"").arg(i + 1).arg(QString::fromUtf8(a.left(170)),
                                                                                     QString::fromUtf8(b.left(170)));
    }
    return QStringLiteral("same lines, different bytes");
}

bool call(QQuickItem* page, const char* fn, const QVariantList& args = {})
{
    QVariant ret;
    bool ok = false;
    switch (args.size()) {
    case 0: ok = QMetaObject::invokeMethod(page, fn, Q_RETURN_ARG(QVariant, ret)); break;
    case 3:
        ok = QMetaObject::invokeMethod(page, fn, Q_RETURN_ARG(QVariant, ret), Q_ARG(QVariant, args[0]),
                                       Q_ARG(QVariant, args[1]), Q_ARG(QVariant, args[2]));
        break;
    default: break;
    }
    return ok && ret.toBool();
}

const char* const kDell = "Dell Inc. DELL U2515H YKFWP4CC09VL";
const char* const kSam1 = "Samsung Electric Company S34C65xU HCNYB00237";
const char* const kSam2 = "Samsung Electric Company S34C65xU HCNYB00019";
const char* const kArz1 = "GWD ARZOPA 0x37330206";
const char* const kArz2 = "GWD ARZOPA 0000001504150";

}  // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    g_case = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    if (g_case != QLatin1String("b1") && g_case != QLatin1String("b2") && g_case != QLatin1String("b3")) {
        std::fprintf(stderr, "usage: tst_displays b1|b2|b3\n");
        return 2;
    }
    const QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
    const QString expected = qEnvironmentVariable("RILEY_BITDEPTH_EXPECTED");
    const auto done = [] {
        std::printf("DONE %d\n", g_checks);
        return 0;
    };

    QQmlApplicationEngine engine;
    QString error;
    riley::panel::LoadOptions options;   // applyActions stays false: the reload is recorded, never run
    const bool loaded = riley::panel::load(engine, options, &error);
    QQuickWindow* w = loaded && !engine.rootObjects().isEmpty() ? qobject_cast<QQuickWindow*>(engine.rootObjects().first())
                                                                : nullptr;
    check(QStringLiteral("the panel opens with the real description"), w != nullptr, error);
    if (!w)
        return done();
    w->resize(1280, 1000);
    w->show();
    (void)QTest::qWaitForWindowExposed(w);
    if (QQuickItem* entry = waitFor(w, QStringLiteral("page:displays")))
        QTest::mouseClick(w, Qt::LeftButton, Qt::NoModifier,
                          entry->mapToScene(QPointF(entry->width() / 2, entry->height() / 2)).toPoint());
    QQuickItem* page = waitFor(w, QStringLiteral("widget:displays"));
    check(QStringLiteral("the Displays page is shown"), page != nullptr);
    if (!page)
        return done();
    QTest::qWait(50);

    const auto screens = [&] {
        QHash<QString, QVariantMap> out;
        for (const QVariant& v : page->property("screens").toList())
            out.insert(v.toMap().value(QStringLiteral("desc")).toString(), v.toMap());
        return out;
    };

    // What the page shows before any change.
    {
        const auto s = screens();
        QStringList got, want;
        for (const char* d : {kDell, kSam1, kSam2, kArz1, kArz2}) {
            got << s.value(QString::fromLatin1(d)).value(QStringLiteral("bitdepth")).toString();
        }
        want << QStringLiteral("8") << QStringLiteral("10") << QStringLiteral("8") << QStringLiteral("10")
             << QStringLiteral("8");
        check(QStringLiteral("bit depths shown: Dell 8, Samsung 237 10, Samsung 019 8, ARZOPA 0x37 10, ARZOPA 1504 8"),
              got == want, QStringLiteral("got ") + got.join(QLatin1Char(' ')));
        QStringList offered;
        for (const char* d : {kDell, kSam1, kSam2, kArz1, kArz2})
            offered << s.value(QString::fromLatin1(d)).value(QStringLiteral("depths")).toStringList().join(QLatin1Char('/'));
        check(QStringLiteral("depths offered: Dell 8 only, the four others 8/10"),
              offered == QStringList({QStringLiteral("8"), QStringLiteral("8/10"), QStringLiteral("8/10"),
                                      QStringLiteral("8/10"), QStringLiteral("8/10")}),
              QStringLiteral("got ") + offered.join(QLatin1Char(' ')));
    }

    QString name = QStringLiteral("b1");
    QString jsonName = name;
    if (g_case == QLatin1String("b1")) {
        jsonName = QStringLiteral("b1-edit");
        check(QStringLiteral("setScreenField(ARZOPA 1504, scale, 1.5) is taken"),
              call(page, "setScreenField", {QString::fromLatin1(kArz2), QStringLiteral("scale"), QStringLiteral("1.5")}));
    }
    if (g_case == QLatin1String("b3")) {
        jsonName = QStringLiteral("b3");
        name = QStringLiteral("b3");
        check(QStringLiteral("setScreenField(Samsung 019, bitdepth, 10) is taken"),
              call(page, "setScreenField", {QString::fromLatin1(kSam2), QStringLiteral("bitdepth"), QStringLiteral("10")}));
    }
    check(QStringLiteral("Apply, then Keep"), call(page, "apply") && call(page, "keep"),
          readAll(config + QStringLiteral("/../actions.log")).left(120));

    const QByteArray lua = readAll(config + QStringLiteral("/hypr/settings.lua"));
    const QByteArray wantLua = readAll(expected + QLatin1Char('/') + name + QStringLiteral(".settings.lua"));
    check(QStringLiteral("settings.lua is expected/") + name + QStringLiteral(".settings.lua"), lua == wantLua,
          firstDifference(lua, wantLua));
    const QByteArray json = readAll(config + QStringLiteral("/hypr/settings.json"));
    const QByteArray wantJson = readAll(expected + QLatin1Char('/') + jsonName + QStringLiteral(".model.json"));
    check(QStringLiteral("settings.json is expected/") + jsonName + QStringLiteral(".model.json"), json == wantJson,
          firstDifference(json, wantJson));
    const QByteArray values = readAll(config + QStringLiteral("/riley/settings.xml"));
    const int tens = int(values.count("bitdepth=\"10\"")), eights = int(values.count("bitdepth=\"8\""));
    const int wantTens = name == QLatin1String("b3") ? 3 : 2;
    check(QStringLiteral("the values file holds bitdepth=\"10\" %1 times and bitdepth=\"8\" never").arg(wantTens),
          tens == wantTens && eights == 0, QStringLiteral("10: %1, 8: %2").arg(tens).arg(eights));
    w->close();
    return done();
}
