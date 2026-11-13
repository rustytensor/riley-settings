// A writer turns the values into one program's own file (a target). One writer per target, each in
// src/config/writers/<target>.cpp, found by target name through the registry below.
#pragma once

#include "config.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <memory>

namespace riley::config {

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

    // Render, then write the text to `outPath` through writeTargetFile (atomic). Not virtual: every writer's output
    // goes through that one function.
    bool write(const Config& config, const QString& outPath, const QByteArray* existing, QString* error) const;
};

// The writer for `target`, or null when there is none.
const Writer* findWriter(const QString& target);

// Every target that has a writer, in registry order.
QStringList writerTargets();

}  // namespace riley::config
