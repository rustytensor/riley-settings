#include "config.h"

#include <cstdio>

namespace riley::config {
namespace {

bool trimmed(OptionType t)
{
    return t == OptionType::Switch || t == OptionType::Choice || t == OptionType::Slider || t == OptionType::Number
        || t == OptionType::Colour;
}

double toNumber(const QString& s)
{
    double v = 0;
    return parseNumber(s.trimmed(), &v) ? v : 0.0;
}

}  // namespace

bool Item::has(const QString& field) const
{
    const Field* f = m_option ? m_option->field(field) : nullptr;
    return f && ((m_item && m_item->attrs.contains(field)) || f->defaultValue);
}

QString Item::text(const QString& field) const
{
    const Field* f = m_option ? m_option->field(field) : nullptr;
    if (!f)
        return {};
    if (m_item && m_item->attrs.contains(field)) {
        const QString v = m_item->attrs.value(field);
        return trimmed(f->type) ? v.trimmed() : v;
    }
    return f->defaultValue.value_or(QString());
}

double Item::number(const QString& field) const
{
    return toNumber(text(field));
}

bool Item::flag(const QString& field) const
{
    return text(field) == QLatin1String("true");
}

bool Config::require(const QStringList& keys, QString* error) const
{
    for (const QString& key : keys) {
        if (!m_description.option(key)) {
            *error = QStringLiteral("the description has no option ") + key;
            return false;
        }
    }
    return true;
}

QString Config::text(const QString& key) const
{
    const Option* o = m_description.option(key);
    if (!o)
        return {};
    if (const Value* v = m_values.value(key))
        return trimmed(o->type) ? v->text.trimmed() : v->text;
    return o->defaultValue.value_or(QString());
}

double Config::number(const QString& key) const
{
    return toNumber(text(key));
}

bool Config::flag(const QString& key) const
{
    return text(key) == QLatin1String("true");
}

QList<Item> Config::list(const QString& key) const
{
    QList<Item> out;
    const Option* o = m_description.option(key);
    const Value* v = m_values.value(key);
    if (!o || !v)
        return out;
    for (const ListItem& item : v->items)
        out.append(Item(o, &item));
    return out;
}

QString formatNumber(double value)
{
    char buf[64];
    std::snprintf(buf, sizeof buf, "%g", value);
    return QString::fromLatin1(buf);
}

}  // namespace riley::config
