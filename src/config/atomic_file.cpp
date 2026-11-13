#include "atomic_file.h"

#include <QSaveFile>

namespace riley::config {

bool writeFileAtomic(const QString& path, const QByteArray& data, QString* error)
{
    QSaveFile f(path);   // a temporary file in the same directory, renamed over `path` on commit
    f.setDirectWriteFallback(false);
    if (!f.open(QIODevice::WriteOnly)) {
        *error = path + QStringLiteral(": ") + f.errorString();
        return false;
    }
    if (f.write(data) != data.size()) {
        *error = path + QStringLiteral(": ") + f.errorString();
        f.cancelWriting();
        return false;
    }
    if (!f.commit()) {
        *error = path + QStringLiteral(": ") + f.errorString();
        return false;
    }
    return true;
}

bool plantedFault()
{
#ifdef RILEY_PLANT_FAULT
    return true;
#else
    return false;
#endif
}

bool writeTargetFile(const QString& path, const QByteArray& data, QString* error)
{
#ifdef RILEY_PLANT_FAULT
    // The gate's planted fault: the second line goes missing.
    QByteArray faulty = data;
    const qsizetype first = faulty.indexOf('\n');
    if (first >= 0) {
        const qsizetype second = faulty.indexOf('\n', first + 1);
        faulty.remove(first + 1, (second < 0 ? faulty.size() : second + 1) - (first + 1));
    }
    return writeFileAtomic(path, faulty, error);
#else
    return writeFileAtomic(path, data, error);
#endif
}

}  // namespace riley::config
