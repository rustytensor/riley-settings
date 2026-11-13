// The system launcher: the menu entry of the system-launcher page launches hypr-settings and leaves the page shown as
// it was (docs/pages-contract.md, "The system launcher"; Riley's app.py). The launch is recorded in the action log as
// `launch hypr-settings` and run only with --apply-actions.
#pragma once

#include "common.h"

namespace riley::panel::pages {

class SystemLauncher : public PageBackend {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit SystemLauncher(QObject* parent = nullptr) : PageBackend(parent) {}

    bool ready() const override { return true; }

    // Records `launch hypr-settings` (run only with --apply-actions). False when the panel is not set.
    Q_INVOKABLE bool launch();

Q_SIGNALS:
    void changed();
};

}  // namespace riley::panel::pages
