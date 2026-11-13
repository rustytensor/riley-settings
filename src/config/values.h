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

}  // namespace riley::config
