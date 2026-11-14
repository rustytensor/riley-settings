// Writer for the target "riley-overview": ~/.config/riley-overview/config.xml, read by riley-overview when it starts
// (its schema: riley-overview's data/config.xsd, installed as $XDG_DATA_HOME/riley-overview/config.xsd).
//
// Only what the Overview page sets is written: <click>, <click-key> and the attributes of <hot-corner> whose keys
// the values file sets. Everything else of an existing config.xml (margin, launcher, comments inside <overview>,
// an attribute of <hot-corner> the page does not set) is kept as it was, in its order; an element the page sets
// replaces the old one in place, or goes at the end. With no Overview key set the file is left alone.
//
// Refused (nothing written, one line): a value riley-overview's schema would not take, an existing file that is not
// well-formed or whose root is not riley-overview's <overview>, and a result that the installed schema rejects (an
// existing element out of range, for example), so riley-overview never falls back to its defaults because of a save.
#include "writers.h"

#include "../xml.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <cmath>
#include <optional>

namespace riley::config::writers {
namespace {

const QString kNs = QStringLiteral("urn:riley-overview:config");
const QString kClick = QStringLiteral("overview.click");
const QString kClickKey = QStringLiteral("overview.click_key");
const QString kCorner = QStringLiteral("overview.hot_corner");
const QString kScreens = QStringLiteral("overview.hot_corner_screens");
const QString kDelay = QStringLiteral("overview.hot_corner_delay");

// One node directly inside <overview>, as it was written.
struct Node {
    QString name;   // the element's local name in riley-overview's namespace; empty for a comment or another element
    QString raw;    // its text, from '<' to the end of its closing tag
    QXmlStreamAttributes attributes;
};

// The installed schema of riley-overview, or "" when there is none (then only well-formedness is checked).
QString findSchema()
{
    QStringList dirs;
    const QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    dirs.append(dataHome.isEmpty() ? QDir::homePath() + QStringLiteral("/.local/share") : dataHome);
    const QString dataDirs = qEnvironmentVariable("XDG_DATA_DIRS");
    dirs.append((dataDirs.isEmpty() ? QStringLiteral("/usr/local/share:/usr/share") : dataDirs)
                    .split(QLatin1Char(':'), Qt::SkipEmptyParts));
    for (const QString& d : std::as_const(dirs)) {
        const QString path = d + QStringLiteral("/riley-overview/config.xsd");
        if (QFile::exists(path))
            return path;
    }
    return {};
}

// The nodes inside the root of an existing config.xml. False with a one-line *error when it is not riley-overview's.
bool readExisting(const QByteArray& data, QList<Node>* nodes, QString* error)
{
    const QString text = QString::fromUtf8(data);
    QXmlStreamReader r(text);
    bool inRoot = false;
    while (!r.atEnd()) {
        const auto token = r.readNext();
        if (token == QXmlStreamReader::DTD) {
            *error = QStringLiteral("config.xml has a DOCTYPE; riley-overview's file has none");
            return false;
        }
        if (token == QXmlStreamReader::Invalid)
            break;
        if (!inRoot) {
            if (token != QXmlStreamReader::StartElement)
                continue;
            if (r.name() != QLatin1String("overview") || r.namespaceUri() != kNs || !r.prefix().isEmpty()) {
                *error = QStringLiteral("config.xml is not riley-overview's (its root is <%1>); fix or remove it")
                             .arg(r.qualifiedName().toString());
                return false;
            }
            inRoot = true;
            continue;
        }
        if (token == QXmlStreamReader::EndElement)
            break;   // the end of <overview>
        // The reader reads one character ahead (a text token ends after the next '<'), so a node starts at its own
        // "<": the last "<" before the end of its start tag ('<' cannot appear in an attribute), or the last "<!--"
        // before the end of a comment ("--" cannot appear in one).
        if (token == QXmlStreamReader::Comment) {
            const qint64 end = r.characterOffset();
            const qint64 start = text.lastIndexOf(QStringLiteral("<!--"), end - 1);
            nodes->append({QString(), text.mid(start, end - start), {}});
        } else if (token == QXmlStreamReader::StartElement) {
            if (!r.prefix().isEmpty()) {
                *error = QStringLiteral("config.xml line %1: <%2> has a namespace prefix; write it without one")
                             .arg(r.lineNumber())
                             .arg(r.qualifiedName().toString());
                return false;
            }
            const qint64 start = text.lastIndexOf(QLatin1Char('<'), r.characterOffset() - 1);
            Node n;
            n.name = r.namespaceUri() == kNs ? r.name().toString() : QString();
            n.attributes = r.attributes();
            r.skipCurrentElement();
            if (r.hasError())
                break;
            n.raw = text.mid(start, r.characterOffset() - start);
            nodes->append(n);
        }
    }
    if (r.hasError()) {
        *error = QStringLiteral("config.xml line %1: %2").arg(r.lineNumber()).arg(r.errorString());
        return false;
    }
    if (!inRoot) {
        *error = QStringLiteral("config.xml has no <overview> element; fix or remove it");
        return false;
    }
    return true;
}

class RileyOverview final : public Writer {
public:
    QString target() const override { return QStringLiteral("riley-overview"); }

    // The rest of an existing file is kept, so the writer is given it.
    bool ownBlockOnly() const override { return true; }

    bool leavesFileAlone(const Config& config) const override
    {
        for (const QString& key : {kClick, kClickKey, kCorner, kScreens, kDelay}) {
            if (config.isSet(key))
                return false;
        }
        return true;
    }

    bool render(const Config& config, const QByteArray* existing, QByteArray* out, QString* error) const override
    {
        // What the page sets, checked against riley-overview's types.
        std::optional<QString> click, clickKey, corner, screens, delay;
        if (config.isSet(kClick)) {
            click = config.text(kClick).trimmed();
            static const QStringList ok = {QStringLiteral("fullscreen"), QStringLiteral("maximize"), QStringLiteral("focus")};
            if (!ok.contains(*click))
                return refuse(error, kClick, *click, QStringLiteral("fullscreen, maximize or focus"));
        }
        if (config.isSet(kClickKey) && !config.text(kClickKey).trimmed().isEmpty()) {   // empty keeps the default
            clickKey = config.text(kClickKey).trimmed();
            static const QRegularExpression one(QStringLiteral("^[A-Za-z0-9]$"));
            if (!one.match(*clickKey).hasMatch())
                return refuse(error, kClickKey, *clickKey, QStringLiteral("one letter or digit"));
        }
        if (config.isSet(kCorner)) {
            corner = config.text(kCorner).trimmed();
            static const QStringList ok = {QStringLiteral("top-left"), QStringLiteral("top-right"),
                                           QStringLiteral("bottom-left"), QStringLiteral("bottom-right"),
                                           QStringLiteral("off")};
            if (!ok.contains(*corner))
                return refuse(error, kCorner, *corner, QStringLiteral("top-left, top-right, bottom-left, bottom-right or off"));
        }
        if (config.isSet(kScreens)) {
            // Names separated by spaces or commas, written with single spaces; empty keeps the default.
            static const QRegularExpression sep(QStringLiteral("[\\s,]+"));
            static const QRegularExpression name(QStringLiteral("^[A-Za-z0-9_.:\\-]+$"));
            const QStringList names = config.text(kScreens).split(sep, Qt::SkipEmptyParts);
            for (const QString& n : names) {
                if (!name.match(n).hasMatch())
                    return refuse(error, kScreens, n, QStringLiteral("all, primary or screen names (letters, digits, _ . : -)"));
            }
            if (!names.isEmpty())
                screens = names.join(QLatin1Char(' '));
        }
        if (config.isSet(kDelay)) {
            const double d = config.number(kDelay);
            if (!(d >= 50 && d <= 1000) || d != std::floor(d))
                return refuse(error, kDelay, config.text(kDelay), QStringLiteral("a whole number from 50 to 1000"));
            delay = QString::number(static_cast<long long>(d));
        }

        QList<Node> nodes;
        if (existing && !readExisting(*existing, &nodes, error))
            return false;
#ifdef RILEY_PLANT_FAULT_OVERVIEW
        nodes.removeIf([](const Node& n) { return n.name.isEmpty() || (n.name != QLatin1String("click")
                                                                        && n.name != QLatin1String("click-key")
                                                                        && n.name != QLatin1String("hot-corner")); });
#endif

        const bool cornerSet = corner || screens || delay;
        const auto hotCorner = [&](const QXmlStreamAttributes& old) {
            QString s = QStringLiteral("<hot-corner");
            const auto attr = [&](const char* name, const std::optional<QString>& set) {
                const QString n = QString::fromLatin1(name);
                const QString v = set ? *set : old.value(n).toString();
                if (set || old.hasAttribute(n))
                    s += QLatin1Char(' ') + n + QStringLiteral("=\"") + v.toHtmlEscaped() + QLatin1Char('"');
            };
            attr("corner", corner);
            attr("screens", screens);
            attr("delay-ms", delay);
            return s + QStringLiteral("/>");
        };
        const auto element = [](const char* name, const QString& value) {
            const QString n = QString::fromLatin1(name);
            return QLatin1Char('<') + n + QLatin1Char('>') + value.toHtmlEscaped() + QStringLiteral("</") + n
                + QLatin1Char('>');
        };

        QStringList body;
        bool doneClick = false, doneKey = false, doneCorner = false;
        for (const Node& n : std::as_const(nodes)) {
            if (n.name == QLatin1String("click") && click) {
                if (!doneClick)
                    body.append(element("click", *click));
                doneClick = true;
            } else if (n.name == QLatin1String("click-key") && clickKey) {
                if (!doneKey)
                    body.append(element("click-key", *clickKey));
                doneKey = true;
            } else if (n.name == QLatin1String("hot-corner") && cornerSet) {
                if (!doneCorner)
                    body.append(hotCorner(n.attributes));
                doneCorner = true;
            } else {
                body.append(n.raw);
            }
        }
        if (click && !doneClick)
            body.append(element("click", *click));
        if (clickKey && !doneKey)
            body.append(element("click-key", *clickKey));
        if (cornerSet && !doneCorner)
            body.append(hotCorner({}));

        QString s = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!-- Written by Riley Settings (Overview). Elements it does not set are kept as they were. -->\n"
            "<overview xmlns=\"urn:riley-overview:config\" version=\"1\">\n");
        for (const QString& b : std::as_const(body))
            s += QStringLiteral("  ") + b + QLatin1Char('\n');
        s += QStringLiteral("</overview>\n");
        const QByteArray bytes = s.toUtf8();

        // The result must be one riley-overview takes.
        QXmlStreamReader check(bytes);
        while (!check.atEnd())
            check.readNext();
        if (check.hasError()) {
            *error = QStringLiteral("config.xml would not be well-formed (line %1: %2); fix the file first")
                         .arg(check.lineNumber())
                         .arg(check.errorString());
            return false;
        }
        const QString xsd = findSchema();
        if (!xsd.isEmpty()) {
            QString why;
            if (!xml::parseAndValidate(bytes, QStringLiteral("config.xml"), xsd, &why)) {
                *error = QStringLiteral("riley-overview's schema rejects config.xml: ") + why;
                return false;
            }
        }
        *out = bytes;
        return true;
    }

private:
    static bool refuse(QString* error, const QString& key, const QString& value, const QString& want)
    {
        *error = key + QStringLiteral(": \"") + value + QStringLiteral("\" is not ") + want;
        return false;
    }
};

}  // namespace

std::unique_ptr<Writer> makeRileyOverview()
{
    return std::make_unique<RileyOverview>();
}

}  // namespace riley::config::writers
