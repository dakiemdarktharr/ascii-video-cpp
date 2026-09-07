#pragma once
#include "AsciiConverter.hpp"
#include "MediaInput.hpp"
#include <atomic>
#include <functional>

namespace ascii {
struct Metrics {
    qint64 frames = 0;
    double atlasMs = 0;
    double decodeMs = 0;
    double preprocessMs = 0;
    double renderMs = 0;
    double encodeMs = 0;
    double totalMs = 0;
    int peakResidentFrames = 0;
    double fps() const { return totalMs > 0 ? static_cast<double>(frames) * 1000 / totalMs : 0; }
};
struct Progress {
    QImage preview;
    Metrics metrics;
    qint64 expectedFrames = 0;
    qint64 outputBytes = 0;
};
struct ConversionResult {
    MediaInfo input;
    QString output;
    QImage poster;
    Metrics metrics;
};
using ProgressCallback = std::function<void(const Progress &)>;
class FramePipeline {
  public:
    ConversionResult run(const QString &input, const QString &output, const Settings &settings,
                         std::atomic_bool &stop, const ProgressCallback &progress = {},
                         const QString &codec = "libx264");
};
} // namespace ascii
