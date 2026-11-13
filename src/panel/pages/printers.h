// Printers: CUPS queues, jobs, your options and default, through the CUPS command-line tools (docs/pages-contract.md,
// "Printers"; a port of Riley's printers.py and printers_core.py). Riley asked CUPS through pycups; the page runs
// the CUPS tools that make the same requests: lpstat (queues, accepting, your default, jobs), lpoptions (your
// options, your default), lp (the test page) and cancel. Adding or removing a printer needs admin rights, so that is
// left to system-config-printer, launched like an action.
#pragma once

#include "common.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace riley::panel::pages {

class PrintersBackend : public PageBackend {
    Q_OBJECT
    QML_ELEMENT
    // One map per queue, sorted by name: name, info, location, model, state (idle, printing, stopped), reasons,
    // accepting, isDefault, colour (your print-color-mode, or ""), options (one map per option offered: name, title,
    // choices, labels, value).
    Q_PROPERTY(QVariantList printers READ printers NOTIFY printersChanged)
    // One map per job not completed: id, queue, user, size, state (waiting, held, printing, stopped).
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY jobsChanged)
    // False when CUPS did not answer (the reason is in `problem`).
    Q_PROPERTY(bool available READ available NOTIFY printersChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY printersChanged)

public:
    explicit PrintersBackend(QObject* parent = nullptr) : PageBackend(parent) {}

    bool ready() const override { return true; }
    QVariantList printers() const { return m_printers; }
    QVariantList jobs() const { return m_jobs; }
    bool available() const { return m_problem.isEmpty(); }
    QString problem() const { return m_problem; }

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool printTestPage(const QString& queue);
    Q_INVOKABLE bool cancelJob(const QString& requestId);
    Q_INVOKABLE bool setOption(const QString& queue, const QString& option, const QString& value);
    Q_INVOKABLE QString userOption(const QString& queue, const QString& option);
    Q_INVOKABLE bool setDefault(const QString& queue);
    Q_INVOKABLE bool openSetupTool();

    // The printer options Riley's page offers (printers_core.py OPTIONS, option_choices; docs/switch-contract.md,
    // "Printer options"): the options this queue has more than one choice for, in OPTIONS order; an option's choices,
    // their names, and the one shown (yours, else the printer's default, else the first choice). From the queue's
    // last reading (refresh); empty for an unknown queue or option.
    Q_INVOKABLE QStringList optionNames(const QString& queue) const;
    Q_INVOKABLE QStringList optionChoices(const QString& queue, const QString& option) const;
    Q_INVOKABLE QStringList optionLabels(const QString& queue, const QString& option) const;
    Q_INVOKABLE QString optionValue(const QString& queue, const QString& option) const;

Q_SIGNALS:
    void printersChanged();
    void jobsChanged();

private:
    struct Offered {
        QString name;
        QStringList choices;
        QString value;
    };
    bool knownQueue(const QString& queue) const;
    QString defaultQueue() const;
    QList<Offered> offered(const QString& queue) const;

    QVariantList m_printers;
    QVariantList m_jobs;
    QString m_problem;
    QHash<QString, QHash<QString, QString>> m_mine;   // queue: your options (lpoptions -p), from the last refresh
    QHash<QString, QByteArray> m_attributes;           // queue: its ipptool answer, read once per queue shown
};

// The parts without a process, for reading the CUPS tools' output (C locale).
namespace cups {

struct Queue {
    QString name;
    QString state;        // idle, printing, stopped
    QString printingJob;  // the request id it is printing, when printing
    QString info;         // Description:
    QString location;     // Location:
    QStringList reasons;  // Alerts:, without "none"
};

struct Job {
    QString id;      // request id, <queue>-<number>
    QString queue;
    QString user;
    qlonglong size = 0;
    QStringList alerts;
};

QList<Queue> parseQueues(const QByteArray& lpstatLP);          // lpstat -l -p
QHash<QString, bool> parseAccepting(const QByteArray& lpstatA); // lpstat -a
QString parseDefault(const QByteArray& lpstatD);                // lpstat -d
QList<Job> parseJobs(const QByteArray& lpstatLO);               // lpstat -l -o
QHash<QString, QString> parseOptions(const QByteArray& lpoptionsP);   // lpoptions -p QUEUE: key=value, quoted values
QString requestId(const QByteArray& lpOutput);                  // lp: "request id is Q-12 (1 file(s))"

// One attribute of `ipptool -tv ... get-printer-attributes.test`: "name (type) = value[,value...]".
struct IppAttribute {
    QString type;          // "keyword", "1setOf keyword", "enum", ...
    QStringList values;
    bool isSet() const { return type.startsWith(QLatin1String("1setOf ")); }
};
QHash<QString, IppAttribute> parseIppAttributes(const QByteArray& ipptoolTv);

}  // namespace cups

}  // namespace riley::panel::pages
