#include "MainWindow.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("ascii-video-cpp");
    QCoreApplication::setApplicationVersion("1.0.0");
    cv::setNumThreads(1);
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "ASCII image and video converter. Without options, opens the desktop UI.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"terminal", "Print first input frame as ASCII.", "input"});
    parser.addOption({"ansi", "Use ANSI 24-bit color with --terminal."});
    parser.addOption({"columns", "ASCII columns (8-320).", "count", "100"});
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
            settings.columns = parser.value("columns").toInt();
            ascii::MediaInput input(parser.value("terminal"));
            cv::Mat frame;
            input.read(frame);
            std::cout << ascii::AsciiConverter(settings).terminal(frame, parser.isSet("ansi"));
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
