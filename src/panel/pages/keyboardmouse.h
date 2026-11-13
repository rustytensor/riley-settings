// Keyboard and Mouse: the page's options are M2 controls; the page adds a box to try key repeat, which saves nothing
// (docs/pages-contract.md, "Keyboard and Mouse"). A port of Riley's keyboard_mouse.py.
#pragma once

#include "common.h"

#include <QList>
#include <QObject>
#include <QString>

namespace riley::panel::pages {

class KeyboardMouseBackend : public PageBackend {
    Q_OBJECT
    QML_ELEMENT
    // The page's scalar options (OptionItem), in file order: what the page draws as M2 controls.
    Q_PROPERTY(QList<QObject*> options READ options NOTIFY changed)

public:
    explicit KeyboardMouseBackend(QObject* parent = nullptr);

    bool ready() const override { return true; }
    QList<QObject*> options() const { return m_options; }

    // Refresh the shown value of every control from the values.
    Q_INVOKABLE bool refresh();

Q_SIGNALS:
    void changed();

private:
    void rebuild();

    QList<QObject*> m_options;
    QList<OptionItem*> m_own;   // items this backend made (the planted fault's), owned here
};

}  // namespace riley::panel::pages
