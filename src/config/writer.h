// A writer turns the values into one program's own file (a target). One writer per target, each in
// src/config/writers/<target>.cpp, found by target name through the registry below.
#pragma once

#include "config.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <memory>

namespace riley::config {

// Where a writer that finds its own files (Writer::writesOwnFiles) writes, and what the change was.
struct WriteContext {
    QString home;          // $HOME
    QString configHome;    // $XDG_CONFIG_HOME
    QString cacheHome;     // $XDG_CACHE_HOME
    QStringList changed;   // the keys this change saved (one user step); empty: write everything
    QVariantMap pageData;  // what the page that made the change passes on (for example the screens it knows)
};

class Writer {
public:
    virtual ~Writer() = default;

    // The target name, as settings.xml's target attribute writes it (for example "hypridle").
    virtual QString target() const = 0;

    // True when the writer replaces only its own block of an existing file; then `existing` holds that file (or is
    // null when there is none yet). False when it writes the whole file.
    virtual bool ownBlockOnly() const { return false; }

    // The new text of the target. False with a one-line *error when it cannot be made.
    virtual bool render(const Config& config, const QByteArray* existing, QByteArray* out, QString* error) const = 0;

    // True when, with these values, the writer leaves its file as it is: an existing file is not touched and a
    // missing one is not made (hyprpaper with no wallpaper picked in the panel: the theme decides).
    virtual bool leavesFileAlone(const Config& config) const { Q_UNUSED(config); return false; }

    // True when, with these values, the target file must not exist: write() removes it (hyprland-keys with no key
    // set: a missing keys-user.lua keeps every Riley default).
    virtual bool removesFile(const Config& config) const { Q_UNUSED(config); return false; }

    // Render, then write the text to `outPath` through writeTargetFile (atomic). Not virtual: every writer's output
    // goes through that one function. Writes nothing (and returns true) when leavesFileAlone(); removes `outPath`
    // when removesFile().
    bool write(const Config& config, const QString& outPath, const QByteArray* existing, QString* error) const;

    // True when the writer finds its own files: more than one, outside $XDG_CONFIG_HOME, or only the part of a file
    // that belongs to the keys that changed (riley-dock, riley-desktop). The panel then calls writeFiles instead of
    // write(). Every file it writes still goes through writeTargetFile (atomic, by rename).
    virtual bool writesOwnFiles() const { return false; }
    virtual bool writeFiles(const Config& config, const WriteContext& context, QString* error) const;
    // The files writeFiles may write, absolute, in the order it writes them (for --check).
    virtual QStringList ownFiles(const WriteContext& context) const { Q_UNUSED(context); return {}; }
};

// The file of a single-file target, relative to $XDG_CONFIG_HOME (for example "hypr/idle.conf"), or empty for a
// target that finds its own files or has no writer.
QString targetFileName(const QString& target);

// The writer for `target`, or null when there is none.
const Writer* findWriter(const QString& target);

// Every target that has a writer, in registry order.
QStringList writerTargets();

}  // namespace riley::config
