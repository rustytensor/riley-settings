#include "xml.h"

#include <QFile>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlschemas.h>

namespace riley::config::xml {
namespace {

// Keeps the first error libxml2 reports, as one line: "file:line: message".
struct FirstError {
    QString text;
};

QString oneLine(QString s)
{
    s.replace(QLatin1Char('\r'), QLatin1Char(' '));
    s.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return s.simplified();
}

void keepFirst(void* data, const xmlError* err)
{
    auto* first = static_cast<FirstError*>(data);
    if (!first->text.isEmpty() || !err || err->level == XML_ERR_WARNING)
        return;
    QString where = err->file ? QString::fromUtf8(err->file) : QStringLiteral("(input)");
    if (err->line > 0)
        where += QLatin1Char(':') + QString::number(err->line);
    first->text = where + QStringLiteral(": ") + oneLine(QString::fromUtf8(err->message ? err->message : "error"));
}

void ignoreGeneric(void*, const char*, ...) {}

struct ParserCtxtDeleter {
    void operator()(xmlParserCtxt* c) const { xmlFreeParserCtxt(c); }
};
struct SchemaParserDeleter {
    void operator()(xmlSchemaParserCtxt* c) const { xmlSchemaFreeParserCtxt(c); }
};
struct SchemaDeleter {
    void operator()(xmlSchema* s) const { xmlSchemaFree(s); }
};
struct ValidDeleter {
    void operator()(xmlSchemaValidCtxt* c) const { xmlSchemaFreeValidCtxt(c); }
};

// Anything libxml2 would print by itself goes nowhere: our errors come through keepFirst only.
void silence()
{
    static const bool done = [] {
        xmlSetGenericErrorFunc(nullptr, ignoreGeneric);
        xmlSetStructuredErrorFunc(nullptr, [](void*, const xmlError*) {});
        return true;
    }();
    (void)done;
}

}  // namespace

void DocDeleter::operator()(_xmlDoc* doc) const
{
    xmlFreeDoc(doc);
}

Doc parseAndValidate(const QByteArray& data, const QString& name, const QString& xsdPath, QString* error)
{
    silence();
    const QByteArray nameUtf8 = name.toUtf8();

    FirstError first;
    std::unique_ptr<xmlParserCtxt, ParserCtxtDeleter> ctxt(xmlNewParserCtxt());
    if (!ctxt) {
        *error = QStringLiteral("out of memory");
        return nullptr;
    }
    xmlCtxtSetErrorHandler(ctxt.get(), keepFirst, &first);
    Doc doc(xmlCtxtReadMemory(ctxt.get(), data.constData(), int(data.size()), nameUtf8.constData(), "UTF-8",
                              XML_PARSE_NONET | XML_PARSE_NO_XXE));
    if (!doc) {
        *error = first.text.isEmpty() ? name + QStringLiteral(": not well-formed XML") : first.text;
        return nullptr;
    }
    if (doc->intSubset || doc->extSubset) {
        *error = name + QStringLiteral(": a DOCTYPE is not allowed");
        return nullptr;
    }

    FirstError schemaError;
    const QByteArray xsd = QFile::encodeName(xsdPath);
    std::unique_ptr<xmlSchemaParserCtxt, SchemaParserDeleter> sp(xmlSchemaNewParserCtxt(xsd.constData()));
    if (!sp) {
        *error = xsdPath + QStringLiteral(": cannot read the schema");
        return nullptr;
    }
    xmlSchemaSetParserStructuredErrors(sp.get(), keepFirst, &schemaError);
    std::unique_ptr<xmlSchema, SchemaDeleter> schema(xmlSchemaParse(sp.get()));
    if (!schema) {
        *error = schemaError.text.isEmpty() ? xsdPath + QStringLiteral(": not a valid schema") : schemaError.text;
        return nullptr;
    }
    std::unique_ptr<xmlSchemaValidCtxt, ValidDeleter> vc(xmlSchemaNewValidCtxt(schema.get()));
    if (!vc) {
        *error = QStringLiteral("out of memory");
        return nullptr;
    }
    FirstError invalid;
    xmlSchemaSetValidStructuredErrors(vc.get(), keepFirst, &invalid);
    const int rc = xmlSchemaValidateDoc(vc.get(), doc.get());
    if (rc != 0) {
        *error = invalid.text.isEmpty() ? name + QStringLiteral(": does not match its schema") : invalid.text;
        return nullptr;
    }
    return doc;
}

Doc loadAndValidate(const QString& path, const QString& xsdPath, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = path + QStringLiteral(": ") + f.errorString();
        return nullptr;
    }
    return parseAndValidate(f.readAll(), path, xsdPath, error);
}

_xmlNode* root(const Doc& doc)
{
    return xmlDocGetRootElement(doc.get());
}

bool isElement(const _xmlNode* node, const char* name)
{
    return node && node->type == XML_ELEMENT_NODE && xmlStrEqual(node->name, BAD_CAST name);
}

_xmlNode* firstChild(const _xmlNode* node)
{
    return node ? node->children : nullptr;
}

_xmlNode* next(const _xmlNode* node)
{
    return node ? node->next : nullptr;
}

bool hasAttr(const _xmlNode* node, const char* name)
{
    return xmlHasProp(node, BAD_CAST name) != nullptr;
}

QString attr(const _xmlNode* node, const char* name)
{
    xmlChar* v = xmlGetProp(node, BAD_CAST name);
    if (!v)
        return {};
    QString s = QString::fromUtf8(reinterpret_cast<const char*>(v));
    xmlFree(v);
    return s;
}

QString text(const _xmlNode* node)
{
    QString out;
    for (const xmlNode* c = node ? node->children : nullptr; c; c = c->next) {
        if ((c->type == XML_TEXT_NODE || c->type == XML_CDATA_SECTION_NODE) && c->content)
            out += QString::fromUtf8(reinterpret_cast<const char*>(c->content));
    }
    return out;
}

int line(const _xmlNode* node)
{
    return node ? int(xmlGetLineNo(node)) : 0;
}

}  // namespace riley::config::xml
