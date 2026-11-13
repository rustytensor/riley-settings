// Writing files atomically: the new text goes to a temporary file next to the target, then replaces it by rename,
// so a reader sees the old file or the new one, never half of one.
#pragma once

#include <QByteArray>
#include <QString>

namespace riley::config {

// Write `data` to `path` atomically. False with a one-line *error on failure (the old file is then untouched).
bool writeFileAtomic(const QString& path, const QByteArray& data, QString* error);

// The one way a writer's text reaches its target file. Atomic, as above.
// Built with RILEY_PLANT_FAULT (the CMake option of that name, for the gate), it drops the second line of `data`:
// a fault the writer checks must catch. Off by default.
bool writeTargetFile(const QString& path, const QByteArray& data, QString* error);

// Whether this build carries the planted fault.
bool plantedFault();

}  // namespace riley::config
