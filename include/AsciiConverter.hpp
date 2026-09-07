#pragma once
#include <QColor>
#include <QFont>
#include <QImage>
#include <array>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace ascii {
QFont monospaceFont();
struct Settings {
    int columns = 100;
    double brightness = 0.0;
    double contrast = 1.0;
    std::string charset = " .:-=+*#%@";
    QColor foreground{100, 255, 150};
    bool color = false;
    int threads = 4;
    int previewFps = 12;
    int queueCapacity = 8;
    void validate() const;
};
struct RenderedFrame {
    QImage image;
    std::string text;
    double preprocessMs = 0;
    double renderMs = 0;
};
class AsciiConverter {
  public:
    static constexpr int glyphWidth = 8;
    static constexpr int glyphHeight = 16;
    explicit AsciiConverter(Settings settings);
    static int rowsFor(int width, int height, int columns);
    char map(unsigned char gray) const;
    RenderedFrame convert(const cv::Mat &input);
    std::string terminal(const cv::Mat &input, bool ansi);

  private:
    Settings settings_;
    std::array<unsigned char, 256> lut_{};
    std::vector<QImage> atlas_;
    cv::Mat small_, gray_;
};
} // namespace ascii
