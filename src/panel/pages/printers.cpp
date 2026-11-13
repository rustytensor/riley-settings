#include "printers.h"

#include <QRegularExpression>

#include <algorithm>

namespace riley::panel::pages {

namespace {

const QString TEST_PAGE = QStringLiteral("/usr/share/cups/data/testprint");
const QString TEST_TITLE = QStringLiteral("Test page from Riley Settings");

QString line1(const CommandResult& r)
{
    if (!r.started)
        return QStringLiteral("did not run");
    QString e = QString::fromUtf8(r.err.trimmed());
    if (e.isEmpty())
        e = QString::fromUtf8(r.out.trimmed());
    e = e.section(QLatin1Char('\n'), 0, 0).left(200);
    return e.isEmpty() ? QStringLiteral("exit code %1").arg(r.exitCode) : e;
}

QStringList lines(const QByteArray& text)
{
    return QString::fromUtf8(text).split(QLatin1Char('\n'));
}

}  // namespace

namespace cups {

QList<Queue> parseQueues(const QByteArray& text)
{
    static const QRegularExpression head(QStringLiteral("^printer (\\S+) (.*)$"));
    static const QRegularExpression nowPrinting(QStringLiteral("now printing (\\S+?)\\.(\\s|$)"));
    QList<Queue> out;
    for (const QString& l : lines(text)) {
        const auto m = head.match(l);
        if (m.hasMatch()) {
            Queue q;
            q.name = m.captured(1);
            const QString rest = m.captured(2);
            if (rest.startsWith(QLatin1String("disabled")))
                q.state = QStringLiteral("stopped");
            else if (const auto p = nowPrinting.match(rest); p.hasMatch()) {
                q.state = QStringLiteral("printing");
                q.printingJob = p.captured(1);
            } else
                q.state = QStringLiteral("idle");
            out.append(q);
            continue;
        }
        if (out.isEmpty() || !l.startsWith(QLatin1Char('\t')))
            continue;
        const QString t = l.trimmed();
        Queue& q = out.last();
        if (t.startsWith(QLatin1String("Description:")))
            q.info = t.mid(12).trimmed();
        else if (t.startsWith(QLatin1String("Location:")))
            q.location = t.mid(9).trimmed();
        else if (t.startsWith(QLatin1String("Alerts:"))) {
            for (const QString& a : t.mid(7).split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts)) {
                if (a != QLatin1String("none"))
                    q.reasons.append(a);
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const Queue& a, const Queue& b) { return a.name < b.name; });
    return out;
}

QHash<QString, bool> parseAccepting(const QByteArray& text)
{
    static const QRegularExpression re(QStringLiteral("^(\\S+) (not )?accepting requests"));
    QHash<QString, bool> out;
    for (const QString& l : lines(text)) {
        const auto m = re.match(l);
        if (m.hasMatch())
            out.insert(m.captured(1), m.captured(2).isEmpty());
    }
    return out;
}

QString parseDefault(const QByteArray& text)
{
    static const QRegularExpression re(QStringLiteral("^system default destination: (\\S+)"));
    for (const QString& l : lines(text)) {
        const auto m = re.match(l);
        if (m.hasMatch())
            return m.captured(1);
    }
    return {};
}

QList<Job> parseJobs(const QByteArray& text)
{
    static const QRegularExpression head(QStringLiteral("^(\\S+)-(\\d+)\\s+(\\S+)\\s+(\\d+)\\s"));
    QList<Job> out;
    for (const QString& l : lines(text)) {
        const auto m = head.match(l);
        if (m.hasMatch()) {
            Job j;
            j.queue = m.captured(1);
            j.id = m.captured(1) + QLatin1Char('-') + m.captured(2);
            j.user = m.captured(3);
            j.size = m.captured(4).toLongLong();
            out.append(j);
            continue;
        }
        if (out.isEmpty() || !l.startsWith(QLatin1Char('\t')))
            continue;
        const QString t = l.trimmed();
        if (t.startsWith(QLatin1String("Alerts:")))
            out.last().alerts += t.mid(7).split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
    }
    return out;
}

// lpoptions prints key=value pairs separated by spaces; a value with spaces is quoted ('Office laser') or escaped.
QHash<QString, QString> parseOptions(const QByteArray& bytes)
{
    QHash<QString, QString> out;
    const QString text = QString::fromUtf8(bytes);
    qsizetype i = 0;
    const qsizetype n = text.size();
    while (i < n) {
        while (i < n && text[i].isSpace())
            ++i;
        if (i >= n)
            break;
        QString token;
        QChar quote;
        for (; i < n; ++i) {
            const QChar c = text[i];
            if (!quote.isNull()) {
                if (c == quote)
                    quote = QChar();
                else if (c == QLatin1Char('\\') && i + 1 < n && quote == QLatin1Char('"'))
                    token += text[++i];
                else
                    token += c;
            } else if (c == QLatin1Char('\'') || c == QLatin1Char('"')) {
                quote = c;
            } else if (c == QLatin1Char('\\') && i + 1 < n) {
                token += text[++i];
            } else if (c.isSpace()) {
                break;
            } else {
                token += c;
            }
        }
        const qsizetype eq = token.indexOf(QLatin1Char('='));
        if (eq > 0)
            out.insert(token.left(eq), token.mid(eq + 1));
        else if (!token.isEmpty())
            out.insert(token, QString());
    }
    return out;
}

QString requestId(const QByteArray& text)
{
    static const QRegularExpression re(QStringLiteral("request id is (\\S+)"));
    const auto m = re.match(QString::fromUtf8(text));
    return m.hasMatch() ? m.captured(1) : QString();
}

QHash<QString, IppAttribute> parseIppAttributes(const QByteArray& text)
{
    static const QRegularExpression re(QStringLiteral("^\\s+([A-Za-z0-9._-]+) \\(([^)]*)\\) = (.*)$"));
    QHash<QString, IppAttribute> out;
    for (const QString& l : lines(text)) {
        const auto m = re.match(l);
        if (!m.hasMatch())
            continue;
        IppAttribute a;
        a.type = m.captured(2);
        a.values = m.captured(3).split(QLatin1Char(','));
        out.insert(m.captured(1), a);
    }
    return out;
}

}  // namespace cups

namespace {

// Riley's printers_core.py OPTIONS, in its order, with their titles.
struct PrinterOption {
    const char* name;
    const char* title;
};
constexpr PrinterOption OPTIONS[] = {
    {"media", "Paper size"},
    {"print-color-mode", "Colour"},
    {"print-quality", "Quality"},
    {"sides", "Two-sided"},
};

// Riley's NICE: the names of the values the page shows; any other value is shown as it is.
QString nice(const QString& v)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("monochrome"), QStringLiteral("Black and white")},
        {QStringLiteral("color"), QStringLiteral("Colour")},
        {QStringLiteral("one-sided"), QStringLiteral("One-sided")},
        {QStringLiteral("two-sided-long-edge"), QStringLiteral("Two-sided, long edge")},
        {QStringLiteral("two-sided-short-edge"), QStringLiteral("Two-sided, short edge")},
        {QStringLiteral("3"), QStringLiteral("Draft")},
        {QStringLiteral("4"), QStringLiteral("Normal")},
        {QStringLiteral("5"), QStringLiteral("Best")},
        {QStringLiteral("na_letter_8.5x11in"), QStringLiteral("Letter")},
        {QStringLiteral("na_legal_8.5x14in"), QStringLiteral("Legal")},
        {QStringLiteral("iso_a4_210x297mm"), QStringLiteral("A4")},
        {QStringLiteral("iso_a5_148x210mm"), QStringLiteral("A5")},
        {QStringLiteral("iso_a6_105x148mm"), QStringLiteral("A6")},
        {QStringLiteral("na_executive_7.25x10.5in"), QStringLiteral("Executive")},
        {QStringLiteral("na_number-10_4.125x9.5in"), QStringLiteral("Envelope #10")},
        {QStringLiteral("iso_dl_110x220mm"), QStringLiteral("Envelope DL")},
        {QStringLiteral("iso_c5_162x229mm"), QStringLiteral("Envelope C5")},
        {QStringLiteral("na_monarch_3.875x7.5in"), QStringLiteral("Envelope Monarch")},
        {QStringLiteral("jis_b5_182x257mm"), QStringLiteral("B5 (JIS)")},
        {QStringLiteral("iso_b5_176x250mm"), QStringLiteral("B5")},
        {QStringLiteral("na_index-3x5_3x5in"), QString::fromUtf8("Index card 3×5")},
        {QStringLiteral("na_foolscap_8.5x13in"), QStringLiteral("Foolscap")},
        {QStringLiteral("om_folio_210x330mm"), QStringLiteral("Folio")},
    };
    return names.value(v, v);
}

// print-quality is an IPP enum: ipptool names it, pycups (and lpoptions) give its number.
QString ippValue(const QString& option, const QString& v)
{
    if (option != QLatin1String("print-quality"))
        return v;
    static const QHash<QString, QString> numbers = {
        {QStringLiteral("draft"), QStringLiteral("3")},
        {QStringLiteral("normal"), QStringLiteral("4")},
        {QStringLiteral("high"), QStringLiteral("5")},
    };
    return numbers.value(v, v);
}

}  // namespace

QList<PrintersBackend::Offered> PrintersBackend::offered(const QString& queue) const
{
    QList<Offered> out;
    const auto attributes = m_attributes.constFind(queue);
    if (attributes == m_attributes.cend())
        return out;
    const QHash<QString, cups::IppAttribute> a = cups::parseIppAttributes(*attributes);
    const QHash<QString, QString> mine = m_mine.value(queue);
    for (const PrinterOption& o : OPTIONS) {
        const QString name = QString::fromLatin1(o.name);
        // Riley: isinstance(sup, list) and len(sup) > 1, then the values without custom_ ones.
        const cups::IppAttribute sup = a.value(name + QStringLiteral("-supported"));
        if (!sup.isSet() || sup.values.size() <= 1)
            continue;
        Offered off;
        off.name = name;
        for (const QString& v : sup.values) {
            if (!v.startsWith(QLatin1String("custom_")))
                off.choices.append(ippValue(name, v));
        }
        if (off.choices.isEmpty())
            continue;
        // Riley: cur = mine.get(opt, default) (default: the printer's, else the first choice); index 0 when cur is
        // not a choice.
        const auto def = a.constFind(name + QStringLiteral("-default"));
        const QString printerDefault = def != a.cend() ? ippValue(name, def->values.join(QLatin1Char(','))) : off.choices.first();
        const QString cur = mine.contains(name) ? mine.value(name) : printerDefault;
        off.value = off.choices.contains(cur) ? cur : off.choices.first();
        out.append(off);
    }
#ifdef RILEY_PLANT_FAULT_SWITCH
    // The M4 gate's planted fault: the last option offered is left out.
    if (!out.isEmpty())
        out.removeLast();
#endif
    return out;
}

QStringList PrintersBackend::optionNames(const QString& queue) const
{
    QStringList out;
    for (const Offered& o : offered(queue))
        out.append(o.name);
    return out;
}

QStringList PrintersBackend::optionChoices(const QString& queue, const QString& option) const
{
    for (const Offered& o : offered(queue)) {
        if (o.name == option)
            return o.choices;
    }
    return {};
}

QStringList PrintersBackend::optionLabels(const QString& queue, const QString& option) const
{
    QStringList out;
    for (const QString& c : optionChoices(queue, option))
        out.append(nice(c));
    return out;
}

QString PrintersBackend::optionValue(const QString& queue, const QString& option) const
{
    for (const Offered& o : offered(queue)) {
        if (o.name == option)
            return o.value;
    }
    return {};
}

bool PrintersBackend::knownQueue(const QString& queue) const
{
    for (const QVariant& p : m_printers) {
        if (p.toMap().value(QStringLiteral("name")).toString() == queue)
            return true;
    }
    return false;
}

QString PrintersBackend::defaultQueue() const
{
    for (const QVariant& p : m_printers) {
        const QVariantMap m = p.toMap();
        if (m.value(QStringLiteral("isDefault")).toBool())
            return m.value(QStringLiteral("name")).toString();
    }
    return {};
}

bool PrintersBackend::refresh()
{
    QVariantList printers, jobs;
    QString problem;
    const CommandResult lp = runCommand({QStringLiteral("lpstat"), QStringLiteral("-l"), QStringLiteral("-p")});
    if (!lp.started)
        problem = QStringLiteral("The printing service did not answer: lpstat ") + line1(lp);
    else if (!lp.ok() && !lp.err.contains("No destinations"))
        problem = QStringLiteral("The printing service did not answer: ") + line1(lp);

    QList<cups::Queue> queues;
    if (problem.isEmpty() && lp.ok())
        queues = cups::parseQueues(lp.out);
    if (!queues.isEmpty()) {
        const CommandResult a = runCommand({QStringLiteral("lpstat"), QStringLiteral("-a")});
        const QHash<QString, bool> accepting = cups::parseAccepting(a.out);
        const CommandResult d = runCommand({QStringLiteral("lpstat"), QStringLiteral("-d")});
        const QString def = cups::parseDefault(d.out);
        const CommandResult o = runCommand({QStringLiteral("lpstat"), QStringLiteral("-l"), QStringLiteral("-o")});
        const QList<cups::Job> jobList = o.ok() ? cups::parseJobs(o.out) : QList<cups::Job>();

        for (const cups::Queue& q : std::as_const(queues)) {
            const CommandResult opts = runCommand({QStringLiteral("lpoptions"), QStringLiteral("-p"), q.name});
            const QHash<QString, QString> mine = opts.ok() ? cups::parseOptions(opts.out) : QHash<QString, QString>();
            m_mine.insert(q.name, mine);
            // What the printer supports: asked once per queue (Riley: getPrinterAttributes when the page is built);
            // a queue that does not answer offers no options.
            if (!m_attributes.contains(q.name) && !q.name.startsWith(QLatin1Char('-'))) {
                const CommandResult ipp = runCommand({QStringLiteral("ipptool"), QStringLiteral("-tv"),
                                                      QStringLiteral("ipp://localhost/printers/") + q.name,
                                                      QStringLiteral("get-printer-attributes.test")});
                m_attributes.insert(q.name, ipp.ok() ? ipp.out : QByteArray());
            }
            QVariantList offeredOptions;
            for (const Offered& o : offered(q.name)) {
                QStringList labels;
                for (const QString& c : o.choices)
                    labels.append(nice(c));
                QVariantMap om;
                om.insert(QStringLiteral("name"), o.name);
                for (const PrinterOption& po : OPTIONS) {
                    if (o.name == QLatin1String(po.name))
                        om.insert(QStringLiteral("title"), QString::fromLatin1(po.title));
                }
                om.insert(QStringLiteral("choices"), o.choices);
                om.insert(QStringLiteral("labels"), labels);
                om.insert(QStringLiteral("value"), o.value);
                offeredOptions.append(om);
            }
            QVariantMap m;
            m.insert(QStringLiteral("name"), q.name);
            m.insert(QStringLiteral("info"), q.info.isEmpty() ? q.name : q.info);
            m.insert(QStringLiteral("location"), q.location);
            m.insert(QStringLiteral("model"), mine.value(QStringLiteral("printer-make-and-model")));
            m.insert(QStringLiteral("state"), q.state);
            m.insert(QStringLiteral("reasons"), q.reasons);
            m.insert(QStringLiteral("accepting"), accepting.value(q.name, false));
            m.insert(QStringLiteral("isDefault"), q.name == def);
            m.insert(QStringLiteral("colour"), mine.value(QStringLiteral("print-color-mode")));
            m.insert(QStringLiteral("options"), offeredOptions);
            printers.append(m);
        }
        for (const cups::Job& j : jobList) {
            QString state = QStringLiteral("waiting");
            const auto queue = std::find_if(queues.cbegin(), queues.cend(), [&](const cups::Queue& q) { return q.name == j.queue; });
            if (j.alerts.contains(QStringLiteral("job-hold-until-specified")))
                state = QStringLiteral("held");
            else if (queue != queues.cend() && queue->printingJob == j.id)
                state = QStringLiteral("printing");
            else if (j.alerts.contains(QStringLiteral("job-printing-stopped"))
                     || (queue != queues.cend() && queue->state == QLatin1String("stopped")))
                state = QStringLiteral("stopped");
            QVariantMap m;
            m.insert(QStringLiteral("id"), j.id);
            m.insert(QStringLiteral("queue"), j.queue);
            m.insert(QStringLiteral("user"), j.user);
            m.insert(QStringLiteral("size"), j.size);
            m.insert(QStringLiteral("state"), state);
            jobs.append(m);
        }
    }

    if (printers != m_printers || problem != m_problem) {
        m_printers = printers;
        m_problem = problem;
        Q_EMIT printersChanged();
    }
    if (jobs != m_jobs) {
        m_jobs = jobs;
        Q_EMIT jobsChanged();
    }
    return problem.isEmpty() || refuse(problem);
}

bool PrintersBackend::printTestPage(const QString& queueIn)
{
    if (!knownQueue(queueIn))
        refresh();
    if (!knownQueue(queueIn))
        return refuse(QStringLiteral("Printers: there is no printer %1").arg(queueIn));
    QString queue = queueIn;
#ifdef RILEY_PLANT_FAULT_PAGES
    // The M3 gate's planted fault: the test page goes to your default printer.
    if (const QString def = defaultQueue(); !def.isEmpty())
        queue = def;
#endif
    const CommandResult r = runCommand({QStringLiteral("lp"), QStringLiteral("-d"), queue, QStringLiteral("-t"), TEST_TITLE, TEST_PAGE});
    if (!r.ok()) {
        refresh();
        return refuse(QStringLiteral("The test page was refused: ") + line1(r));
    }
    const QString id = cups::requestId(r.out);
    refresh();
    if (panel())
        panel()->setMessage(id.isEmpty() ? QStringLiteral("Test page sent") : QStringLiteral("Test page sent (job %1)").arg(id));
    return true;
}

bool PrintersBackend::cancelJob(const QString& requestId)
{
    if (requestId.isEmpty() || requestId.startsWith(QLatin1Char('-')))
        return refuse(QStringLiteral("Printers: no such job"));
    const CommandResult r = runCommand({QStringLiteral("cancel"), requestId});
    refresh();
    if (!r.ok())
        return refuse(QStringLiteral("Could not cancel: ") + line1(r));
    if (panel())
        panel()->setMessage(QStringLiteral("Job cancelled"));
    return true;
}

bool PrintersBackend::setOption(const QString& queue, const QString& option, const QString& value)
{
    static const QRegularExpression name(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    if (!knownQueue(queue))
        refresh();
    if (!knownQueue(queue))
        return refuse(QStringLiteral("Printers: there is no printer %1").arg(queue));
    if (!name.match(option).hasMatch() || value.isEmpty() || value.contains(QLatin1Char(' ')))
        return refuse(QStringLiteral("Printers: %1=%2 is not an option lpoptions takes").arg(option, value));
    const CommandResult r = runCommand({QStringLiteral("lpoptions"), QStringLiteral("-p"), queue, QStringLiteral("-o"),
                                        option + QLatin1Char('=') + value});
    refresh();
    if (!r.ok())
        return refuse(QStringLiteral("Could not save that option: ") + line1(r));
    return true;
}

QString PrintersBackend::userOption(const QString& queue, const QString& option)
{
    if (queue.isEmpty() || queue.startsWith(QLatin1Char('-')))
        return {};
    const CommandResult r = runCommand({QStringLiteral("lpoptions"), QStringLiteral("-p"), queue});
    if (!r.ok())
        return {};
    return cups::parseOptions(r.out).value(option);
}

bool PrintersBackend::setDefault(const QString& queue)
{
    if (!knownQueue(queue))
        refresh();
    if (!knownQueue(queue))
        return refuse(QStringLiteral("Printers: there is no printer %1").arg(queue));
    const CommandResult r = runCommand({QStringLiteral("lpoptions"), QStringLiteral("-d"), queue});
    refresh();
    if (!r.ok())
        return refuse(QStringLiteral("Could not change the default: ") + line1(r));
    if (panel())
        panel()->setMessage(QStringLiteral("%1 is now your default printer").arg(queue));
    return true;
}

bool PrintersBackend::openSetupTool()
{
    if (!panel())
        return false;
    panel()->launch(QStringLiteral("system-config-printer"));
    return true;
}

}  // namespace riley::panel::pages
