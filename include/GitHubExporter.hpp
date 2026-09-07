#pragma once
#include "FramePipeline.hpp"

namespace ascii {
struct ExportOptions {
    int previewSeconds = 8;
    int previewFps = 10;
    int width = 640;
    qint64 maxGifBytes = 5 * 1024 * 1024;
    QString repository;
};
class GitHubExporter {
  public:
    static QString create(const ConversionResult &result, const QString &parent, const ExportOptions &options,
                          std::atomic_bool &stop);
};
} // namespace ascii
