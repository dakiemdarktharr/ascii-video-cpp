#include "GitHubExporter.hpp"
#include "MediaOutput.hpp"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <stdexcept>

namespace ascii {
QString GitHubExporter::create(const ConversionResult &result, const QString &parent,
                               const ExportOptions &options, std::atomic_bool &stop) {
    if (options.previewSeconds < 1 || options.previewSeconds > 15 || options.previewFps < 1 ||
        options.previewFps > 20 || options.width < 160 || options.width > 960 || options.maxGifBytes < 1024)
        throw std::invalid_argument(
            "Preview must be 1-15 seconds, 1-20 FPS, 160-960 pixels; GIF budget at least 1 KiB.");
    QString repo = options.repository.trimmed();
    repo.remove(QRegularExpression("^https://github\\.com/"));
    if (repo.endsWith('/'))
        repo.chop(1);
    if (repo.endsWith(".git"))
        repo.chop(4);
    if (repo.isEmpty())
        repo = "YOUR_GITHUB_USERNAME/YOUR_REPOSITORY";
    if (!QRegularExpression("^[A-Za-z0-9_-]+/[A-Za-z0-9_.-]+$").match(repo).hasMatch())
        throw std::invalid_argument(
            "Repository must be owner/repository or https://github.com/owner/repository.");
    if (!QFileInfo(result.output).isFile() || result.poster.isNull())
        throw std::runtime_error("Conversion output is unavailable.");
    QDir root(parent);
    const auto target = root.filePath("github-export");
    if (QFileInfo::exists(target))
        throw std::runtime_error(
            "github-export already exists. Choose another parent folder or rename it first.");
    QTemporaryDir temporary(root.filePath(".github-export-XXXXXX"));
    if (!temporary.isValid())
        throw std::runtime_error("Cannot create export folder.");
    const auto base = "https://github.com/" + repo + "/";
    const auto raw = base + "raw/HEAD/github-export/";
    QString snippet = "<!-- Upload the github-export folder at your repository root. -->\n\n";
    auto poster =
        result.poster.scaledToWidth(std::min(options.width, result.poster.width()), Qt::SmoothTransformation);
    if (!result.input.video) {
        saveImage(poster, temporary.filePath("ascii-profile.png"));
        snippet += "![ASCII profile](" + raw + "ascii-profile.png)\n";
    } else {
        saveImage(poster, temporary.filePath("preview.png"));
        copyOutput(result.output, temporary.filePath("ascii-video.mp4"));
        bool fits = false;
        int width = options.width, fps = options.previewFps;
        const auto gif = temporary.filePath("ascii-profile.gif");
        for (int attempt = 0; attempt < 6; ++attempt) {
            const QString filter =
                QString("fps=%1,scale=%2:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=64:stats_mode="
                        "diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3")
                    .arg(fps)
                    .arg(width);
            runFfmpeg({"-t", QString::number(options.previewSeconds), "-i", result.output,
                       "-filter_complex_threads", "1", "-filter_complex", filter, "-loop", "0", gif},
                      stop);
            if (QFileInfo(gif).size() <= options.maxGifBytes) {
                fits = true;
                break;
            }
            width = std::max(160, width * 3 / 4);
            fps = std::max(3, fps - 2);
        }
        if (!fits)
            throw std::runtime_error(
                "GIF cannot fit the configured byte budget. Shorten the preview or increase the budget.");
        snippet += "[![ASCII animation](" + raw + "ascii-profile.gif)](" + base +
                   "blob/HEAD/github-export/ascii-video.mp4)\n\n";
        snippet += "[Poster](" + raw + "preview.png) · [Full MP4](" + base +
                   "blob/HEAD/github-export/ascii-video.mp4)\n";
    }
    if (stop)
        throw std::runtime_error("Stopped.");
    QSaveFile markdown(temporary.filePath("profile-snippet.md"));
    const auto bytes = snippet.toUtf8();
    if (!markdown.open(QIODevice::WriteOnly) || markdown.write(bytes) != bytes.size() || !markdown.commit())
        throw std::runtime_error("Cannot write profile snippet.");
    if (!root.rename(temporary.path(), target))
        throw std::runtime_error("Cannot finalize export directory.");
    temporary.setAutoRemove(false);
    return target;
}
} // namespace ascii
