// Printers: CUPS queues, jobs, your options and default, through the CUPS command-line tools (docs/pages-contract.md, "Printers").
// M3 scaffolding: a stub. Every function refuses with "<page title>: not ready".
#pragma once

#include "common.h"

#include <QString>
#include <QStringList>
#include <QVariantList>

namespace riley::panel::pages {

class PrintersBackend : public PageBackend {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList printers READ printers NOTIFY changed)
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY changed)

public:
    explicit PrintersBackend(QObject* parent = nullptr) : PageBackend(parent) {}

    QVariantList printers() const { return m_printers; }
    QVariantList jobs() const { return m_jobs; }

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool printTestPage(const QString& queue);
    Q_INVOKABLE bool cancelJob(const QString& requestId);
    Q_INVOKABLE bool setOption(const QString& queue, const QString& option, const QString& value);
    Q_INVOKABLE QString userOption(const QString& queue, const QString& option);
    Q_INVOKABLE bool setDefault(const QString& queue);
    Q_INVOKABLE bool openSetupTool();

Q_SIGNALS:
    void changed();

private:
    QVariantList m_printers;
    QVariantList m_jobs;
};

}  // namespace riley::panel::pages
