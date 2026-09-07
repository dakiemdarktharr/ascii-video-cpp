#include "MediaInput.hpp"
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>

namespace ascii {
MediaInput::MediaInput(const QString &path) {
    QFileInfo file(path);
    if (!file.isFile() || file.size() == 0)
        throw std::runtime_error("Input does not exist, is not a file, or is empty.");
    info_.path = file.absoluteFilePath();
    const QString suffix = file.suffix().toLower();
    const bool image = QStringList{"png", "jpg", "jpeg", "bmp", "tif", "tiff", "webp"}.contains(suffix);
    if (image) {
        QFile source(path);
        if (!source.open(QIODevice::ReadOnly))
            throw std::runtime_error("Cannot read input image.");
        const auto bytes = source.readAll();
        const std::vector<uchar> data(bytes.begin(), bytes.end());
        first_ = cv::imdecode(data, cv::IMREAD_COLOR);
        if (first_.empty())
            throw std::runtime_error("Image is corrupt or its codec is unavailable.");
        info_.frames = 1;
    } else {
        info_.video = true;
        if (!capture_.open(info_.path.toUtf8().constData(), cv::CAP_ANY))
            throw std::runtime_error("Cannot open video. File may be corrupt or its decoder unavailable.");
        info_.fps = capture_.get(cv::CAP_PROP_FPS);
        if (!std::isfinite(info_.fps) || info_.fps <= 0 || info_.fps > 1000)
            throw std::runtime_error("Video has no usable frame rate.");
        const double count = capture_.get(cv::CAP_PROP_FRAME_COUNT);
        info_.frames = std::isfinite(count) && count > 0 && count < 1e12 ? static_cast<qint64>(count) : 0;
        info_.duration = static_cast<double>(info_.frames) / info_.fps;
        if (!capture_.read(first_) || first_.empty())
            throw std::runtime_error("Video contains no decodable frames.");
    }
    info_.width = first_.cols;
    info_.height = first_.rows;
}
bool MediaInput::read(cv::Mat &frame) {
    if (!consumed_) {
        consumed_ = true;
        frame = std::move(first_);
        return true;
    }
    if (!info_.video)
        return false;
    return capture_.read(frame) && !frame.empty();
}
} // namespace ascii
