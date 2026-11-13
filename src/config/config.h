// What a writer reads: each option's value from your values file, or else its default from the description.
// Writers name the keys they need; they never hold a default of their own.
#pragma once

#include "description.h"
#include "values.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace riley::config {

// One item of a list value, with each field the item leaves out taken from the field's default.
class Item {
public:
    Item(const Option* option, const ListItem* item) : m_option(option), m_item(item) {}

    bool has(const QString& field) const;   // written in the item, or has a default
    QString text(const QString& field) const;
    double number(const QString& field) const;
    bool flag(const QString& field) const;
    int line() const { return m_item ? m_item->line : 0; }

private:
    const Option* m_option;
    const ListItem* m_item;
};

class Config {
public:
    Config(const Description& description, const Values& values) : m_description(description), m_values(values) {}

    const Description& description() const { return m_description; }
    const Values& values() const { return m_values; }

    // False with *error naming the first key the description does not hold (a writer and settings.xml disagree).
    bool require(const QStringList& keys, QString* error) const;

    const Option* option(const QString& key) const { return m_description.option(key); }
    bool isSet(const QString& key) const { return m_values.value(key) != nullptr; }

    QString text(const QString& key) const;      // the value's text, or the default
    double number(const QString& key) const;     // slider, number, and choices whose values are numbers
    bool flag(const QString& key) const;         // switch
    QList<Item> list(const QString& key) const;  // list: the items, or empty

private:
    const Description& m_description;
    const Values& m_values;
};

// A number as C's %g writes it (1.0 -> 1, -0.2 -> -0.2), as Riley's Python writers do with f"{v:g}".
QString formatNumber(double value);

}  // namespace riley::config
