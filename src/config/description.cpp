#include "description.h"

#include "xml.h"

#include <QRegularExpression>

#include <libxml/tree.h>

#include <cmath>

namespace riley::config {
namespace {

struct TypeName {
    OptionType type;
    const char* name;
};
constexpr TypeName kTypes[] = {
    {OptionType::Switch, "switch"}, {OptionType::Choice, "choice"},  {OptionType::Slider, "slider"},
    {OptionType::Number, "number"}, {OptionType::Text, "text"},      {OptionType::Colour, "colour"},
    {OptionType::Font, "font"},     {OptionType::File, "file"},      {OptionType::Folder, "folder"},
    {OptionType::KeyBinding, "keybinding"}, {OptionType::List, "list"},
};

std::optional<double> numberAttr(const xmlNode* node, const char* name)
{
    if (!xml::hasAttr(node, name))
        return std::nullopt;
    double v = 0;
    if (!parseNumber(xml::attr(node, name).trimmed(), &v))
        return std::nullopt;   // the schema has checked xs:decimal already
    return v;
}

QString where(const QString& path, int line)
{
    return path + QLatin1Char(':') + QString::number(line) + QStringLiteral(": ");
}

// Reads the attributes and choice children common to options and fields.
void readTyped(const xmlNode* node, Typed* t)
{
    t->type = optionTypeFromName(xml::attr(node, "type")).value_or(OptionType::Text);
    t->label = xml::attr(node, "label");
    t->help = xml::attr(node, "help");
    if (xml::hasAttr(node, "default"))
        t->defaultValue = xml::attr(node, "default");
    t->min = numberAttr(node, "min");
    t->max = numberAttr(node, "max");
    t->step = numberAttr(node, "step");
    t->line = xml::line(node);
    for (const xmlNode* c = xml::firstChild(node); c; c = xml::next(c)) {
        if (xml::isElement(c, "choice"))
            t->choices.append({xml::attr(c, "value"), xml::attr(c, "label")});
    }
}

// What XSD 1.0 cannot check. `what` names the option (and field) in the message.
QString checkTyped(const Typed& t, const QString& what)
{
    const bool ranged = t.type == OptionType::Slider || t.type == OptionType::Number;
    if (t.type == OptionType::Slider && (!t.min || !t.max))
        return what + QStringLiteral(": a slider needs min and max");
    if ((t.min || t.max || t.step) && !ranged)
        return what + QStringLiteral(": min, max and step are for slider and number only");
    if (t.min && t.max && *t.min > *t.max)
        return what + QStringLiteral(": min is above max");
    if (t.step && *t.step <= 0)
        return what + QStringLiteral(": step must be above 0");
    if (t.type == OptionType::Choice && t.choices.isEmpty())
        return what + QStringLiteral(": a choice needs choice children");
    if (t.type != OptionType::Choice && !t.choices.isEmpty())
        return what + QStringLiteral(": choice children are for type choice only");
    for (qsizetype i = 0; i < t.choices.size(); ++i) {
        for (qsizetype j = 0; j < i; ++j) {
            if (t.choices[i].value == t.choices[j].value)
                return what + QStringLiteral(": choice value \"") + t.choices[i].value + QStringLiteral("\" given twice");
        }
    }
    if (t.defaultValue) {
        const QString why = checkScalar(t, *t.defaultValue);
        if (!why.isEmpty())
            return what + QStringLiteral(": default \"") + *t.defaultValue + QStringLiteral("\" ") + why;
    }
    return {};
}

}  // namespace

std::optional<OptionType> optionTypeFromName(const QString& name)
{
    for (const auto& t : kTypes) {
        if (name == QLatin1String(t.name))
            return t.type;
    }
    return std::nullopt;
}

QString optionTypeName(OptionType type)
{
    for (const auto& t : kTypes) {
        if (t.type == type)
            return QString::fromLatin1(t.name);
    }
    return {};
}

bool parseNumber(const QString& text, double* out)
{
    static const QRegularExpression re(QStringLiteral("^[+-]?(\\d+(\\.\\d*)?|\\.\\d+)$"));
    if (!re.match(text).hasMatch())
        return false;
    bool ok = false;
    const double v = text.toDouble(&ok);   // QString::toDouble always uses the C locale
    if (!ok || !std::isfinite(v))
        return false;
    *out = v;
    return true;
}

QString checkScalar(const Typed& t, const QString& text)
{
    switch (t.type) {
    case OptionType::Switch:
        if (text != QLatin1String("true") && text != QLatin1String("false"))
            return QStringLiteral("is not true or false");
        return {};
    case OptionType::Choice:
        if (!t.hasChoice(text))
            return QStringLiteral("is not one of its choices");
        return {};
    case OptionType::Slider:
    case OptionType::Number: {
        double v = 0;
        if (!parseNumber(text, &v))
            return QStringLiteral("is not a decimal number");
        if (t.min && v < *t.min)
            return QStringLiteral("is below its min ") + QString::number(*t.min);
        if (t.max && v > *t.max)
            return QStringLiteral("is above its max ") + QString::number(*t.max);
        return {};
    }
    case OptionType::Colour: {
        static const QRegularExpression re(QStringLiteral("^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$"));
        if (!re.match(text).hasMatch())
            return QStringLiteral("is not #rrggbb or #rrggbbaa");
        return {};
    }
    case OptionType::List:
        return QStringLiteral("is text, but a list holds items");
    case OptionType::Text:
    case OptionType::Font:
    case OptionType::File:
    case OptionType::Folder:
    case OptionType::KeyBinding:
        return {};
    }
    return {};
}

bool Typed::hasChoice(const QString& value) const
{
    for (const auto& c : choices) {
        if (c.value == value)
            return true;
    }
    return false;
}

bool Typed::integerChoices() const
{
    static const QRegularExpression re(QStringLiteral("^-?\\d+$"));
    if (choices.isEmpty())
        return false;
    for (const auto& c : choices) {
        if (!re.match(c.value).hasMatch())
            return false;
    }
    return true;
}

const Field* Option::field(const QString& name) const
{
    for (const auto& f : fields) {
        if (f.name == name)
            return &f;
    }
    return nullptr;
}

const Option* Description::option(const QString& key) const
{
    const auto it = m_byKey.constFind(key);
    return it == m_byKey.constEnd() ? nullptr : &m_options[size_t(*it)];
}

std::unique_ptr<Description> Description::load(const QString& path, const QString& schemaDir, QString* error)
{
    xml::Doc doc = xml::loadAndValidate(path, schemaDir + QStringLiteral("/settings.xsd"), error);
    if (!doc)
        return nullptr;

    std::unique_ptr<Description> d(new Description);
    auto readOption = [&](const xmlNode* node) -> int {
        Option o;
        readTyped(node, &o);
        o.key = xml::attr(node, "key");
        o.role = xml::attr(node, "role");
        o.targets = xml::attr(node, "target").split(QLatin1Char(' '), Qt::SkipEmptyParts);
        o.actions = xml::attr(node, "action").split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const xmlNode* c = xml::firstChild(node); c; c = xml::next(c)) {
            if (!xml::isElement(c, "field"))
                continue;
            Field f;
            readTyped(c, &f);
            f.name = xml::attr(c, "name");
            o.fields.append(f);
        }
        d->m_options.push_back(std::move(o));
        return int(d->m_options.size() - 1);
    };

    for (const xmlNode* p = xml::firstChild(xml::root(doc)); p; p = xml::next(p)) {
        if (!xml::isElement(p, "page"))
            continue;
        Page page;
        page.id = xml::attr(p, "id");
        page.title = xml::attr(p, "title");
        page.icon = xml::attr(p, "icon");
        page.help = xml::attr(p, "help");
        page.widget = xml::attr(p, "widget");
        page.order = xml::attr(p, "order").trimmed().toInt();
        for (const xmlNode* c = xml::firstChild(p); c; c = xml::next(c)) {
            if (xml::isElement(c, "option")) {
                if (page.widget.isEmpty()) {
                    *error = where(path, xml::line(c)) + QStringLiteral("page \"") + page.id
                        + QStringLiteral("\" has no widget, so its options belong in sections");
                    return nullptr;
                }
                page.options.push_back(readOption(c));
            } else if (xml::isElement(c, "section")) {
                if (!page.widget.isEmpty()) {
                    *error = where(path, xml::line(c)) + QStringLiteral("page \"") + page.id
                        + QStringLiteral("\" is drawn by its widget, so it holds options, not sections");
                    return nullptr;
                }
                Section s;
                s.title = xml::attr(c, "title");
                s.help = xml::attr(c, "help");
                for (const xmlNode* o = xml::firstChild(c); o; o = xml::next(o)) {
                    if (xml::isElement(o, "option"))
                        s.options.push_back(readOption(o));
                }
                page.sections.push_back(std::move(s));
            }
        }
        d->m_pages.push_back(std::move(page));
    }

    for (size_t i = 0; i < d->m_options.size(); ++i) {
        const Option& o = d->m_options[i];
        d->m_byKey.insert(o.key, int(i));   // unique: the schema's xs:unique has checked it
        const QString what = QStringLiteral("option ") + o.key;
        QString why;
        if (o.type == OptionType::List) {
            if (o.defaultValue)
                why = what + QStringLiteral(": a list has no default (it is empty)");
            else if (o.fields.isEmpty())
                why = what + QStringLiteral(": a list needs field children");
        } else {
            if (!o.fields.isEmpty())
                why = what + QStringLiteral(": field children are for type list only");
            else if (!o.defaultValue)
                why = what + QStringLiteral(": needs a default");
        }
        if (why.isEmpty())
            why = checkTyped(o, what);
        for (const Field& f : o.fields) {
            if (!why.isEmpty())
                break;
            why = checkTyped(f, what + QStringLiteral(" field ") + f.name);
        }
        if (o.actions.contains(QStringLiteral("none")) && o.actions.size() > 1)
            why = what + QStringLiteral(": action none cannot be combined with another action");
        if (!why.isEmpty()) {
            *error = where(path, o.line) + why;
            return nullptr;
        }
    }
    return d;
}

}  // namespace riley::config
