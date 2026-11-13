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
    // accepting, isDefault, colour (your print-color-mode, or "").
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

Q_SIGNALS:
    void printersChanged();
    void jobsChanged();

private:
    bool knownQueue(const QString& queue) const;
    QString defaultQueue() const;

    QVariantList m_printers;
    QVariantList m_jobs;
    QString m_problem;
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

}  // namespace cups

}  // namespace riley::panel::pages
