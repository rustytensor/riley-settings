#include "writer.h"

#include "atomic_file.h"
#include "writers/writers.h"

#include <QFile>

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
        w.push_back(writers::makeRileyModel());
        w.push_back(writers::makeRileyDock());
        w.push_back(writers::makeRileyDesktop());
        w.push_back(writers::makeHyprlandKeys());
        return w;
    }();
    return writers;
}

// Where each single-file target is written, under $XDG_CONFIG_HOME.
struct TargetFile {
    const char* target;
    const char* file;
};
constexpr TargetFile kTargetFiles[] = {
    {"hypridle", "hypr/idle.conf"},
    {"hyprpaper", "hypr/hyprpaper.conf"},
    {"hyprland-lua", "hypr/settings.lua"},
    {"hyprland-keys", "hypr/keys-user.lua"},   // read by Riley's hypr/keys.lua (riley bf5b0fb)
    // Riley's model: the GTK panel's own file, which riley-greeter-sync.path watches for the login screen.
    {"riley-model", "hypr/settings.json"},
    {"greeter-sway", "riley/greeter-sway.conf"},   // riley-config and the M1 gate only; no option names it
};

}  // namespace

QString targetFileName(const QString& target)
{
    for (const auto& t : kTargetFiles) {
        if (target == QLatin1String(t.target))
            return QString::fromLatin1(t.file);
    }
    return {};
}

bool Writer::write(const Config& config, const QString& outPath, const QByteArray* existing, QString* error) const
{
    if (leavesFileAlone(config))
        return true;
    if (removesFile(config)) {
        QFile file(outPath);
        if (file.exists() && !file.remove()) {
            *error = outPath + QStringLiteral(": ") + file.errorString();
            return false;
        }
        return true;
    }
    QByteArray text;
    if (!render(config, existing, &text, error))
        return false;
    return writeTargetFile(outPath, text, error);
}

bool Writer::writeFiles(const Config&, const WriteContext&, QString* error) const
{
    *error = target() + QStringLiteral(": this writer writes one file, through write()");
    return false;
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
