// Your values (~/.config/riley/settings.xml): only what differs from the defaults, by key.
// Loading checks the file against schema/values.xsd. Changing a value edits the text of that one value in place, so
// every other byte (comments, layout, quoting) stays as it was; the file is never re-serialised from a tree.
#pragma once

#include "description.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace riley::config {

struct ListItem {
    QHash<QString, QString> attrs;   // field name -> text, as written
    QStringList order;               // attribute names in file order
    int line = 0;
};

struct Value {
    QString key;
    QString text;              // the value's text (whitespace kept)
    QList<ListItem> items;     // for a list
    int line = 0;
};

class Values {
public:
    // Load and check `path` against `<schemaDir>/values.xsd`. Null with a one-line *error when it is not valid.
    static std::unique_ptr<Values> load(const QString& path, const QString& schemaDir, QString* error);
    // The same from bytes already read (`name` is used in messages).
    static std::unique_ptr<Values> parse(const QByteArray& data, const QString& name, const QString& schemaDir,
                                         QString* error);

    const std::vector<Value>& values() const { return m_values; }
    const Value* value(const QString& key) const;
    const QString& path() const { return m_path; }

    // Check every value against the description. Keys it does not know go to *unknown (kept, not an error).
    // Returns false with a one-line *error on the first value that does not fit its option.
    bool check(const Description& description, QStringList* unknown, QString* error) const;

private:
    Values() = default;
    QString m_path;
    std::vector<Value> m_values;
    QHash<QString, int> m_byKey;
};

// Change the text of one scalar value in the values file `path`, in place and atomically. The key must be known to
// `description` and `text` must be a valid value for it. A key not yet in the file gets a new line before
// </settings>. Every other byte of the file is kept.
bool setValue(const QString& path, const Description& description, const QString& schemaDir, const QString& key,
              const QString& text, QString* error);

// The edit itself, on bytes (for setValue and its checks): returns the new file text, or an empty array with *error.
QByteArray editValueText(const QByteArray& data, const QString& key, const QString& text, QString* error);

// Escape text for XML character data.
QByteArray escapeText(const QString& text);

// --- The panel's save path: the set path above, plus a new file, a removed key and a list. ---

// What a values file holds before any value: the XML declaration, one comment, an empty <settings>.
QByteArray newValuesFileText();

// One list item to store: field name -> text. Fields the option does not have are ignored.
using ItemFields = QHash<QString, QString>;

// Remove the element of `key` (and its whole lines when it stands alone on them). Data unchanged when not set.
QByteArray removeValueElement(const QByteArray& data, const QString& key, QString* error);

// Set a list value: every field of every item in the option's field order, a missing field taking its default.
// An existing element of the key is replaced from its start tag to its end tag; a new one goes where setValue puts
// a new key.
QByteArray editListValue(const QByteArray& data, const Option& option, const QList<ItemFields>& items,
                         QString* error);

// Whether `text` is the option's default (numbers compared as numbers).
bool isDefaultText(const Option& option, const QString& text);

// Store one scalar value in the values file `path`, as the panel does on a change: a value equal to the default
// removes the key; otherwise the key is set (the file is created first, as newValuesFileText(), when missing).
// The file is changed atomically, or not at all. False with a one-line *error (nothing changed) on a refused value.
bool storeValue(const QString& path, const Description& description, const QString& schemaDir, const QString& key,
                const QString& text, QString* error);

// The same for a list: an empty list is the default and removes the key.
bool storeList(const QString& path, const Description& description, const QString& schemaDir, const QString& key,
               const QList<ItemFields>& items, QString* error);

}  // namespace riley::config
