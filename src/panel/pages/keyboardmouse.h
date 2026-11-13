// Keyboard and Mouse: the page's options are M2 controls; the page adds a box to try key repeat, which saves nothing
// (docs/pages-contract.md, "Keyboard and Mouse"), and Riley's layout picker (docs/switch-contract.md, "Keyboard
// layout picker"), which sets the layout, its variants and the switch key in one step. A port of Riley's
// keyboard_mouse.py.
#pragma once

#include "common.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace riley::panel::pages {

// The system's keyboard database (/usr/share/X11/xkb/rules/evdev.lst), read the way Riley's layouts() reads it.
struct KeyboardLayouts {
    QStringList codes;                      // the "! layout" section, sorted by name ignoring case
    QStringList names;                      // the same order
    QHash<QString, QStringList> variants;   // layout code: its variants, in file order
};
KeyboardLayouts parseEvdev(const QString& text, bool exists);

class KeyboardMouseBackend : public PageBackend {
    Q_OBJECT
    QML_ELEMENT
    // The page's scalar options (OptionItem), in file order: what the page draws as M2 controls.
    Q_PROPERTY(QList<QObject*> options READ options NOTIFY changed)
    // The picker's choices: layout codes (by name), their names, and the keys that switch between two layouts.
    Q_PROPERTY(QStringList layoutCodes READ layoutCodes CONSTANT)
    Q_PROPERTY(QStringList layoutNames READ layoutNames CONSTANT)
    Q_PROPERTY(QStringList switchKeys READ switchKeys CONSTANT)
    Q_PROPERTY(QStringList switchKeyNames READ switchKeyNames CONSTANT)
    // The present choice, read from the values: first, firstVariant, second, secondVariant, switchKey.
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)

public:
    explicit KeyboardMouseBackend(QObject* parent = nullptr);

    bool ready() const override { return true; }
    QList<QObject*> options() const { return m_options; }
    QStringList layoutCodes() const { return m_layouts.codes; }
    QStringList layoutNames() const { return m_layouts.names; }
    QStringList switchKeys() const;
    QStringList switchKeyNames() const;
    QVariantMap current() const;

    // Refresh the shown value of every control from the values.
    Q_INVOKABLE bool refresh();
    // "" (no variant) and the variants of a layout, in the database's order.
    Q_INVOKABLE QStringList variantsFor(const QString& code) const;
    // Riley's on_layout: kb_layout, kb_variant and kb_options as one change. `second` "" means one layout.
    Q_INVOKABLE bool chooseLayouts(const QString& first, const QString& firstVariant, const QString& second,
                                   const QString& secondVariant, const QString& switchKey);

Q_SIGNALS:
    void changed();

private:
    void rebuild();
    OptionItem* option(const QString& role) const;

    QList<QObject*> m_options;
    QList<OptionItem*> m_own;   // items this backend made (the planted fault's), owned here
    KeyboardLayouts m_layouts;
};

}  // namespace riley::panel::pages
