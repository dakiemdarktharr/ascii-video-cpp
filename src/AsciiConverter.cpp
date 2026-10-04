#include "AsciiConverter.hpp"
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QResource>
#include <QStandardPaths>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

static void initializeFontResources() {
    Q_INIT_RESOURCE(fonts);
}
namespace ascii {
QFont monospaceFont() {
    static const QFont selected = [] {
        initializeFontResources();
        const int bundledId = QFontDatabase::addApplicationFont(":/fonts/mono.ttf");
        const auto bundled = QFontDatabase::applicationFontFamilies(bundledId);
        if (!bundled.isEmpty()) {
            QFont font(bundled.front());
            font.setPixelSize(14);
            return font;
        }
        // Loading the system font explicitly also works with Qt's offscreen platform on Windows.
        for (const auto &folder : QStandardPaths::standardLocations(QStandardPaths::FontsLocation)) {
            const auto path = QDir(folder).filePath("consola.ttf");
            if (QFileInfo::exists(path)) {
                const int id = QFontDatabase::addApplicationFont(path);
                const auto families = QFontDatabase::applicationFontFamilies(id);
                if (!families.isEmpty()) {
                    QFont font(families.front());
                    font.setPixelSize(14);
                    return font;
                }
            }
        }
        const auto available = QFontDatabase::families();
        for (const auto &family : {"DejaVu Sans Mono", "Liberation Mono", "Consolas", "Courier New"}) {
            if (available.contains(QString::fromLatin1(family))) {
                QFont font(QString::fromLatin1(family));
                font.setPixelSize(14);
                return font;
            }
        }
        QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        font.setStyleHint(QFont::Monospace);
        font.setPixelSize(14);
        return font;
    }();
    return selected;
}
void Settings::validate() const {
    if (columns < 8 || columns > 960 || !std::isfinite(brightness) || brightness < -255 || brightness > 255 ||
        !std::isfinite(contrast) || contrast < 0 || contrast > 4 || charset.empty() || charset.size() > 94 ||
        threads < 1 || threads > 32 || previewFps < 1 || previewFps > 60 || queueCapacity < 1 ||
        queueCapacity > 64 || !foreground.isValid())
        throw std::invalid_argument("Invalid conversion settings.");
    for (char character : charset) {
        const auto c = static_cast<unsigned char>(character);
        if (c < 32 || c > 126)
            throw std::invalid_argument("Charset must contain printable ASCII characters only.");
    }
}
int AsciiConverter::rowsFor(int width, int height, int columns) {
    if (width <= 0 || height <= 0 || columns <= 0)
        throw std::invalid_argument("Invalid frame dimensions.");
    const double rows = static_cast<double>(height) / width * columns * glyphWidth / glyphHeight;
    if (rows > 2048)
        throw std::invalid_argument("Frame aspect ratio exceeds the 2048-row safety limit.");
    return std::max(1, static_cast<int>(std::lround(rows)));
}
QSize AsciiConverter::outputSize(int width, int height, const Settings &settings) {
    settings.validate();
    const int cell = settings.fineDetail ? 4 : glyphWidth;
    const QSize size(settings.columns * cell, rowsFor(width, height, settings.columns) * cell * 2);
    if (static_cast<qint64>(size.width()) * size.height() > 33554432)
        throw std::invalid_argument("Output is too large. Choose fewer characters per line.");
    return size;
}
AsciiConverter::AsciiConverter(Settings settings)
    : settings_(std::move(settings)), cellWidth_(settings_.fineDetail ? 4 : glyphWidth),
      cellHeight_(cellWidth_ * 2) {
    settings_.validate();
    for (int i = 0; i < 256; ++i) {
        const double value =
            std::clamp((i - 127.5) * settings_.contrast + 127.5 + settings_.brightness, 0.0, 255.0);
        lut_[static_cast<size_t>(i)] =
            static_cast<unsigned char>(value * static_cast<double>(settings_.charset.size() - 1) / 255.0);
    }
    initializeFontResources();
    // Pre-rendered ASCII cells keep tiny glyphs identical across Qt/platform font engines.
    const QImage atlas(settings_.fineDetail ? ":/fonts/atlas-fine.png" : ":/fonts/atlas-classic.png");
    if (atlas.isNull())
        throw std::runtime_error("The bundled ASCII glyph atlas is unavailable.");
    for (char c : settings_.charset) {
        QImage glyph = atlas.copy((c - 32) * cellWidth_, 0, cellWidth_, cellHeight_)
                           .convertToFormat(QImage::Format_ARGB32);
        atlas_.push_back(std::move(glyph));
    }
}
char AsciiConverter::map(unsigned char gray) const {
    return settings_.charset[lut_[gray]];
}
RenderedFrame AsciiConverter::convert(const cv::Mat &input) {
    using Clock = std::chrono::steady_clock;
    if (input.empty() || input.depth() != CV_8U ||
        (input.channels() != 1 && input.channels() != 3 && input.channels() != 4))
        throw std::invalid_argument("Expected a nonempty 8-bit gray, BGR or BGRA frame.");
    const auto start = Clock::now();
    const int rows = rowsFor(input.cols, input.rows, settings_.columns);
    cv::resize(input, small_, {settings_.columns, rows}, 0, 0, cv::INTER_AREA);
    if (small_.channels() == 1)
        gray_ = small_;
    else
        cv::cvtColor(small_, gray_, small_.channels() == 4 ? cv::COLOR_BGRA2GRAY : cv::COLOR_BGR2GRAY);
    if (settings_.sharpen) {
        cv::Mat blurred;
        cv::GaussianBlur(gray_, blurred, {3, 3}, 0.8);
        cv::addWeighted(gray_, 1.6, blurred, -0.6, 0, gray_);
    }
    const auto prepared = Clock::now();
    RenderedFrame result;
    result.image = QImage(outputSize(input.cols, input.rows, settings_), QImage::Format_RGB888);
    if (result.image.isNull())
        throw std::runtime_error("Cannot allocate rendered image; reduce columns.");
    result.text.reserve(static_cast<size_t>((settings_.columns + 1) * rows));
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < settings_.columns; ++x) {
            const auto index = lut_[gray_.at<unsigned char>(y, x)];
            result.text += settings_.charset[index];
            QColor color = settings_.foreground;
            if (settings_.color && small_.channels() >= 3) {
                const auto *pixel = small_.ptr<unsigned char>(y) + x * small_.channels();
                color = QColor(pixel[2], pixel[1], pixel[0]);
            }
            const auto &glyph = atlas_[index];
            for (int gy = 0; gy < cellHeight_; ++gy) {
                auto *dst = result.image.scanLine(y * cellHeight_ + gy) + x * cellWidth_ * 3;
                const auto *src = reinterpret_cast<const QRgb *>(glyph.constScanLine(gy));
                for (int gx = 0; gx < cellWidth_; ++gx) {
                    const int alpha = qAlpha(src[gx]);
                    dst[gx * 3] = static_cast<uchar>(color.red() * alpha / 255);
                    dst[gx * 3 + 1] = static_cast<uchar>(color.green() * alpha / 255);
                    dst[gx * 3 + 2] = static_cast<uchar>(color.blue() * alpha / 255);
                }
            }
        }
        result.text += '\n';
    }
    result.preprocessMs = std::chrono::duration<double, std::milli>(prepared - start).count();
    result.renderMs = std::chrono::duration<double, std::milli>(Clock::now() - prepared).count();
    return result;
}
std::string AsciiConverter::terminal(const cv::Mat &input, bool ansi) {
    auto result = convert(input);
    if (!ansi)
        return result.text;
    std::string text;
    for (int y = 0; y < gray_.rows; ++y) {
        for (int x = 0; x < gray_.cols; ++x) {
            int r = gray_.at<uchar>(y, x), g = r, b = r;
            if (small_.channels() >= 3) {
                const auto *p = small_.ptr<uchar>(y) + x * small_.channels();
                r = p[2];
                g = p[1];
                b = p[0];
            }
            text +=
                "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
            text += map(gray_.at<uchar>(y, x));
        }
        text += "\033[0m\n";
    }
    return text;
}
} // namespace ascii
