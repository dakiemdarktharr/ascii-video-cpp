#include "AsciiConverter.hpp"
#include "FramePipeline.hpp"
#include "GitHubExporter.hpp"
#include "MainWindow.hpp"
#include "MediaOutput.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFontInfo>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <opencv2/imgproc.hpp>

class Tests : public QObject {
    Q_OBJECT
    QTemporaryDir files_;
    QString source_, image_;
    std::atomic_bool stop_{false};
    ascii::Settings config() {
        ascii::Settings s;
        s.columns = 24;
        s.fineDetail = false;
        s.sharpen = false;
        s.threads = 4;
        s.queueCapacity = 3;
        return s;
    }
    void makeVideo(const QString &path, int count, double fps = 23.976) {
        ascii::MediaOutput output(path, {96, 64}, fps, stop_);
        for (int i = 0; i < count; ++i) {
            QImage image(96, 64, QImage::Format_RGB888);
            const int level = 10 + (i % 16) * 15;
            image.fill(QColor(level, level, level));
            {
                QPainter painter(&image);
                painter.fillRect((i % 16) * 6, 0, 6, 64, Qt::white);
            }
            output.write(image);
        }
        output.finish();
    }
    ascii::ConversionResult convertVideo(const QString &name) {
        return ascii::FramePipeline{}.run(source_, files_.filePath(name), config(), stop_);
    }
  private slots:
    void initTestCase() {
        cv::setNumThreads(1);
        QVERIFY(files_.isValid());
        image_ = files_.filePath(QString::fromUtf8("source-ảnh.png"));
        QImage image(96, 64, QImage::Format_RGB888);
        image.fill(Qt::white);
        ascii::saveImage(image, image_);
        source_ = files_.filePath("source.mp4");
        makeVideo(source_, 16);
    }
    void monospace() {
        QCOMPARE(ascii::monospaceFont().family(), QString("DejaVu Sans Mono"));
        QVERIFY(QFontInfo(ascii::monospaceFont()).fixedPitch());
    }
    void mapping() {
        ascii::Settings s;
        ascii::AsciiConverter converter(s);
        QCOMPARE(converter.map(0), ' ');
        QCOMPARE(converter.map(255), '@');
        for (int i = 0; i < 256; ++i)
            QCOMPARE(converter.map(static_cast<uchar>(i)), s.charset[static_cast<size_t>(i * 9 / 255)]);
        s.charset = "X";
        QCOMPARE(ascii::AsciiConverter(s).map(200), 'X');
        s.charset = "\n";
        QVERIFY_EXCEPTION_THROWN(ascii::AsciiConverter{s}, std::invalid_argument);
    }
    void brightnessContrast() {
        ascii::Settings s;
        s.brightness = 255;
        QCOMPARE(ascii::AsciiConverter(s).map(0), '@');
        s.brightness = -255;
        QCOMPARE(ascii::AsciiConverter(s).map(255), ' ');
        s.brightness = 0;
        s.contrast = 0;
        ascii::AsciiConverter flat(s);
        QCOMPARE(flat.map(0), flat.map(255));
        s.contrast = 2;
        ascii::AsciiConverter high(s);
        QCOMPARE(high.map(60), ' ');
        QCOMPARE(high.map(200), '@');
    }
    void aspectRatio() {
        QCOMPARE(ascii::AsciiConverter::rowsFor(1920, 1080, 120), 34);
        QCOMPARE(ascii::AsciiConverter::rowsFor(100, 100, 100), 50);
        QCOMPARE(ascii::AsciiConverter::rowsFor(1000, 1, 8), 1);
        QVERIFY_EXCEPTION_THROWN(ascii::AsciiConverter::rowsFor(0, 100, 100), std::invalid_argument);
    }
    void highResolutionText() {
        ascii::Settings fine;
        QCOMPARE(ascii::AsciiConverter::outputSize(1920, 1080, fine), QSize(1920, 1080));
        fine.columns = 960;
        QCOMPARE(ascii::AsciiConverter::outputSize(1920, 1080, fine), QSize(3840, 2160));
        fine.columns = 961;
        QVERIFY_EXCEPTION_THROWN(fine.validate(), std::invalid_argument);
        fine.columns = 480;
        fine.sharpen = false;
        cv::Mat letters(1080, 1920, CV_8UC3, cv::Scalar(0, 0, 0));
        cv::putText(letters, "Readable subtitles 123", {180, 500}, cv::FONT_HERSHEY_SIMPLEX, 1.5,
                    {255, 255, 255}, 3, cv::LINE_AA);
        auto measure = [&](const ascii::Settings &settings) {
            const auto rendered = ascii::AsciiConverter(settings).convert(letters);
            // Recover the spatial envelope of the glyphs, then compare it to subtitle strokes.
            cv::Mat rgb(rendered.image.height(), rendered.image.width(), CV_8UC3,
                        const_cast<uchar *>(rendered.image.constBits()),
                        static_cast<size_t>(rendered.image.bytesPerLine()));
            cv::Mat gray, resized;
            cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);
            cv::resize(gray, resized, letters.size());
            cv::GaussianBlur(resized, resized, {15, 15}, 3);
            cv::Mat mask;
            cv::threshold(resized, mask, 5, 255, cv::THRESH_BINARY);
            cv::Mat original;
            cv::cvtColor(letters, original, cv::COLOR_BGR2GRAY);
            cv::threshold(original, original, 60, 255, cv::THRESH_BINARY);
            cv::Mat intersection, combined;
            cv::bitwise_and(mask, original, intersection);
            cv::bitwise_or(mask, original, combined);
            return static_cast<double>(cv::countNonZero(intersection)) / cv::countNonZero(combined);
        };
        auto old = fine;
        old.columns = 100;
        old.fineDetail = false;
        QVERIFY2(measure(fine) > measure(old) * 1.5,
                 "Fine ASCII must recover subtitle strokes better than the old 100-column default.");
        const auto demo = qEnvironmentVariable("ASCII_TEXT_DEMO_DIR");
        if (!demo.isEmpty()) {
            QDir().mkpath(demo);
            cv::Mat rgb;
            cv::cvtColor(letters, rgb, cv::COLOR_BGR2RGB);
            ascii::saveImage(
                QImage(rgb.data, rgb.cols, rgb.rows, static_cast<qsizetype>(rgb.step), QImage::Format_RGB888),
                demo + "/source.png");
            ascii::saveImage(ascii::AsciiConverter(fine).convert(letters).image, demo + "/fine.png");
            ascii::saveImage(ascii::AsciiConverter(old).convert(letters).image, demo + "/old.png");
        }
    }
    void audioPreservation() {
        const auto input = files_.filePath("sound.mp4");
        ascii::runFfmpeg({"-f", "lavfi", "-i", "color=size=96x64:rate=24:duration=0.5", "-f", "lavfi", "-i",
                          "sine=frequency=440:duration=0.5", "-c:v", "libx264", "-c:a", "aac", "-shortest",
                          input},
                         stop_);
        const auto output = files_.filePath("sound-ascii.mp4");
        ascii::FramePipeline{}.run(input, output, config(), stop_);
        // A required audio map fails if the output has lost sound.
        ascii::runFfmpeg({"-i", output, "-map", "0:a:0", "-f", "null", "-"}, stop_);
        auto muted = config();
        muted.keepAudio = false;
        const auto silent = files_.filePath("silent-ascii.mp4");
        ascii::FramePipeline{}.run(input, silent, muted, stop_);
        QVERIFY_EXCEPTION_THROWN(ascii::runFfmpeg({"-i", silent, "-map", "0:a:0", "-f", "null", "-"}, stop_),
                                 std::runtime_error);
    }
    void terminalColor() {
        cv::Mat frame(16, 16, CV_8UC3, cv::Scalar(0, 0, 255));
        ascii::AsciiConverter converter(config());
        QVERIFY(converter.terminal(frame, true).find("\033[38;2;255;0;0m") != std::string::npos);
        QVERIFY(converter.terminal(frame, false).find('\033') == std::string::npos);
    }
    void imageInputOutputDownload() {
        ascii::MediaInput input(image_);
        QVERIFY(!input.info().video);
        auto result = ascii::FramePipeline{}.run(image_, files_.filePath("image.png"), config(), stop_);
        QCOMPARE(result.metrics.frames, 1);
        QVERIFY(!QImage(result.output).isNull());
        const auto copy = files_.filePath("ascii-image.png");
        ascii::copyOutput(result.output, copy);
        QCOMPARE(QImage(copy), QImage(result.output));
    }
    void invalidInputs() {
        QVERIFY_EXCEPTION_THROWN(ascii::MediaInput(files_.filePath("missing.mp4")), std::runtime_error);
        QFile empty(files_.filePath("empty.mp4"));
        QVERIFY(empty.open(QIODevice::WriteOnly));
        empty.close();
        QVERIFY_EXCEPTION_THROWN(ascii::MediaInput(empty.fileName()), std::runtime_error);
        QFile corrupt(files_.filePath("corrupt.png"));
        QVERIFY(corrupt.open(QIODevice::WriteOnly));
        corrupt.write("invalid");
        corrupt.close();
        QVERIFY_EXCEPTION_THROWN(ascii::MediaInput(corrupt.fileName()), std::runtime_error);
        const auto noFrames = files_.filePath("no-frames.mp4");
        ascii::runFfmpeg(
            {"-f", "lavfi", "-i", "color=size=96x64:rate=24", "-frames:v", "0", "-c:v", "libx264", noFrames},
            stop_);
        QVERIFY(QFileInfo(noFrames).size() > 0);
        QVERIFY_EXCEPTION_THROWN(ascii::MediaInput{noFrames}, std::runtime_error);
    }
    void missingCodec() {
        const auto output = files_.filePath("bad-codec.mp4");
        QVERIFY_EXCEPTION_THROWN(
            ascii::FramePipeline{}.run(source_, output, config(), stop_, {}, "encoder_does_not_exist"),
            std::runtime_error);
        QVERIFY(!QFileInfo::exists(output));
    }
    void videoFpsOrderDownload() {
        const auto result = convertVideo("video.mp4");
        ascii::MediaInput output(result.output), input(source_);
        QVERIFY(std::abs(output.info().fps - input.info().fps) < 0.01);
        QCOMPARE(result.metrics.frames, 16);
        ascii::AsciiConverter converter(config());
        cv::Mat original, actual;
        std::vector<cv::Mat> expectedFrames;
        // Only this 16-frame fixture is retained to identify each lossy output frame by its nearest oracle.
        while (input.read(original)) {
            auto expected = converter.convert(original).image;
            cv::Mat rgb(expected.height(), expected.width(), CV_8UC3, expected.bits(),
                        static_cast<size_t>(expected.bytesPerLine()));
            cv::Mat gray;
            cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);
            expectedFrames.push_back(std::move(gray));
        }
        int count = 0;
        while (output.read(actual)) {
            QVERIFY(count < 16);
            cv::Mat gray;
            cv::cvtColor(actual, gray, cv::COLOR_BGR2GRAY);
            int nearest = -1;
            double best = 1e9;
            for (size_t index = 0; index < expectedFrames.size(); ++index) {
                const double difference =
                    cv::norm(gray, expectedFrames[index], cv::NORM_L1) / static_cast<double>(gray.total());
                if (difference < best) {
                    best = difference;
                    nearest = static_cast<int>(index);
                }
            }
            QCOMPARE(nearest, count);
            QVERIFY2(best < 12, qPrintable(QString("Frame %1 luminance error %2").arg(count).arg(best)));
            ++count;
        }
        QCOMPARE(count, 16);
        ascii::copyOutput(result.output, files_.filePath("ascii-video.mp4"));
        ascii::MediaInput copied(files_.filePath("ascii-video.mp4"));
        QCOMPARE(copied.info().frames, 16);
    }

    void exportImageVideoLimits() {
        auto image =
            ascii::FramePipeline{}.run(image_, files_.filePath("profile-image.png"), config(), stop_);
        QTemporaryDir imageRoot;
        ascii::ExportOptions options;
        auto folder = ascii::GitHubExporter::create(image, imageRoot.path(), options, stop_);
        QVERIFY(!QImage(folder + "/ascii-profile.png").isNull());
        QFile markdown(folder + "/profile-snippet.md");
        QVERIFY(markdown.open(QIODevice::ReadOnly));
        QVERIFY(markdown.readAll().contains("YOUR_GITHUB_USERNAME/YOUR_REPOSITORY"));
        QVERIFY_EXCEPTION_THROWN(ascii::GitHubExporter::create(image, imageRoot.path(), options, stop_),
                                 std::runtime_error);
        auto video = convertVideo("profile-video.mp4");
        QTemporaryDir videoRoot;
        options.previewSeconds = 1;
        options.maxGifBytes = 512 * 1024;
        options.repository = "https://github.com/example/ascii-video-cpp";
        folder = ascii::GitHubExporter::create(video, videoRoot.path(), options, stop_);
        for (const auto &name : {"ascii-profile.gif", "ascii-video.mp4", "preview.png", "profile-snippet.md"})
            QVERIFY(QFileInfo::exists(folder + "/" + name));
        QVERIFY(QFileInfo(folder + "/ascii-profile.gif").size() <= options.maxGifBytes);
        ascii::MediaInput gif(folder + "/ascii-profile.gif");
        QVERIFY(gif.info().duration <= 1.1);
        QFile snippet(folder + "/profile-snippet.md");
        QVERIFY(snippet.open(QIODevice::ReadOnly));
        QVERIFY(snippet.readAll().contains("example/ascii-video-cpp/raw/HEAD"));
        QTemporaryDir smallRoot;
        options.maxGifBytes = 1024;
        QVERIFY_EXCEPTION_THROWN(ascii::GitHubExporter::create(video, smallRoot.path(), options, stop_),
                                 std::runtime_error);
        QVERIFY(!QFileInfo::exists(smallRoot.filePath("github-export")));
    }
    void boundedMemoryAndCancellation() {
        const auto longSource = files_.filePath("long.mp4");
        makeVideo(longSource, 320);
        const auto shortRun = convertVideo("short-bound.mp4");
        const auto longRun =
            ascii::FramePipeline{}.run(longSource, files_.filePath("long-bound.mp4"), config(), stop_);
        QVERIFY(shortRun.metrics.peakResidentFrames <= config().queueCapacity);
        QVERIFY(longRun.metrics.peakResidentFrames <= config().queueCapacity);
        QCOMPARE(longRun.metrics.frames, 320);
        QTemporaryDir exportRoot;
        ascii::ExportOptions options;
        options.previewSeconds = 1;
        auto folder = ascii::GitHubExporter::create(longRun, exportRoot.path(), options, stop_);
        ascii::MediaInput preview(folder + "/ascii-profile.gif");
        QVERIFY(preview.info().duration <= 1.1);
        const auto cancelled = files_.filePath("cancelled.mp4");
        QVERIFY_EXCEPTION_THROWN(ascii::FramePipeline{}.run(longSource, cancelled, config(), stop_,
                                                            [&](const auto &) { stop_ = true; }),
                                 std::runtime_error);
        stop_ = false;
        QVERIFY(!QFileInfo::exists(cancelled));
    }
    void uiWorkflow() {
        ascii::MainWindow window;
        window.show();
        auto *convert = window.findChild<QPushButton *>("convertButton");
        auto *download = window.findChild<QPushButton *>("downloadButton");
        auto *github = window.findChild<QPushButton *>("githubButton");
        QVERIFY(convert && download && github);
        QCOMPARE(convert->text(), QString("Create ASCII video"));
        QVERIFY(!convert->isEnabled());
        QVERIFY(!download->isEnabled());
        auto *resolution = window.findChild<QComboBox *>("resolutionCombo");
        auto *columns = window.findChild<QSpinBox *>("columnsSpin");
        QVERIFY(resolution && columns);
        resolution->setCurrentIndex(2);
        QCOMPARE(columns->value(), 960);
        resolution->setCurrentIndex(0);
        QCOMPARE(columns->value(), 320);
        int ticks = 0;
        QTimer heartbeat;
        connect(&heartbeat, &QTimer::timeout, [&] { ++ticks; });
        heartbeat.start(5);
        for (const auto &input : {image_, source_}) {
            window.importPath(input);
            QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 15000);
            QVERIFY2(convert->isEnabled(), qPrintable(window.statusText()));
            QVERIFY(!window.converted());
            QVERIFY(window.statusText().contains("Preview ready"));
            QTest::mouseClick(convert, Qt::LeftButton);
            QVERIFY(!download->isEnabled());
            QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 30000);
            QVERIFY2(window.converted(), qPrintable(window.statusText()));
            QVERIFY(download->isEnabled());
            QVERIFY(github->isEnabled());
            const auto target = files_.filePath(input == image_ ? "ui.png" : "ui.mp4");
            window.downloadTo(target);
            QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 15000);
            QVERIFY(QFileInfo(target).size() > 0);
            QTemporaryDir exportRoot;
            ascii::ExportOptions options;
            options.previewSeconds = 1;
            window.exportTo(exportRoot.path(), options);
            QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 30000);
            QVERIFY2(QFileInfo::exists(exportRoot.filePath("github-export/profile-snippet.md")),
                     qPrintable(window.statusText()));
        }
        QVERIFY(ticks > 10);
        const auto demo = qEnvironmentVariable("ASCII_TEST_DEMO");
        if (!demo.isEmpty()) {
            window.importPath(demo);
            QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 15000);
            window.startConversion();
            QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 30000);
            QVERIFY2(window.converted(), qPrintable(window.statusText()));
            QVERIFY(window.grab().save("ui-preview.png", "PNG"));
        }
    }
};
QTEST_MAIN(Tests)
#include "tests.moc"
