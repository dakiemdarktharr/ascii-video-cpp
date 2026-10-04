#include "MediaOutput.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>
#include <stdexcept>

namespace ascii {
QString ffmpegExecutable() {
    const auto override = qEnvironmentVariable("ASCII_FFMPEG");
    QString path = override;
    if (path.isEmpty()) {
#ifdef _WIN32
        const auto bundled = QDir(QCoreApplication::applicationDirPath()).filePath("ffmpeg.exe");
#else
        const auto bundled = QDir(QCoreApplication::applicationDirPath()).filePath("ffmpeg");
#endif
        path = QFileInfo(bundled).isFile() ? bundled : QStandardPaths::findExecutable("ffmpeg");
    }
    if (path.isEmpty())
        throw std::runtime_error("FFmpeg not found. Install it and add it to PATH or set ASCII_FFMPEG.");
    return path;
}
void runFfmpeg(const QStringList &arguments, std::atomic_bool &stop, int timeoutMs) {
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(ffmpegExecutable(),
                  {QStringList{"-hide_banner", "-loglevel", "error", "-nostdin", "-y"} + arguments});
    if (!process.waitForStarted(10000))
        throw std::runtime_error("Could not start FFmpeg.");
    QByteArray diagnostic;
    QElapsedTimer timer;
    timer.start();
    while (process.state() != QProcess::NotRunning) {
        process.waitForFinished(50);
        diagnostic = (diagnostic + process.readAll()).right(8192);
        if (stop || timer.elapsed() > timeoutMs) {
            process.kill();
            process.waitForFinished(3000);
            throw std::runtime_error(stop ? "Stopped." : "FFmpeg timed out.");
        }
    }
    diagnostic = (diagnostic + process.readAll()).right(8192);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        throw std::runtime_error("FFmpeg failed: " + diagnostic.toStdString());
}
void saveImage(const QImage &image, const QString &path) {
    QSaveFile file(path);
    if (image.isNull() || !file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit())
        throw std::runtime_error("Cannot save PNG image.");
}
void copyOutput(const QString &source, const QString &destination) {
    QFile input(source);
    QSaveFile output(destination);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
        throw std::runtime_error("Cannot open download source or destination.");
    while (!input.atEnd()) {
        const auto chunk = input.read(1024 * 1024);
        if (chunk.isEmpty() || output.write(chunk) != chunk.size())
            throw std::runtime_error("Download failed while copying data.");
    }
    if (!output.commit())
        throw std::runtime_error("Cannot commit downloaded file.");
}
MediaOutput::MediaOutput(const QString &path, QSize size, double fps, std::atomic_bool &stop,
                         const QString &codec)
    : size_(size), stop_(stop), path_(path) {
    if (size.isEmpty() || !std::isfinite(fps) || fps <= 0)
        throw std::invalid_argument("Invalid encoder size or frame rate.");
    process_.setProcessChannelMode(QProcess::MergedChannels);
    QStringList args{"-hide_banner",
                     "-loglevel",
                     "error",
                     "-nostdin",
                     "-y",
                     "-f",
                     "rawvideo",
                     "-pixel_format",
                     "rgb24",
                     "-video_size",
                     QString("%1x%2").arg(size.width()).arg(size.height()),
                     "-framerate",
                     QString::number(fps, 'g', 15),
                     "-i",
                     "pipe:0",
                     "-an",
                     "-c:v",
                     codec};
    if (codec == "libx264")
        args << "-preset" << "veryfast" << "-crf" << "20";
    args << "-threads" << "2" << "-pix_fmt" << "yuv420p" << "-movflags" << "+faststart" << path;
    process_.start(ffmpegExecutable(), args);
    if (!process_.waitForStarted(10000))
        throw std::runtime_error("Could not start FFmpeg encoder.");
}
MediaOutput::~MediaOutput() {
    if (process_.state() != QProcess::NotRunning) {
        process_.kill();
        process_.waitForFinished(3000);
    }
    if (!finished_)
        QFile::remove(path_);
}
void MediaOutput::check() {
    if (stop_)
        throw std::runtime_error("Stopped.");
    if (process_.state() == QProcess::NotRunning)
        throw std::runtime_error("Encoder stopped; codec unavailable or output failed: " +
                                 process_.readAll().toStdString());
    // Successful encodes are silent at loglevel=error; keep diagnostics bounded on malformed input.
    if (process_.bytesAvailable() > 8192)
        process_.read(process_.bytesAvailable() - 8192);
}
void MediaOutput::write(const QImage &image) {
    if (image.size() != size_ || image.format() != QImage::Format_RGB888)
        throw std::runtime_error("Encoder received inconsistent frame dimensions or format.");
    check();
    QElapsedTimer stalled;
    stalled.start();
    for (int row = 0; row < image.height(); ++row) {
        const auto bytes = static_cast<qint64>(image.width()) * 3;
        if (process_.write(reinterpret_cast<const char *>(image.constScanLine(row)), bytes) != bytes)
            throw std::runtime_error("Cannot write frame to FFmpeg.");
        while (process_.bytesToWrite() > 256 * 1024) {
            if (process_.waitForBytesWritten(50))
                stalled.restart();
            check();
            if (stalled.elapsed() > 30000)
                throw std::runtime_error("FFmpeg encoder stalled.");
        }
    }
}
void MediaOutput::finish() {
    process_.closeWriteChannel();
    QElapsedTimer timer;
    timer.start();
    while (process_.state() != QProcess::NotRunning) {
        process_.waitForFinished(50);
        if (stop_ || timer.elapsed() > 120000)
            throw std::runtime_error(stop_ ? "Stopped." : "Encoder finalization timed out.");
    }
    if (process_.exitStatus() != QProcess::NormalExit || process_.exitCode() != 0)
        throw std::runtime_error("FFmpeg encoding failed: " + process_.readAll().toStdString());
    finished_ = true;
}
} // namespace ascii
