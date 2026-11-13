// --check (docs/switch-contract.md, "--check"): what every writer would leave at its targets, compared with the
// files there now. The writers run into a temporary directory; nothing in the home is written and no command runs.
#pragma once

#include "import.h"

#include <QString>

namespace riley::config {

// Prints "<target>\t<same|differs>\t<path>" per target file. Returns 0 when all are the same, 1 when one differs,
// 2 when the description or the values cannot be loaded (one line on stderr, nothing on stdout).
// `outDir` (may be empty): also keep every file the writers would leave, at its path relative to $HOME.
int runCheck(const QString& descriptionPath, const QString& schemaDir, const QString& valuesPath,
             const HomePaths& paths, const QString& outDir);

}  // namespace riley::config
