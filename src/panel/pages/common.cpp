#include "common.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>

namespace riley::panel::pages {

void PageBackend::setPanel(Controller* panel)
{
    if (m_panel == panel)
        return;
    m_panel = panel;
    Q_EMIT panelChanged();
}

void PageBackend::setPage(PageItem* page)
{
    if (m_page == page)
        return;
    m_page = page;
    Q_EMIT pageChanged();
}

bool PageBackend::refuse(const QString& reason) const
{
    if (m_panel)
        m_panel->setMessage(reason);
    return false;
}

bool PageBackend::notReady() const
{
    return refuse((m_page ? m_page->title() : QStringLiteral("This page")) + QStringLiteral(": not ready"));
}

CommandResult runCommand(const QStringList& argv, int timeoutMs)
{
    CommandResult r;
    if (argv.isEmpty())
        return r;
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    p.setProcessEnvironment(env);
    p.setProgram(argv.first());
    p.setArguments(argv.mid(1));
    p.start(QIODevice::ReadOnly);
    if (!p.waitForStarted(timeoutMs))
        return r;
    p.closeWriteChannel();
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        return r;
    }
    r.started = p.exitStatus() == QProcess::NormalExit;
    r.exitCode = p.exitCode();
    r.out = p.readAllStandardOutput();
    r.err = p.readAllStandardError();
    return r;
}

QString Monitor::mode() const
{
    return QStringLiteral("%1x%2@%3").arg(width).arg(height).arg(refreshRate, 0, 'f', 2);
}

bool hyprMonitors(bool all, QList<Monitor>* out, QString* error)
{
    const QStringList argv = all ? QStringList{QStringLiteral("hyprctl"), QStringLiteral("monitors"),
                                               QStringLiteral("all"), QStringLiteral("-j")}
                                 : QStringList{QStringLiteral("hyprctl"), QStringLiteral("monitors"),
                                               QStringLiteral("-j")};
    const CommandResult r = runCommand(argv);
    if (!r.ok()) {
        *error = QStringLiteral("hyprctl monitors: ")
            + (r.started ? QString::fromUtf8(r.err.trimmed().left(200)) : QStringLiteral("did not run"));
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(r.out, &pe);
    if (!doc.isArray()) {
        *error = QStringLiteral("hyprctl monitors: not a JSON list (") + pe.errorString() + QLatin1Char(')');
        return false;
    }
    out->clear();
    for (const QJsonValue& v : doc.array()) {
        const QJsonObject o = v.toObject();
        Monitor m;
        m.name = o.value(QStringLiteral("name")).toString();
        m.desc = o.value(QStringLiteral("description")).toString();
        m.enabled = !o.value(QStringLiteral("disabled")).toBool(false);
        m.width = o.value(QStringLiteral("width")).toInt();
        m.height = o.value(QStringLiteral("height")).toInt();
        m.refreshRate = o.value(QStringLiteral("refreshRate")).toDouble();
        m.x = o.value(QStringLiteral("x")).toInt();
        m.y = o.value(QStringLiteral("y")).toInt();
        m.scale = o.value(QStringLiteral("scale")).toDouble(1);
        m.transform = o.value(QStringLiteral("transform")).toInt();
        m.format = o.value(QStringLiteral("currentFormat")).toString();
        for (const QJsonValue& mode : o.value(QStringLiteral("availableModes")).toArray())
            m.modes.append(mode.toString());
        out->append(m);
    }
    return true;
}

}  // namespace riley::panel::pages
