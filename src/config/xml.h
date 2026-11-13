// libxml2 behind a small interface: parse a file or a buffer, check it against an XSD, report the first problem as
// one line. Nothing else in the library talks to libxml2's error machinery.
#pragma once

#include <QByteArray>
#include <QString>

#include <memory>

struct _xmlDoc;
struct _xmlNode;

namespace riley::config::xml {

struct DocDeleter {
    void operator()(_xmlDoc* doc) const;
};
using Doc = std::unique_ptr<_xmlDoc, DocDeleter>;

// Parse `data` (named `name` in messages) and validate it against the schema file `xsdPath`.
// Refuses a DOCTYPE (no entities, no external anything). On failure returns null and sets *error to one line.
Doc parseAndValidate(const QByteArray& data, const QString& name, const QString& xsdPath, QString* error);

// Read `path` and do the same.
Doc loadAndValidate(const QString& path, const QString& xsdPath, QString* error);

// Small helpers over the libxml2 tree.
_xmlNode* root(const Doc& doc);
bool isElement(const _xmlNode* node, const char* name);
_xmlNode* firstChild(const _xmlNode* node);
_xmlNode* next(const _xmlNode* node);
bool hasAttr(const _xmlNode* node, const char* name);
QString attr(const _xmlNode* node, const char* name);
QString text(const _xmlNode* node);   // the concatenated text of the node's direct text and CDATA children
int line(const _xmlNode* node);

}  // namespace riley::config::xml
