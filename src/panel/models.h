// The models the panel's QML draws: pages, their sections, and one OptionItem per option, all made from the
// description. Nothing here names an option; every page, section and control comes from settings.xml.
//
// A change (OptionItem::commit / commitNumber / commitItems) goes through the Controller, which in order: saves the
// values file (config::storeValue / storeList), runs the writers of the option's targets, then its actions.
#pragma once

#include "config/description.h"
#include "config/values.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

namespace riley::panel {

class Controller;

class OptionItem : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("made by the panel from the description")
    Q_PROPERTY(QString key READ key CONSTANT)
    Q_PROPERTY(QString type READ type CONSTANT)
    Q_PROPERTY(QString label READ label CONSTANT)
    Q_PROPERTY(QString help READ help CONSTANT)
    Q_PROPERTY(QString defaultText READ defaultText CONSTANT)
    Q_PROPERTY(bool hasMin READ hasMin CONSTANT)
    Q_PROPERTY(bool hasMax READ hasMax CONSTANT)
    Q_PROPERTY(double min READ min CONSTANT)
    Q_PROPERTY(double max READ max CONSTANT)
    Q_PROPERTY(double step READ step CONSTANT)
    Q_PROPERTY(QVariantList choices READ choices CONSTANT)
    Q_PROPERTY(QVariantList fields READ fields CONSTANT)
    Q_PROPERTY(QString valueText READ valueText NOTIFY valueChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY valueChanged)

public:
    // `shown` is the option the control is drawn for; `bound` the one whose value it shows and saves (the same,
    // except in the planted-fault build).
    OptionItem(Controller* controller, const config::Option* shown, const config::Option* bound, QObject* parent);

    QString key() const { return m_shown->key; }
    QString type() const;
    QString label() const { return m_shown->label; }
    QString help() const { return m_shown->help; }
    QString defaultText() const;
    bool hasMin() const { return m_shown->min.has_value(); }
    bool hasMax() const { return m_shown->max.has_value(); }
    double min() const { return m_shown->min.value_or(0); }
    double max() const { return m_shown->max.value_or(100); }
    double step() const { return m_shown->step.value_or(1); }
    QVariantList choices() const;
    QVariantList fields() const;
    QString valueText() const { return m_valueText; }
    QVariantList items() const { return m_items; }

    const config::Option* bound() const { return m_bound; }

    // A change. False (and nothing touched) when the value is refused; the reason is the controller's message.
    Q_INVOKABLE bool commit(const QString& text);
    Q_INVOKABLE bool commitNumber(double value);           // slider and number: written as the values file does
    Q_INVOKABLE bool commitItems(const QVariantList& items);   // list: [{field: text}]

    // For the dialogs: a file:// URL as a local path, and back (an empty path is an empty URL).
    Q_INVOKABLE QString localPath(const QUrl& url) const;
    Q_INVOKABLE QUrl urlOf(const QString& path) const;
    // A key press as a key binding: modifiers in the order SUPER, CTRL, ALT, SHIFT, then the key, joined by "+".
    // Empty for a key that is only a modifier.
    Q_INVOKABLE QString bindingText(int key, int modifiers) const;

    // Re-read the shown value from the controller's values.
    void refresh();

Q_SIGNALS:
    void valueChanged();

private:
    Controller* m_controller;
    const config::Option* m_shown;
    const config::Option* m_bound;
    QString m_valueText;
    QVariantList m_items;
};

class SectionItem : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("made by the panel from the description")
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString help READ help CONSTANT)
    Q_PROPERTY(QList<QObject*> options READ options CONSTANT)

public:
    SectionItem(const QString& title, const QString& help, QObject* parent)
        : QObject(parent), m_title(title), m_help(help) {}
    QString title() const { return m_title; }
    QString help() const { return m_help; }
    QList<QObject*> options() const { return m_options; }
    void add(OptionItem* o) { m_options.append(o); }

private:
    QString m_title;
    QString m_help;
    QList<QObject*> m_options;
};

class PageItem : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("made by the panel from the description")
    Q_PROPERTY(QString pageId READ pageId CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString help READ help CONSTANT)
    Q_PROPERTY(QString icon READ icon CONSTANT)
    Q_PROPERTY(QString widget READ widget CONSTANT)
    Q_PROPERTY(QList<QObject*> sections READ sections CONSTANT)

public:
    PageItem(const config::Page& page, QObject* parent) : QObject(parent), m_page(page) {}
    QString pageId() const { return m_page.id; }
    QString title() const { return m_page.title; }
    QString help() const { return m_page.help; }
    QString icon() const { return m_page.icon; }
    QString widget() const { return m_page.widget; }
    QList<QObject*> sections() const { return m_sections; }
    void add(SectionItem* s) { m_sections.append(s); }

private:
    const config::Page& m_page;
    QList<QObject*> m_sections;
};

struct Paths {
    QString description;   // settings.xml of the description
    QString schemaDir;     // its schemas
    QString values;        // your values file
    QString configHome;    // $XDG_CONFIG_HOME: where the targets are written
    QString actionLog;     // $RILEY_SETTINGS_ACTION_LOG, or empty
};

class Controller : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("made by the panel")
    Q_PROPERTY(QList<QObject*> pages READ pages CONSTANT)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)

public:
    // Null with a one-line *error when the description or the values file cannot be read.
    static Controller* create(const Paths& paths, bool applyActions, QString* error);

    QList<QObject*> pages() const { return m_pages; }
    QString message() const { return m_message; }

    // The value the values file gives `option`, or its default, as the values file writes it.
    QString valueText(const config::Option& option) const;
    // A list's items, each with every field (a missing field taking its default).
    QVariantList items(const config::Option& option) const;

    bool commit(const config::Option& option, const QString& text);
    bool commitItems(const config::Option& option, const QList<config::ItemFields>& items);

    Q_INVOKABLE void clearMessage();

Q_SIGNALS:
    void messageChanged();

private:
    explicit Controller(QObject* parent = nullptr) : QObject(parent) {}
    bool reloadValues(QString* error);
    void afterSave(const config::Option& option);
    bool runWriters(const config::Option& option);
    void runActions(const config::Option& option);
    void setMessage(const QString& text);

    Paths m_paths;
    bool m_applyActions = false;
    std::unique_ptr<config::Description> m_description;
    std::unique_ptr<config::Values> m_values;
    std::vector<config::Page> m_sortedPages;
    QList<QObject*> m_pages;
    QList<OptionItem*> m_options;
    QString m_message;
};

}  // namespace riley::panel
