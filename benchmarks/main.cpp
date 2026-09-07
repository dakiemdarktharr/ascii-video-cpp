#include "AsciiConverter.hpp"
#include "FramePipeline.hpp"
#include "GitHubExporter.hpp"
#include "MediaOutput.hpp"
#include <QDir>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <opencv2/imgproc.hpp>

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    cv::setNumThreads(1);
    std::atomic_bool stop{false};
    try {
        const auto args = app.arguments();
        if (args.size() == 3 && args[1] == "--demo") {
            QDir assets(args[2]);
            if (!assets.mkpath("."))
                throw std::runtime_error("Cannot create assets folder.");
            QTemporaryDir temporary;
            const auto source = temporary.filePath("pattern.mp4");
            ascii::MediaOutput writer(source, {640, 360}, 24, stop);
            for (int frame = 0; frame < 144; ++frame) {
                cv::Mat pattern(360, 640, CV_8UC3);
                const double time = static_cast<double>(frame) / 24;
                for (int y = 0; y < pattern.rows; ++y) {
                    for (int x = 0; x < pattern.cols; ++x) {
                        const double wave =
                            (std::sin(x * 0.016 + time * 1.5) + std::cos(y * 0.023 - time) + 2) / 4;
                        const auto v = static_cast<uchar>(30 + wave * 155);
                        pattern.at<cv::Vec3b>(y, x) = {static_cast<uchar>(v / 2), v,
                                                       static_cast<uchar>(v / 3)};
                    }
                }
                const auto cx = static_cast<int>(320 + 180 * std::sin(time));
                cv::circle(pattern, {cx, 170}, 80, {220, 255, 230}, -1, cv::LINE_AA);
                cv::circle(pattern, {cx + 22, 150}, 57, {12, 25, 15}, -1, cv::LINE_AA);
                cv::putText(pattern, "ASCII / C++", {90, 320}, cv::FONT_HERSHEY_DUPLEX, 1.9, {235, 255, 240},
                            3, cv::LINE_AA);
                cv::Mat rgb;
                cv::cvtColor(pattern, rgb, cv::COLOR_BGR2RGB);
                QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<qsizetype>(rgb.step),
                             QImage::Format_RGB888);
                if (frame == 0)
                    ascii::saveImage(image, assets.filePath("demo-source.png"));
                writer.write(image);
            }
            writer.finish();
            ascii::Settings settings;
            settings.columns = 80;
            auto result =
                ascii::FramePipeline{}.run(source, assets.filePath("demo-ascii.mp4"), settings, stop);
            ascii::saveImage(result.poster, assets.filePath("preview.png"));
            ascii::ExportOptions options;
            options.previewSeconds = 6;
            options.width = 640;
            const auto exported = ascii::GitHubExporter::create(result, temporary.path(), options, stop);
            ascii::copyOutput(exported + "/ascii-profile.gif", assets.filePath("demo.gif"));
            std::cout << "Generated original deterministic demo: 144 frames at 24 FPS.\n";
            return 0;
        }
        if (args.size() < 3 || args.size() > 5) {
            std::cerr << "Usage: ascii-benchmark INPUT OUTPUT [THREADS] [COLUMNS]\n       ascii-benchmark "
                         "--demo ASSETS_DIR\n";
            return 2;
        }
        ascii::Settings settings;
        if (args.size() >= 4)
            settings.threads = args[3].toInt();
        if (args.size() >= 5)
            settings.columns = args[4].toInt();
        const auto result = ascii::FramePipeline{}.run(args[1], args[2], settings, stop);
        const auto &m = result.metrics;
        const QJsonObject report{{"frames", m.frames},
                                 {"decode_ms", m.decodeMs},
                                 {"atlas_ms", m.atlasMs},
                                 {"grayscale_resize_ms", m.preprocessMs},
                                 {"ascii_render_ms", m.renderMs},
                                 {"encode_ms", m.encodeMs},
                                 {"total_ms", m.totalMs},
                                 {"processing_fps", m.fps()},
                                 {"peak_resident_slots", m.peakResidentFrames},
                                 {"threads", settings.threads},
                                 {"columns", settings.columns},
                                 {"note", "Stage times include accumulated worker work and encoder waits; "
                                          "they overlap and are not additive."}};
        std::cout << QJsonDocument(report).toJson().constData();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
