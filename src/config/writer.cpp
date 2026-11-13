#include "writer.h"

#include "atomic_file.h"
#include "writers/writers.h"

#include <vector>

namespace riley::config {
namespace {

// The registry. A new writer: its file in src/config/writers/, a factory in writers/writers.h, a line here and its
// file in CMakeLists.txt.
const std::vector<std::unique_ptr<Writer>>& registry()
{
    static const std::vector<std::unique_ptr<Writer>> writers = [] {
        std::vector<std::unique_ptr<Writer>> w;
        w.push_back(writers::makeHyprlandLua());
        w.push_back(writers::makeHypridle());
        w.push_back(writers::makeHyprpaper());
        w.push_back(writers::makeGreeterSway());
        return w;
    }();
    return writers;
}

}  // namespace

bool Writer::write(const Config& config, const QString& outPath, const QByteArray* existing, QString* error) const
{
    QByteArray text;
    if (!render(config, existing, &text, error))
        return false;
    return writeTargetFile(outPath, text, error);
}

const Writer* findWriter(const QString& target)
{
    for (const auto& w : registry()) {
        if (w->target() == target)
            return w.get();
    }
    return nullptr;
}

QStringList writerTargets()
{
    QStringList out;
    for (const auto& w : registry())
        out.append(w->target());
    return out;
}

}  // namespace riley::config
