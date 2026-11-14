// The description: every page, section and option, read from share/riley-settings/settings.xml.
// Loading checks the file against schema/settings.xsd, then what XSD 1.0 cannot express (a default within its range,
// a choice default among its choices, a page drawn from sections versus one drawn by its own widget).
#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>
#include <vector>

namespace riley::config {

enum class OptionType { Switch, Choice, Slider, Number, Text, Colour, Font, File, Folder, KeyBinding, List };

std::optional<OptionType> optionTypeFromName(const QString& name);
QString optionTypeName(OptionType type);

struct Choice {
    QString value;
    QString label;
};

// What an option and a list field have in common: a type, a default and its limits.
struct Typed {
    OptionType type = OptionType::Text;
    QString label;
    QString help;
    std::optional<QString> defaultValue;
    std::optional<double> min;
    std::optional<double> max;
    std::optional<double> step;
    QList<Choice> choices;
    int line = 0;

    bool hasChoice(const QString& value) const;
    // Whether every choice value is an integer (such a choice is a number to the writers, not a string).
    bool integerChoices() const;
};

struct Field : Typed {
    QString name;
    bool optional = false;   // optional="true": left out of an item while it equals its default
};

struct Option : Typed {
    QString key;
    QString role;          // what a widget page uses the option for (role="pins"); empty for most options
    QList<Field> fields;   // list only
    QStringList targets;
    QStringList actions;

    const Field* field(const QString& name) const;
};

struct Section {
    QString title;
    QString help;
    std::vector<int> options;   // indexes into Description::options()
};

struct Page {
    QString id;
    QString title;
    QString icon;
    QString help;
    QString widget;   // empty: drawn from its sections
    int order = 0;
    std::vector<Section> sections;
    std::vector<int> options;   // widget pages: the options the widget stores
};

class Description {
public:
    // Load and check `path` against `<schemaDir>/settings.xsd`. Null with a one-line *error when it is not valid.
    static std::unique_ptr<Description> load(const QString& path, const QString& schemaDir, QString* error);

    const std::vector<Page>& pages() const { return m_pages; }
    const std::vector<Option>& options() const { return m_options; }
    const Option* option(const QString& key) const;

private:
    Description() = default;
    std::vector<Page> m_pages;
    std::vector<Option> m_options;
    QHash<QString, int> m_byKey;
};

// The checks shared by the loader (for a default) and the values (for a value): is `text` a valid value of `typed`?
// Returns an empty string when it is, else the reason.
QString checkScalar(const Typed& typed, const QString& text);

// Decimal number as the values file writes it: 1, 1.0, -0.2, .5
bool parseNumber(const QString& text, double* out);

}  // namespace riley::config
