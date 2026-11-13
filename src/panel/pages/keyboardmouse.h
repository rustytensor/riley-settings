// Keyboard and Mouse: the page's options are M2 controls; helpers (a layout picker, a repeat test box) end in their commit functions (docs/pages-contract.md, "Keyboard and Mouse").
// M3 scaffolding: a stub. Every function refuses with "<page title>: not ready".
#pragma once

#include "common.h"

#include <QString>
#include <QStringList>
#include <QVariantList>

namespace riley::panel::pages {

class KeyboardMouseBackend : public PageBackend {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit KeyboardMouseBackend(QObject* parent = nullptr) : PageBackend(parent) {}

    Q_INVOKABLE bool refresh();

Q_SIGNALS:
    void changed();

};

}  // namespace riley::panel::pages
