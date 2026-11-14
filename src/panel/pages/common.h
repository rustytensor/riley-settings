// What the custom pages' backends share (docs/pages-contract.md): the base class every backend derives from, the
// one way to run a command, and Hyprland's list of screens. Shared by all pages: change it only by agreement.
#pragma once

#include "../models.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

namespace riley::panel::pages {

// The base of every page backend. The page's QML sets `panel` (the window's controller) and `page` (its PageItem).
class PageBackend : public QObject {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(riley::panel::Controller* panel READ panel WRITE setPanel NOTIFY panelChanged)
    Q_PROPERTY(riley::panel::PageItem* page READ page WRITE setPage NOTIFY pageChanged)
    // False while the page is the M3 scaffolding's stub.
    Q_PROPERTY(bool ready READ ready CONSTANT)

public:
    explicit PageBackend(QObject* parent = nullptr) : QObject(parent) {}

    Controller* panel() const { return m_panel; }
    void setPanel(Controller* panel);
    PageItem* page() const { return m_page; }
    void setPage(PageItem* page);
    virtual bool ready() const { return false; }

Q_SIGNALS:
    void panelChanged();
    void pageChanged();

protected:
    // A refusal: the reason in the panel's message; returns false (for `return refuse(...)`).
    bool refuse(const QString& reason) const;
    // What a stub function does: refuses with "<page title>: not ready".
    bool notReady() const;

private:
    QPointer<Controller> m_panel;
    QPointer<PageItem> m_page;
};

struct CommandResult {
    bool started = false;   // false: not found on PATH, or it did not end within the timeout
    int exitCode = -1;
    QByteArray out;
    QByteArray err;
    bool ok() const { return started && exitCode == 0; }
};

// Run argv (argv[0] found on PATH, never a shell line) with LC_ALL=C and wait for it, at most timeoutMs.
CommandResult runCommand(const QStringList& argv, int timeoutMs = 10000);

// One screen as `hyprctl monitors [all] -j` gives it.
struct Monitor {
    QString name;          // the port (DP-1): live only, never saved
    QString desc;          // "description"
    bool enabled = true;   // not "disabled"
    int width = 0;
    int height = 0;
    double refreshRate = 0;
    int x = 0;
    int y = 0;
    double scale = 1;
    int transform = 0;
    QStringList modes;     // "availableModes": live only
    QString format;        // "currentFormat" (XRGB2101010 is 10-bit): live only

    // <width>x<height>@<refreshRate %.2f>, as the values file writes a screen's mode.
    QString mode() const;
};

// Hyprland's screens, in its order: `hyprctl monitors all -j` when `all` (connected or off), else `hyprctl monitors
// -j` (in use). False with a one-line *error when hyprctl fails or its output is not the expected JSON.
bool hyprMonitors(bool all, QList<Monitor>* out, QString* error);

}  // namespace riley::panel::pages
