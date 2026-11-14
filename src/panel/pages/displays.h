// Displays: drag screens into place, apply with a 15 s revert (docs/pages-contract.md, "Displays").
// A port of Riley's GTK panel's displays.py (riley 2364a47) (Arrangement.on_update, on_end; DisplaysPage.apply, on_keep)
// and core.py (logical_size, apply, restore). The screens are the page's option with role="screens" (a list); the
// page never names its key.
#pragma once

#include "common.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include <optional>

namespace riley::panel::pages {

class DisplaysBackend : public PageBackend {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool pending READ pending NOTIFY changed)
    Q_PROPERTY(int keepSeconds READ keepSeconds NOTIFY changed)
    Q_PROPERTY(int secondsLeft READ secondsLeft NOTIFY changed)
    // Every screen, in list order: {desc, enabled, mode, x, y, scale, transform, bitdepth (texts), w, h (logical
    // size, 0 when the mode or scale cannot be read), modes (live, from hyprctl), depths (the bit depths offered:
    // "8", and "10" when the screen's EDID allows it or it runs 10-bit now), shown (enabled)}.
    Q_PROPERTY(QVariantList screens READ screens NOTIFY changed)
    // The descriptions of the enabled screens, and of every screen: changed only when the set changes, so the
    // page's items are not made again during a drag.
    Q_PROPERTY(QStringList shownDescs READ shownDescs NOTIFY descsChanged)
    Q_PROPERTY(QStringList allDescs READ allDescs NOTIFY descsChanged)
    Q_PROPERTY(QStringList scales READ scales CONSTANT)

public:
    explicit DisplaysBackend(QObject* parent = nullptr);

    bool ready() const override { return true; }

    bool dirty() const { return m_dirty; }
    bool pending() const { return m_pending; }
    int keepSeconds() const { return m_keepSeconds; }
    int secondsLeft() const { return m_secondsLeft; }
    QVariantList screens() const;
    QStringList shownDescs() const { return m_shownDescs; }
    QStringList allDescs() const { return m_allDescs; }
    QStringList scales() const;

    Q_INVOKABLE bool load();
    Q_INVOKABLE bool dragMove(const QString& desc, double dx, double dy);
    Q_INVOKABLE bool dragEnd(const QString& desc);
    Q_INVOKABLE bool setScreenField(const QString& desc, const QString& field, const QString& text);
    Q_INVOKABLE bool reset();
    Q_INVOKABLE bool apply();
    Q_INVOKABLE bool keep();
    Q_INVOKABLE bool revert();
    // The screen's record (as in `screens`), or an empty map.
    Q_INVOKABLE QVariantMap screen(const QString& desc) const;

Q_SIGNALS:
    void changed();
    void descsChanged();

private:
    struct Screen {
        QHash<QString, QString> fields;   // the list option's fields, as text
        QStringList modes;                // live only, never saved
        QStringList depths{QStringLiteral("8")};   // live only, never saved
    };
    struct Remembered {
        QString path;
        std::optional<QByteArray> bytes;   // nullopt: the file did not exist
        bool isLiveConfig = false;         // the hyprland-lua target
    };

    const config::Option* option() const;
    void updateDerived();
    int indexOf(const QString& desc) const;
    bool logicalSize(const Screen& s, int* w, int* h) const;
    QList<config::ItemFields> items() const;
    void tick();
    bool goBack();

    bool m_dirty = false;
    bool m_pending = false;
    int m_keepSeconds = 15;
    int m_secondsLeft = 0;
    QList<Screen> m_list;
    QStringList m_shownDescs;
    QStringList m_allDescs;
    // The drag in progress: the screen and where it was when the drag began.
    QString m_dragDesc;
    int m_dragX = 0;
    int m_dragY = 0;
    QList<Remembered> m_before;
    QTimer m_timer;
    bool m_loaded = false;
};

}  // namespace riley::panel::pages
