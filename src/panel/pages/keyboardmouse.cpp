#include "keyboardmouse.h"

#include <QFile>
#include <QPair>
#include <QRegularExpression>

#include <algorithm>

namespace riley::panel::pages {

namespace {

const QString EVDEV = QStringLiteral("/usr/share/X11/xkb/rules/evdev.lst");
// The roles of settings.xml's three keyboard options: how the picker finds them without naming their keys.
const QString ROLE_LAYOUT = QStringLiteral("kb-layout");
const QString ROLE_VARIANT = QStringLiteral("kb-variant");
const QString ROLE_OPTIONS = QStringLiteral("kb-options");

// Riley's SWITCH_KEYS (not Super+Space: that opens the launcher).
struct SwitchKey {
    const char* option;
    const char* name;
};
constexpr SwitchKey SWITCH_KEYS[] = {
    {"grp:alt_shift_toggle", "Alt + Shift"},
    {"grp:ctrl_shift_toggle", "Ctrl + Shift"},
    {"grp:caps_toggle", "Caps Lock"},
};

}  // namespace

KeyboardLayouts parseEvdev(const QString& text, bool exists)
{
    // Riley's layouts(): re.match(r"^\s+(\S+)\s+(.+)$", line) in the "layout" section. The contract's variant rule:
    // the same line in the "variant" section, whose description starts with "<code>:".
    static const QRegularExpression entry(QStringLiteral("^\\s+(\\S+)\\s+(.+)$"));
    static const QRegularExpression ofLayout(QStringLiteral("^(\\S+):"));
    KeyboardLayouts out;
    QList<QPair<QString, QString>> layouts;
    if (exists) {
        QString section;
        for (const QString& line : text.split(QLatin1Char('\n'))) {
            if (line.startsWith(QLatin1Char('!'))) {
                section = line.mid(1).trimmed();
                continue;
            }
            const auto m = entry.match(line);
            if (!m.hasMatch())
                continue;
            if (section == QLatin1String("layout")) {
                layouts.append({m.captured(1), m.captured(2).trimmed()});
            } else if (section == QLatin1String("variant")) {
                const auto v = ofLayout.match(m.captured(2));
                if (v.hasMatch())
                    out.variants[v.captured(1)].append(m.captured(1));
            }
        }
    }
    // sorted(out, key=lambda x: x[1].lower()): a stable sort by the lower-cased name.
    std::stable_sort(layouts.begin(), layouts.end(),
                     [](const auto& a, const auto& b) { return a.second.toLower() < b.second.toLower(); });
    if (layouts.isEmpty())
        layouts.append({QStringLiteral("us"), QStringLiteral("English (US)")});
    for (const auto& l : std::as_const(layouts)) {
        out.codes.append(l.first);
        out.names.append(l.second);
    }
    return out;
}

KeyboardMouseBackend::KeyboardMouseBackend(QObject* parent) : PageBackend(parent)
{
    connect(this, &PageBackend::panelChanged, this, &KeyboardMouseBackend::rebuild);
    connect(this, &PageBackend::pageChanged, this, &KeyboardMouseBackend::rebuild);
    QFile f(EVDEV);
    const bool exists = f.open(QIODevice::ReadOnly);
    m_layouts = parseEvdev(exists ? QString::fromUtf8(f.readAll()) : QString(), exists);
}

void KeyboardMouseBackend::rebuild()
{
    m_options = page() ? page()->scalarOptions() : QList<QObject*>();
    // The M3 gate's planted fault: the page's second option is drawn as itself but saves to the third option's key.
    if (Controller::plantFaultPages() && panel() && m_options.size() > 2) {
        auto* second = qobject_cast<OptionItem*>(m_options[1]);
        auto* third = qobject_cast<OptionItem*>(m_options[2]);
        if (second && third) {
            auto* wrong = new OptionItem(panel(), second->shown(), third->shown(), this);
            m_own.append(wrong);
            m_options[1] = wrong;
        }
    }
    // The picker shows the present choice: follow the keyboard options' values.
    for (const QString& role : {ROLE_LAYOUT, ROLE_VARIANT, ROLE_OPTIONS}) {
        if (OptionItem* item = option(role))
            connect(item, &OptionItem::valueChanged, this, &KeyboardMouseBackend::changed, Qt::UniqueConnection);
    }
    Q_EMIT changed();
}

bool KeyboardMouseBackend::refresh()
{
    for (QObject* o : std::as_const(m_options)) {
        if (auto* item = qobject_cast<OptionItem*>(o))
            item->refresh();
    }
    Q_EMIT changed();
    return true;
}

OptionItem* KeyboardMouseBackend::option(const QString& role) const
{
    return page() ? page()->optionByRole(role) : nullptr;
}

QStringList KeyboardMouseBackend::switchKeys() const
{
    QStringList out;
    for (const SwitchKey& k : SWITCH_KEYS)
        out.append(QString::fromLatin1(k.option));
    return out;
}

QStringList KeyboardMouseBackend::switchKeyNames() const
{
    QStringList out;
    for (const SwitchKey& k : SWITCH_KEYS)
        out.append(QString::fromLatin1(k.name));
    return out;
}

QStringList KeyboardMouseBackend::variantsFor(const QString& code) const
{
    return QStringList{QString()} + m_layouts.variants.value(code);
}

QVariantMap KeyboardMouseBackend::current() const
{
    OptionItem* layout = option(ROLE_LAYOUT);
    OptionItem* variant = option(ROLE_VARIANT);
    OptionItem* options = option(ROLE_OPTIONS);
    QVariantMap out;
    if (!layout || !variant || !options || !panel())
        return out;
    const QStringList codes = panel()->valueText(*layout->bound()).split(QLatin1Char(','));
    const QStringList variants = panel()->valueText(*variant->bound()).split(QLatin1Char(','));
    out.insert(QStringLiteral("first"), codes.value(0).trimmed());
    out.insert(QStringLiteral("firstVariant"), variants.value(0).trimmed());
    out.insert(QStringLiteral("second"), codes.value(1).trimmed());
    out.insert(QStringLiteral("secondVariant"), variants.value(1).trimmed());
    // Riley: the first of SWITCH_KEYS that the options hold, else the first.
    const QStringList opts = panel()->valueText(*options->bound()).split(QLatin1Char(','));
    QString key = QString::fromLatin1(SWITCH_KEYS[0].option);
    for (const SwitchKey& k : SWITCH_KEYS) {
        if (opts.contains(QString::fromLatin1(k.option))) {
            key = QString::fromLatin1(k.option);
            break;
        }
    }
    out.insert(QStringLiteral("switchKey"), key);
    return out;
}

bool KeyboardMouseBackend::chooseLayouts(const QString& first, const QString& firstVariant, const QString& second,
                                         const QString& secondVariant, const QString& switchKey)
{
    OptionItem* layout = option(ROLE_LAYOUT);
    OptionItem* variant = option(ROLE_VARIANT);
    OptionItem* options = option(ROLE_OPTIONS);
    if (!layout || !variant || !options || !panel())
        return refuse(QStringLiteral("Keyboard layout: the page has no options for the layout"));
    if (!m_layouts.codes.contains(first))
        return refuse(QStringLiteral("Keyboard layout: there is no layout \"%1\"").arg(first));
    if (!second.isEmpty() && !m_layouts.codes.contains(second))
        return refuse(QStringLiteral("Keyboard layout: there is no layout \"%1\"").arg(second));
    if (!variantsFor(first).contains(firstVariant))
        return refuse(QStringLiteral("Keyboard layout: %1 has no variant \"%2\"").arg(first, firstVariant));
    if (!variantsFor(second).contains(secondVariant))
        return refuse(second.isEmpty() ? QStringLiteral("Keyboard layout: a variant needs a second layout")
                                       : QStringLiteral("Keyboard layout: %1 has no variant \"%2\"").arg(second, secondVariant));
    if (!second.isEmpty() && !switchKeys().contains(switchKey))
        return refuse(QStringLiteral("Keyboard layout: \"%1\" is not a key that switches layouts").arg(switchKey));

    // Riley's on_layout: your options that are not a switch key stay, in order; then the switch key.
    QStringList others;
    for (const QString& o : panel()->valueText(*options->bound()).split(QLatin1Char(','))) {
        if (!o.isEmpty() && !o.startsWith(QLatin1String("grp:")))
            others.append(o);
    }
    if (!second.isEmpty())
        others.append(switchKey);

    QString variants = firstVariant;
    if (!second.isEmpty()) {
#ifdef RILEY_PLANT_FAULT_SWITCH
        // The M4 gate's planted fault: the first layout gets the second layout's variant.
        variants = secondVariant + QLatin1Char(',') + secondVariant;
#else
        variants = firstVariant + QLatin1Char(',') + secondVariant;
#endif
    }

    Change cl;
    cl.option = layout->bound();
    cl.text = second.isEmpty() ? first : first + QLatin1Char(',') + second;
    Change cv;
    cv.option = variant->bound();
    cv.text = variants;
    Change co;
    co.option = options->bound();
    co.text = others.join(QLatin1Char(','));
    const bool ok = panel()->commitStep({cl, cv, co});
    Q_EMIT changed();
    return ok;
}

}  // namespace riley::panel::pages
