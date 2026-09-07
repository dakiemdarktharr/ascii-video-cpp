#pragma once
#include <QImage>
#include <QProcess>
#include <QStringList>
#include <atomic>

namespace ascii {
QString ffmpegExecutable();
void runFfmpeg(const QStringList &arguments, std::atomic_bool &stop, int timeoutMs = 120000);
void saveImage(const QImage &image, const QString &path);
void copyOutput(const QString &source, const QString &destination);
class MediaOutput {
  public:
    MediaOutput(const QString &path, QSize size, double fps, std::atomic_bool &stop,
                const QString &codec = "libx264");
    ~MediaOutput();
    void write(const QImage &image);
    void finish();

  private:
    QProcess process_;
    QSize size_;
    std::atomic_bool &stop_;
    QString path_;
    bool finished_ = false;
    void check();
};
} // namespace ascii
