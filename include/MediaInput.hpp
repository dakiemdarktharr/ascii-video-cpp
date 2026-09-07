#pragma once
#include <QString>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

namespace ascii {
struct MediaInfo {
    QString path;
    bool video = false;
    int width = 0;
    int height = 0;
    double fps = 0;
    qint64 frames = 0;
    double duration = 0;
};
class MediaInput {
  public:
    explicit MediaInput(const QString &path);
    const MediaInfo &info() const { return info_; }
    bool read(cv::Mat &frame);

  private:
    MediaInfo info_;
    cv::VideoCapture capture_;
    cv::Mat first_;
    bool consumed_ = false;
};
} // namespace ascii
