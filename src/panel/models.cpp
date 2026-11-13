#include "models.h"

#include "config/config.h"
#include "config/atomic_file.h"
#include "config/writer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeySequence>
#include <QProcess>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace riley::panel {
namespace {

using config::OptionType;

void printErr(const QString& line)
{
    std::fprintf(stderr, "riley-settings: %s\n", line.toUtf8().constData());
}

// Where each target with a writer is written, under $XDG_CONFIG_HOME. A target not listed here has no file yet.
struct TargetFile {
    const char* target;
    const char* file;
};
constexpr TargetFile kTargetFiles[] = {
    {"hypridle", "hypr/idle.conf"},
    {"hyprpaper", "hypr/hyprpaper.conf"},
    {"hyprland-lua", "hypr/settings.lua"},
    {"greeter-sway", "riley/greeter-sway.conf"},   // riley-greeter-sync installs it for the login screen
};

QString targetFile(const QString& target)
{
    for (const auto& t : kTargetFiles) {
        if (target == QLatin1String(t.target))
            return QString::fromLatin1(t.file);
    }
    return {};
}

// What each action runs with --apply-actions: one fixed argv, never a shell line. An empty argv runs nothing
// (the action happens by itself; see the comment on its line).
struct ActionArgv {
    const char* action;
    const char* argv[4];   // ends at the first null
};
const ActionArgv kActions[] = {
    {"hyprland-reload", {"hyprctl", "reload"}},
    // hypridle never rereads its file: hypr-idle restarts it as a child of Hyprland (Riley's core.restart_idle).
    {"hypridle-restart", {"hyprctl", "dispatch", "hl.dsp.exec_cmd(\"hypr-idle --restart\")"}},
    {"hyprpaper-reload", {"hypr-theme", "--restart-wallpaper"}},
    {"dock-reload", {"hypr-dock", "restart"}},
    {"bar-reload", {"hypr-bar", "restart"}},
    // riley-greeter-sync runs as root from riley-greeter-sync.path when the file it watches changes: nothing to run.
    {"greeter-sync", {}},
    {"theme-fonts", {"hypr-theme", "--fonts"}},
    // hypr-desktop-menu carries the desktop boxes; a new start replaces the old one (by its PID file).
    {"desktop-reload", {"hyprctl", "dispatch", "hl.dsp.exec_cmd(\"hypr-desktop-menu\")"}},
};

const ActionArgv* findAction(const QString& name)
{
    for (const auto& a : kActions) {
        if (name == QLatin1String(a.action))
            return &a;
    }
    return nullptr;
}

QString typeName(OptionType t)
{
    return config::optionTypeName(t);
}

QVariantList choiceList(const QList<config::Choice>& choices)
{
    QVariantList out;
    for (const config::Choice& c : choices) {
        QVariantMap m;
        m.insert(QStringLiteral("value"), c.value);
        m.insert(QStringLiteral("label"), c.label);
        out.append(m);
    }
    return out;
}

}  // namespace

// ---- PageItem -------------------------------------------------------------------------------------------------

QList<QObject*> PageItem::scalarOptions() const
{
    QList<QObject*> out;
    for (QObject* o : m_options) {
        if (static_cast<OptionItem*>(o)->type() != QLatin1String("list"))
            out.append(o);
    }
    return out;
}

OptionItem* PageItem::optionByRole(const QString& role) const
{
    if (role.isEmpty())
        return nullptr;
    for (QObject* o : m_options) {
        auto* item = static_cast<OptionItem*>(o);
        if (item->role() == role)
            return item;
    }
    return nullptr;
}

// ---- OptionItem -----------------------------------------------------------------------------------------------

OptionItem::OptionItem(Controller* controller, const config::Option* shown, const config::Option* bound,
                       QObject* parent)
    : QObject(parent), m_controller(controller), m_shown(shown), m_bound(bound)
{
    refresh();
}

QString OptionItem::type() const
{
    return typeName(m_shown->type);
}

QString OptionItem::defaultText() const
{
    return m_shown->defaultValue.value_or(QString());
}

QVariantList OptionItem::choices() const
{
    return choiceList(m_shown->choices);
}

QVariantList OptionItem::fields() const
{
    QVariantList out;
    for (const config::Field& f : m_shown->fields) {
        QVariantMap m;
        m.insert(QStringLiteral("name"), f.name);
        m.insert(QStringLiteral("type"), typeName(f.type));
        m.insert(QStringLiteral("label"), f.label);
        m.insert(QStringLiteral("help"), f.help);
        m.insert(QStringLiteral("defaultText"), f.defaultValue.value_or(QString()));
        m.insert(QStringLiteral("choices"), choiceList(f.choices));
        out.append(m);
    }
    return out;
}

void OptionItem::refresh()
{
    const QString text = m_controller->valueText(*m_bound);
    const QVariantList items = m_bound->type == OptionType::List ? m_controller->items(*m_bound) : QVariantList();
    if (text == m_valueText && items == m_items)
        return;
    m_valueText = text;
    m_items = items;
    Q_EMIT valueChanged();
}

bool OptionItem::commit(const QString& text)
{
    return m_controller->commit(*m_bound, text);
}

bool OptionItem::commitNumber(double value)
{
    if (!std::isfinite(value))
        return commit(QString::number(value));
    // A slider moves in steps from its minimum; the sum of steps is rounded back onto them.
    if (m_shown->type == OptionType::Slider && m_shown->step && *m_shown->step > 0) {
        const double from = m_shown->min.value_or(0);
        value = from + std::round((value - from) / *m_shown->step) * *m_shown->step;
    }
    QString text = QString::number(value, 'g', 12);
    if (text == QLatin1String("-0"))
        text = QStringLiteral("0");
    return commit(text);
}

bool OptionItem::commitItems(const QVariantList& items)
{
    QList<config::ItemFields> list;
    for (const QVariant& v : items) {
        const QVariantMap m = v.toMap();
        config::ItemFields f;
        for (auto it = m.constBegin(); it != m.constEnd(); ++it)
            f.insert(it.key(), it.value().toString());
        list.append(f);
    }
    return m_controller->commitItems(*m_bound, list);
}

// ---- Controller -----------------------------------------------------------------------------------------------

Controller* Controller::create(const Paths& paths, bool applyActions, QString* error)
{
    std::unique_ptr<Controller> c(new Controller);
    c->m_paths = paths;
    c->m_applyActions = applyActions;
    c->m_description = config::Description::load(paths.description, paths.schemaDir, error);
    if (!c->m_description)
        return nullptr;
    if (!c->reloadValues(error))
        return nullptr;

    // One item per option, in description order.
    const std::vector<config::Option>& options = c->m_description->options();
    std::vector<OptionItem*> byIndex;
    for (size_t i = 0; i < options.size(); ++i) {
        const config::Option* bound = &options[i];
#ifdef RILEY_PLANT_FAULT_PANEL
        // The M2 gate's planted fault: the second option's control is bound to the third option's key.
        if (i == 1 && options.size() > 2)
            bound = &options[2];
#endif
        auto* item = new OptionItem(c.get(), &options[i], bound, c.get());
        byIndex.push_back(item);
        c->m_options.append(item);
    }

    // Pages in menu order (`order`, then file order).
    c->m_sortedPages = c->m_description->pages();
    std::stable_sort(c->m_sortedPages.begin(), c->m_sortedPages.end(),
                     [](const config::Page& a, const config::Page& b) { return a.order < b.order; });
    for (const config::Page& page : c->m_sortedPages) {
        auto* p = new PageItem(page, c.get());
        if (page.widget.isEmpty()) {
            for (const config::Section& section : page.sections) {
                auto* s = new SectionItem(section.title, section.help, p);
                for (int idx : section.options)
                    s->add(byIndex[size_t(idx)]);
                p->add(s);
            }
        } else {   // a widget page's options belong to its own component (M3), which finds them by role
            for (int idx : page.options)
                p->addOption(byIndex[size_t(idx)]);
        }
        c->m_pages.append(p);
    }
    return c.release();
}

bool Controller::reloadValues(QString* error)
{
    std::unique_ptr<config::Values> values;
    if (QFile::exists(m_paths.values))
        values = config::Values::load(m_paths.values, m_paths.schemaDir, error);
    else
        values = config::Values::parse(config::newValuesFileText(), m_paths.values, m_paths.schemaDir, error);
    if (!values)
        return false;
    QStringList unknown;
    QString why;
    if (!values->check(*m_description, &unknown, &why))
        setMessage(why);
    for (const QString& key : unknown)   // kept in the file, never dropped
        printErr(QStringLiteral("unknown key in ") + m_paths.values + QStringLiteral(": ") + key);
    m_values = std::move(values);
    return true;
}

QString Controller::valueText(const config::Option& option) const
{
    if (option.type == OptionType::List)
        return {};
    const config::Config cfg(*m_description, *m_values);
    const QString text = cfg.text(option.key);
    if (option.type == OptionType::Slider || option.type == OptionType::Number) {
        double v = 0;
        if (config::parseNumber(text.trimmed(), &v))
            return config::formatNumber(v);
    }
    return text;
}

QVariantList Controller::items(const config::Option& option) const
{
    QVariantList out;
    const config::Config cfg(*m_description, *m_values);
    for (const config::Item& item : cfg.list(option.key)) {
        QVariantMap m;
        for (const config::Field& f : option.fields)
            m.insert(f.name, item.text(f.name));
        out.append(m);
    }
    return out;
}

bool Controller::commit(const config::Option& option, const QString& text)
{
    if (option.type == OptionType::List) {
        setMessage(option.label + QStringLiteral(": a list is changed item by item"));
        return false;
    }
    Change change;
    change.option = &option;
    change.text = text;
    return commitStep({change});
}

bool Controller::commitItems(const config::Option& option, const QList<config::ItemFields>& items)
{
    if (option.type != OptionType::List) {
        setMessage(option.label + QStringLiteral(": not a list"));
        return false;
    }
    Change change;
    change.option = &option;
    change.items = items;
    return commitStep({change});
}

bool Controller::plantFaultPages()
{
#ifdef RILEY_PLANT_FAULT_PAGES
    return true;
#else
    return false;
#endif
}

OptionItem* Controller::optionItem(const config::Option& option) const
{
    for (OptionItem* o : m_options) {
        if (o->shown() == &option)
            return o;
    }
    return nullptr;
}

namespace {

// Whether `change` would leave the values file's value as it is.
bool unchanged(const Controller& c, const Change& change)
{
    const config::Option& option = *change.option;
    if (option.type == OptionType::List) {
        QVariantList asVariant;
        for (const config::ItemFields& item : change.items) {
            QVariantMap m;
            for (const config::Field& f : option.fields)
                m.insert(f.name, item.contains(f.name) ? item.value(f.name) : f.defaultValue.value_or(QString()));
            asVariant.append(m);
        }
        return asVariant == c.items(option);
    }
    const QString current = config::Config(c.description(), c.values()).text(option.key);
    if (change.text == current)
        return true;
    if (option.type == OptionType::Slider || option.type == OptionType::Number) {
        double a = 0, b = 0;
        if (config::parseNumber(change.text.trimmed(), &a) && config::parseNumber(current.trimmed(), &b) && a == b)
            return true;
    }
    return false;
}

}  // namespace

bool Controller::commitStep(const QList<Change>& changes, const QVariantMap& pageData)
{
    // The same value again is no change: nothing is saved, written or run.
    QList<const Change*> todo;
    for (const Change& change : changes) {
        if (!change.option) {
            setMessage(QStringLiteral("a change without an option"));
            return false;
        }
        if (!unchanged(*this, change))
            todo.append(&change);
    }
    if (todo.isEmpty())
        return true;

    // What the values file holds now, to put it back if a later value of the step is refused.
    QByteArray before;
    bool existed = false;
    {
        QFile f(m_paths.values);
        if (f.open(QIODevice::ReadOnly)) {
            before = f.readAll();
            existed = true;
        }
    }
    QDir().mkpath(QFileInfo(m_paths.values).absolutePath());
    QList<const config::Option*> saved;
    for (const Change* change : std::as_const(todo)) {
        const config::Option& option = *change->option;
        QString error;
        const bool ok = option.type == OptionType::List
            ? config::storeList(m_paths.values, *m_description, m_paths.schemaDir, option.key, change->items, &error)
            : config::storeValue(m_paths.values, *m_description, m_paths.schemaDir, option.key, change->text, &error);
        if (!ok) {
            if (!saved.isEmpty()) {   // put the file back as it was before the step
                QString ignored;
                if (existed)
                    config::writeFileAtomic(m_paths.values, before, &ignored);
                else
                    QFile::remove(m_paths.values);
            }
            setMessage(option.label + QStringLiteral(": ") + error);
            return false;
        }
        saved.append(&option);
    }
    afterSave(saved, pageData);
    return true;
}

void Controller::afterSave(const QList<const config::Option*>& options, const QVariantMap& pageData)
{
    QString error;
    if (!reloadValues(&error)) {
        setMessage(error);
        return;
    }
    setMessage(QString());
    for (OptionItem* o : std::as_const(m_options))
        o->refresh();
    QStringList targets, actions, keys;
    for (const config::Option* option : options) {
        keys.append(option->key);
        for (const QString& t : option->targets) {
            if (!targets.contains(t))
                targets.append(t);
        }
        for (const QString& a : option->actions) {
            if (!actions.contains(a))
                actions.append(a);
        }
    }
    if (runWriters(targets, keys, pageData))
        recordActions(actions);
}

QString Controller::targetPath(const QString& target) const
{
    const QString file = targetFile(target);
    return file.isEmpty() ? QString() : m_paths.configHome + QLatin1Char('/') + file;
}

bool Controller::reloadFromDisk()
{
    QString error;
    if (!reloadValues(&error)) {
        setMessage(error);
        return false;
    }
    for (OptionItem* o : std::as_const(m_options))
        o->refresh();
    return true;
}

bool Controller::runWriters(const QStringList& targets, const QStringList& changedKeys, const QVariantMap& pageData)
{
    const config::Config cfg(*m_description, *m_values);
    bool ok = true;
    for (const QString& target : targets) {
        const config::Writer* writer = config::findWriter(target);
        if (writer && writer->writesOwnFiles()) {
            config::WriteContext context;
            context.home = m_paths.home;
            context.configHome = m_paths.configHome;
            context.cacheHome = m_paths.cacheHome;
            context.changed = changedKeys;
            context.pageData = pageData;
            QString error;
            if (!writer->writeFiles(cfg, context, &error)) {
                setMessage(target + QStringLiteral(": ") + error);
                ok = false;
            }
            continue;
        }
        const QString path = targetPath(target);
        if (!writer || path.isEmpty()) {
            printErr(QStringLiteral("target ") + target + QStringLiteral(" has no writer yet; not written"));
            continue;
        }
        QDir().mkpath(QFileInfo(path).absolutePath());
        QByteArray existing;
        bool haveExisting = false;
        if (writer->ownBlockOnly()) {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly)) {
                existing = f.readAll();
                haveExisting = true;
            }
        }
        QString error;
        if (!writer->write(cfg, path, haveExisting ? &existing : nullptr, &error)) {
            setMessage(target + QStringLiteral(": ") + error);
            ok = false;
        }
    }
    return ok;
}

void Controller::appendLog(const QString& line)
{
    if (m_paths.actionLog.isEmpty())
        return;
    QFile log(m_paths.actionLog);
    if (log.open(QIODevice::WriteOnly | QIODevice::Append))
        log.write(line.toUtf8() + '\n');
    else
        printErr(m_paths.actionLog + QStringLiteral(": ") + log.errorString());
}

void Controller::recordActions(const QStringList& actions)
{
    for (const QString& action : actions) {
        if (action == QLatin1String("none"))
            continue;
        appendLog(action);
        if (!m_applyActions) {
            if (m_paths.actionLog.isEmpty())
                printErr(QStringLiteral("action ") + action + QStringLiteral(" (not run: start with --apply-actions)"));
            continue;
        }
        const ActionArgv* a = findAction(action);
        if (!a) {
            printErr(QStringLiteral("action ") + action + QStringLiteral(" is not known; not run"));
            continue;
        }
        QStringList args;
        for (const char* arg : a->argv) {
            if (!arg)
                break;
            args.append(QString::fromUtf8(arg));
        }
        if (args.isEmpty())
            continue;
        const QString program = args.takeFirst();
        if (!QProcess::startDetached(program, args))
            setMessage(QStringLiteral("could not run ") + program);
    }
}

void Controller::launch(const QString& program)
{
    appendLog(QStringLiteral("launch ") + program);
    if (!m_applyActions) {
        if (m_paths.actionLog.isEmpty())
            printErr(QStringLiteral("launch ") + program + QStringLiteral(" (not run: start with --apply-actions)"));
        return;
    }
    if (!QProcess::startDetached(program, {}))
        setMessage(QStringLiteral("could not run ") + program);
}

void Controller::setMessage(const QString& text)
{
    if (text == m_message)
        return;
    m_message = text;
    Q_EMIT messageChanged();
}

void Controller::clearMessage()
{
    setMessage(QString());
}

QString OptionItem::localPath(const QUrl& url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QUrl OptionItem::urlOf(const QString& path) const
{
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}

QString OptionItem::bindingText(int key, int modifiers) const
{
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_CapsLock:
    case 0:
    case Qt::Key_unknown:
        return {};
    default:
        break;
    }
    QStringList parts;
    if (modifiers & Qt::MetaModifier)
        parts.append(QStringLiteral("SUPER"));
    if (modifiers & Qt::ControlModifier)
        parts.append(QStringLiteral("CTRL"));
    if (modifiers & Qt::AltModifier)
        parts.append(QStringLiteral("ALT"));
    if (modifiers & Qt::ShiftModifier)
        parts.append(QStringLiteral("SHIFT"));
    parts.append(QKeySequence(key).toString(QKeySequence::PortableText).toUpper());
    return parts.join(QLatin1Char('+'));
}

}  // namespace riley::panel
