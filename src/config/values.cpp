#include "values.h"

#include "atomic_file.h"
#include "xml.h"

#include <QFile>

#include <libxml/tree.h>

namespace riley::config {
namespace {

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool allSpace(const QByteArray& data, qsizetype from, qsizetype to)
{
    for (qsizetype i = from; i < to; ++i) {
        if (!isSpace(data[i]))
            return false;
    }
    return true;
}

// An attribute value as written, with XML's predefined and numeric character references decoded.
QString decodeAttr(const QByteArray& raw)
{
    QString out;
    const QString s = QString::fromUtf8(raw);
    for (qsizetype i = 0; i < s.size(); ++i) {
        if (s[i] != QLatin1Char('&')) {
            out += s[i];
            continue;
        }
        const qsizetype semi = s.indexOf(QLatin1Char(';'), i);
        if (semi < 0) {
            out += s.mid(i);
            break;
        }
        const QString ref = s.mid(i + 1, semi - i - 1);
        if (ref == QLatin1String("amp")) out += QLatin1Char('&');
        else if (ref == QLatin1String("lt")) out += QLatin1Char('<');
        else if (ref == QLatin1String("gt")) out += QLatin1Char('>');
        else if (ref == QLatin1String("quot")) out += QLatin1Char('"');
        else if (ref == QLatin1String("apos")) out += QLatin1Char('\'');
        else if (ref.startsWith(QLatin1Char('#'))) {
            bool ok = false;
            const uint code = ref.startsWith(QLatin1String("#x")) ? ref.mid(2).toUInt(&ok, 16) : ref.mid(1).toUInt(&ok, 10);
            if (ok)
                out += QString::fromUcs4(reinterpret_cast<const char32_t*>(&code), 1);
        } else {
            out += QLatin1Char('&') + ref + QLatin1Char(';');
        }
        i = semi;
    }
    return out;
}

// One piece of markup or text found by the scanner.
enum class Kind { Text, Comment, CData, Pi, StartTag, EndTag };
struct Token {
    Kind kind;
    qsizetype start;   // first byte
    qsizetype end;     // one past the last byte
    QByteArray name;   // start and end tags
    bool selfClosing = false;
    QString key;       // a start tag's key attribute, decoded
    bool hasKey = false;
};

// Splits an XML document into tokens. It only needs to be right for files the schema has accepted already.
bool scan(const QByteArray& d, std::vector<Token>* out, QString* error)
{
    qsizetype pos = 0;
    const qsizetype n = d.size();
    auto until = [&](qsizetype from, const char* end) { return d.indexOf(end, from); };
    while (pos < n) {
        if (d[pos] != '<') {
            qsizetype next = d.indexOf('<', pos);
            if (next < 0)
                next = n;
            out->push_back({Kind::Text, pos, next, {}, false, {}, false});
            pos = next;
            continue;
        }
        if (d.mid(pos, 4) == "<!--") {
            const qsizetype e = until(pos + 4, "-->");
            if (e < 0) { *error = QStringLiteral("a comment does not end"); return false; }
            out->push_back({Kind::Comment, pos, e + 3, {}, false, {}, false});
            pos = e + 3;
        } else if (d.mid(pos, 9) == "<![CDATA[") {
            const qsizetype e = until(pos + 9, "]]>");
            if (e < 0) { *error = QStringLiteral("a CDATA section does not end"); return false; }
            out->push_back({Kind::CData, pos, e + 3, {}, false, {}, false});
            pos = e + 3;
        } else if (d.mid(pos, 2) == "<?") {
            const qsizetype e = until(pos + 2, "?>");
            if (e < 0) { *error = QStringLiteral("a processing instruction does not end"); return false; }
            out->push_back({Kind::Pi, pos, e + 2, {}, false, {}, false});
            pos = e + 2;
        } else if (d.mid(pos, 2) == "<!") {
            *error = QStringLiteral("a DOCTYPE is not allowed");
            return false;
        } else {
            Token t{Kind::StartTag, pos, pos, {}, false, {}, false};
            qsizetype i = pos + 1;
            if (i < n && d[i] == '/') {
                t.kind = Kind::EndTag;
                ++i;
            }
            const qsizetype nameStart = i;
            while (i < n && !isSpace(d[i]) && d[i] != '/' && d[i] != '>')
                ++i;
            t.name = d.mid(nameStart, i - nameStart);
            for (;;) {
                while (i < n && isSpace(d[i]))
                    ++i;
                if (i >= n) { *error = QStringLiteral("a tag does not end"); return false; }
                if (d[i] == '>') { ++i; break; }
                if (d[i] == '/' && i + 1 < n && d[i + 1] == '>') { t.selfClosing = true; i += 2; break; }
                const qsizetype an = i;
                while (i < n && !isSpace(d[i]) && d[i] != '=' && d[i] != '>' && d[i] != '/')
                    ++i;
                const QByteArray attrName = d.mid(an, i - an);
                while (i < n && isSpace(d[i]))
                    ++i;
                if (i >= n || d[i] != '=') { *error = QStringLiteral("an attribute has no value"); return false; }
                ++i;
                while (i < n && isSpace(d[i]))
                    ++i;
                if (i >= n || (d[i] != '"' && d[i] != '\'')) { *error = QStringLiteral("an attribute is not quoted"); return false; }
                const char q = d[i];
                const qsizetype close = d.indexOf(q, i + 1);
                if (close < 0) { *error = QStringLiteral("an attribute value does not end"); return false; }
                if (attrName == "key") {
                    t.key = decodeAttr(d.mid(i + 1, close - i - 1));
                    t.hasKey = true;
                }
                i = close + 1;
            }
            t.end = i;
            out->push_back(t);
            pos = i;
        }
    }
    return true;
}

QString valueText(const Typed& t, const QString& raw)
{
    switch (t.type) {
    case OptionType::Switch:
    case OptionType::Choice:
    case OptionType::Slider:
    case OptionType::Number:
    case OptionType::Colour:
        return raw.trimmed();
    default:
        return raw;
    }
}

}  // namespace

QByteArray escapeText(const QString& text)
{
    QByteArray out;
    for (const char c : text.toUtf8()) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        default: out += c;
        }
    }
    return out;
}

std::unique_ptr<Values> Values::parse(const QByteArray& data, const QString& name, const QString& schemaDir,
                                      QString* error)
{
    xml::Doc doc = xml::parseAndValidate(data, name, schemaDir + QStringLiteral("/values.xsd"), error);
    if (!doc)
        return nullptr;
    std::unique_ptr<Values> v(new Values);
    v->m_path = name;
    for (const xmlNode* n = xml::firstChild(xml::root(doc)); n; n = xml::next(n)) {
        if (!xml::isElement(n, "value"))
            continue;
        Value value;
        value.key = xml::attr(n, "key");
        value.text = xml::text(n);
        value.line = xml::line(n);
        for (const xmlNode* c = xml::firstChild(n); c; c = xml::next(c)) {
            if (!xml::isElement(c, "item"))
                continue;
            ListItem item;
            item.line = xml::line(c);
            for (const xmlAttr* a = c->properties; a; a = a->next) {
                const QString attrName = QString::fromUtf8(reinterpret_cast<const char*>(a->name));
                item.attrs.insert(attrName, xml::attr(c, reinterpret_cast<const char*>(a->name)));
                item.order.append(attrName);
            }
            value.items.append(item);
        }
        v->m_byKey.insert(value.key, int(v->m_values.size()));   // unique: the schema's xs:unique
        v->m_values.push_back(std::move(value));
    }
    return v;
}

std::unique_ptr<Values> Values::load(const QString& path, const QString& schemaDir, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = path + QStringLiteral(": ") + f.errorString();
        return nullptr;
    }
    return parse(f.readAll(), path, schemaDir, error);
}

const Value* Values::value(const QString& key) const
{
    const auto it = m_byKey.constFind(key);
    return it == m_byKey.constEnd() ? nullptr : &m_values[size_t(*it)];
}

bool Values::check(const Description& description, QStringList* unknown, QString* error) const
{
    for (const Value& v : m_values) {
        const Option* o = description.option(v.key);
        const QString at = m_path + QLatin1Char(':') + QString::number(v.line) + QStringLiteral(": ") + v.key;
        if (!o) {
            unknown->append(v.key);
            continue;
        }
        if (o->type == OptionType::List) {
            if (!v.text.trimmed().isEmpty()) {
                *error = at + QStringLiteral(": a list holds items, not text");
                return false;
            }
            for (const ListItem& item : v.items) {
                for (const QString& name : item.order) {
                    const Field* f = o->field(name);
                    const QString itemAt = m_path + QLatin1Char(':') + QString::number(item.line) + QStringLiteral(": ")
                        + v.key + QLatin1Char(' ') + name;
                    if (!f) {
                        *error = itemAt + QStringLiteral(": not a field of this list");
                        return false;
                    }
                    const QString text = valueText(*f, item.attrs.value(name));
                    const QString why = checkScalar(*f, text);
                    if (!why.isEmpty()) {
                        *error = itemAt + QStringLiteral(": \"") + text + QStringLiteral("\" ") + why;
                        return false;
                    }
                }
            }
            continue;
        }
        if (!v.items.isEmpty()) {
            *error = at + QStringLiteral(": holds items, but it is a ") + optionTypeName(o->type);
            return false;
        }
        const QString text = valueText(*o, v.text);
        const QString why = checkScalar(*o, text);
        if (!why.isEmpty()) {
            *error = at + QStringLiteral(": \"") + text + QStringLiteral("\" ") + why;
            return false;
        }
    }
    return true;
}

QByteArray editValueText(const QByteArray& data, const QString& key, const QString& text, QString* error)
{
    std::vector<Token> tokens;
    if (!scan(data, &tokens, error))
        return {};

    const QByteArray escaped = escapeText(text);
    int depth = 0;
    qsizetype rootEnd = -1;         // index of the root's end tag token
    qsizetype rootSelfClosing = -1; // index of a self-closing root start tag
    qsizetype lastValue = -1;       // index of the last <value> start tag at depth 1
    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token& t = tokens[i];
        if (t.kind == Kind::EndTag) {
            --depth;
            if (depth == 0)
                rootEnd = qsizetype(i);
            continue;
        }
        if (t.kind != Kind::StartTag)
            continue;
        if (depth == 0 && t.selfClosing)
            rootSelfClosing = qsizetype(i);
        if (depth == 1 && t.name == "value") {
            lastValue = qsizetype(i);
            if (t.hasKey && t.key == key) {
                if (t.selfClosing) {
                    // <value key="k"/>  ->  <value key="k">text</value>
                    const qsizetype slash = data.lastIndexOf('/', t.end - 1);
                    return data.left(slash) + '>' + escaped + "</value>" + data.mid(t.end);
                }
                // The content runs to the matching end tag. Text is replaced; comments inside are kept.
                std::vector<const Token*> inside;
                int d = 0;
                size_t j = i + 1;
                for (; j < tokens.size(); ++j) {
                    const Token& u = tokens[j];
                    if (u.kind == Kind::EndTag) {
                        if (d == 0)
                            break;
                        --d;
                    } else if (u.kind == Kind::StartTag) {
                        *error = key + QStringLiteral(" holds items; a list is not set from the command line");
                        return {};
                    }
                    inside.push_back(&u);
                }
                if (j == tokens.size()) {
                    *error = QStringLiteral("the value of ") + key + QStringLiteral(" does not end");
                    return {};
                }
                const qsizetype from = t.end;
                const qsizetype to = tokens[j].start;
                bool plain = true;
                for (const Token* u : inside)
                    plain = plain && u->kind == Kind::Text;
                if (plain)
                    return data.left(from) + escaped + data.mid(to);
                // Mixed with comments: the new text goes where the first text was (or first), other text goes.
                QByteArray content;
                bool placed = false;
                for (const Token* u : inside) {
                    if (u->kind == Kind::Text || u->kind == Kind::CData) {
                        if (!placed)
                            content += escaped;
                        placed = true;
                    } else {
                        if (!placed && u == inside.front()) {
                            content += escaped;
                            placed = true;
                        }
                        content += data.mid(u->start, u->end - u->start);
                    }
                }
                return data.left(from) + content + data.mid(to);
            }
        }
        if (!t.selfClosing)
            ++depth;
    }

    // Not in the file yet: a new line before </settings>, indented like the last value.
    const QByteArray newline = data.contains("\r\n") ? QByteArray("\r\n") : QByteArray("\n");
    const QByteArray element = "<value key=\"" + escapeText(key).replace('"', "&quot;") + "\">" + escaped + "</value>";
    QByteArray indent = "  ";
    if (lastValue >= 0) {
        const qsizetype s = tokens[size_t(lastValue)].start;
        const qsizetype ls = data.lastIndexOf('\n', s - 1) + 1;
        if (allSpace(data, ls, s))
            indent = data.mid(ls, s - ls);
    }
    if (rootSelfClosing >= 0) {
        const Token& t = tokens[size_t(rootSelfClosing)];
        const qsizetype slash = data.lastIndexOf('/', t.end - 1);
        return data.left(slash) + '>' + newline + indent + element + newline + "</" + t.name + '>' + data.mid(t.end);
    }
    if (rootEnd < 0) {
        *error = QStringLiteral("the file has no </settings>");
        return {};
    }
    const qsizetype at = tokens[size_t(rootEnd)].start;
    const qsizetype ls = data.lastIndexOf('\n', at - 1) + 1;
    if (ls > 0 && allSpace(data, ls, at))
        return data.left(ls) + indent + element + newline + data.mid(ls);
    return data.left(at) + element + data.mid(at);
}

bool setValue(const QString& path, const Description& description, const QString& schemaDir, const QString& key,
              const QString& text, QString* error)
{
    const Option* o = description.option(key);
    if (!o) {
        *error = QStringLiteral("unknown key: ") + key;
        return false;
    }
    if (o->type == OptionType::List) {
        *error = key + QStringLiteral(" is a list; a list is not set from the command line");
        return false;
    }
    const QString why = checkScalar(*o, text);
    if (!why.isEmpty()) {
        *error = key + QStringLiteral(": \"") + text + QStringLiteral("\" ") + why;
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = path + QStringLiteral(": ") + f.errorString();
        return false;
    }
    const QByteArray before = f.readAll();
    f.close();
    if (!Values::parse(before, path, schemaDir, error))
        return false;

    QString scanError;
    const QByteArray after = editValueText(before, key, text, &scanError);
    if (after.isEmpty()) {
        *error = path + QStringLiteral(": ") + scanError;
        return false;
    }
    // The edited text must still be a valid values file, holding exactly the new value.
    const auto check = Values::parse(after, path, schemaDir, error);
    if (!check)
        return false;
    const Value* v = check->value(key);
    if (!v || v->text != text || !v->items.isEmpty()) {
        *error = path + QStringLiteral(": the edit of ") + key + QStringLiteral(" did not hold; file left unchanged");
        return false;
    }
    return writeFileAtomic(path, after, error);
}

}  // namespace riley::config
