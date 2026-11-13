// Displays: drag screens into place, apply with a 15 s revert (docs/pages-contract.md, "Displays").
// M3 scaffolding: a stub. Every function refuses with "<page title>: not ready".
#pragma once

#include "common.h"

#include <QString>
#include <QStringList>
#include <QVariantList>

namespace riley::panel::pages {

class DisplaysBackend : public PageBackend {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool pending READ pending NOTIFY changed)
    Q_PROPERTY(int keepSeconds READ keepSeconds NOTIFY changed)
    Q_PROPERTY(int secondsLeft READ secondsLeft NOTIFY changed)
    Q_PROPERTY(QVariantList screens READ screens NOTIFY changed)

public:
    explicit DisplaysBackend(QObject* parent = nullptr) : PageBackend(parent) {}

    bool dirty() const { return m_dirty; }
    bool pending() const { return m_pending; }
    int keepSeconds() const { return m_keepSeconds; }
    int secondsLeft() const { return m_secondsLeft; }
    QVariantList screens() const { return m_screens; }

    Q_INVOKABLE bool load();
    Q_INVOKABLE bool dragMove(const QString& desc, double dx, double dy);
    Q_INVOKABLE bool dragEnd(const QString& desc);
    Q_INVOKABLE bool setScreenField(const QString& desc, const QString& field, const QString& text);
    Q_INVOKABLE bool reset();
    Q_INVOKABLE bool apply();
    Q_INVOKABLE bool keep();
    Q_INVOKABLE bool revert();

Q_SIGNALS:
    void changed();

private:
    bool m_dirty = false;
    bool m_pending = false;
    int m_keepSeconds = 15;
    int m_secondsLeft = 0;
    QVariantList m_screens;
};

}  // namespace riley::panel::pages
