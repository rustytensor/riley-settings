#include "keyboardmouse.h"

namespace riley::panel::pages {

KeyboardMouseBackend::KeyboardMouseBackend(QObject* parent) : PageBackend(parent)
{
    connect(this, &PageBackend::panelChanged, this, &KeyboardMouseBackend::rebuild);
    connect(this, &PageBackend::pageChanged, this, &KeyboardMouseBackend::rebuild);
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
    Q_EMIT changed();
}

bool KeyboardMouseBackend::refresh()
{
    for (QObject* o : std::as_const(m_options)) {
        if (auto* item = qobject_cast<OptionItem*>(o))
            item->refresh();
    }
    return true;
}

}  // namespace riley::panel::pages
