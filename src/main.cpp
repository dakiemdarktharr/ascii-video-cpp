#include "MainWindow.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <cstdio>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char **argv) {
#ifdef _WIN32
    // GUI launches have no console; terminal launches keep redirected output or attach to the shell.
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN &&
        AttachConsole(ATTACH_PARENT_PROCESS)) {
        (void)freopen("CONOUT$", "w", stdout);
        (void)freopen("CONOUT$", "w", stderr);
    }
#endif
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("ascii-video-cpp");
    QCoreApplication::setApplicationVersion(ASCII_VERSION);
    app.setWindowIcon(QIcon(":/icons/app.png"));
    cv::setNumThreads(1);
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "ASCII image and video converter. Without options, opens the desktop UI.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"terminal", "Print first input frame as ASCII.", "input"});
    parser.addOption({"ansi", "Use ANSI 24-bit color with --terminal."});
    parser.addOption({"columns", "Characters per line (8-960). Overrides --width.", "count"});
    parser.addOption({"convert", "Convert a video or image without opening a window.", "input"});
    parser.addOption({"output", "Output MP4 or PNG path for --convert.", "path"});
    parser.addOption({"width", "Output width: 1280, 1920 or 3840.", "pixels", "1920"});
    parser.addOption({"classic", "Use large 8x16 characters instead of fine 4x8 characters."});
    parser.addOption({"no-sharpen", "Disable edge enhancement."});
    parser.addOption({"mute", "Do not preserve source sound."});
    parser.addOption({"threads", "CPU workers (1-32).", "count", "4"});
    parser.addPositionalArgument("input", "Optional file to import in the desktop app.", "[input]");
    parser.process(app);
    try {
        if (parser.isSet("terminal")) {
#ifdef _WIN32
            const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD mode = 0;
            if (GetConsoleMode(handle, &mode))
                SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
            ascii::Settings settings;
            settings.columns = parser.isSet("columns") ? parser.value("columns").toInt() : 100;
            settings.fineDetail = false;
            ascii::MediaInput input(parser.value("terminal"));
            cv::Mat frame;
            input.read(frame);
            std::cout << ascii::AsciiConverter(settings).terminal(frame, parser.isSet("ansi"));
            return 0;
        }
        if (parser.isSet("convert")) {
            if (!parser.isSet("output"))
                throw std::invalid_argument("--convert requires --output.");
            ascii::Settings settings;
            settings.fineDetail = !parser.isSet("classic");
            const int width = parser.value("width").toInt();
            if (!parser.isSet("columns") && width != 1280 && width != 1920 && width != 3840)
                throw std::invalid_argument("--width must be 1280, 1920 or 3840.");
            settings.columns = parser.isSet("columns") ? parser.value("columns").toInt()
                                                       : width / (settings.fineDetail ? 4 : 8);
            settings.sharpen = !parser.isSet("no-sharpen");
            settings.keepAudio = !parser.isSet("mute");
            settings.threads = parser.value("threads").toInt();
            settings.queueCapacity = std::min(8, settings.threads * 2);
            std::atomic_bool stop{false};
            const auto result =
                ascii::FramePipeline{}.run(parser.value("convert"), parser.value("output"), settings, stop);
            std::cout << "Saved " << result.output.toStdString() << " (" << result.poster.width() << "x"
                      << result.poster.height() << ", " << result.metrics.frames << " frames)\n";
            return 0;
        }
        ascii::MainWindow window;
        window.show();
        if (!parser.positionalArguments().isEmpty())
            window.importPath(parser.positionalArguments().first());
        return app.exec();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
