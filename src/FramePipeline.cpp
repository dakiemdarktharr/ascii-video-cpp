#include "FramePipeline.hpp"
#include "MediaOutput.hpp"
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <thread>

namespace ascii {
namespace {
using Clock = std::chrono::steady_clock;
double ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
enum class State { Free, Queued, Working, Ready };
struct Slot {
    State state = State::Free;
    cv::Mat input;
    RenderedFrame output;
};
} // namespace
ConversionResult FramePipeline::run(const QString &input, const QString &output, const Settings &settings,
                                    std::atomic_bool &stop, const ProgressCallback &progress,
                                    const QString &codec) {
    settings.validate();
    const auto begin = Clock::now();
    if (QFileInfo(input).absoluteFilePath() == QFileInfo(output).absoluteFilePath())
        throw std::runtime_error("Output must differ from input.");
    MediaInput source(input);
    const double initialDecodeMs = ms(begin);
    ConversionResult result;
    result.input = source.info();
    result.output = output;
    QTemporaryDir staging(QFileInfo(output).absolutePath() + "/.ascii-XXXXXX");
    if (!staging.isValid())
        throw std::runtime_error("Cannot create output staging directory.");
    const auto staged = staging.filePath(result.input.video ? "output.mp4" : "output.png");
    const QSize size(settings.columns * AsciiConverter::glyphWidth,
                     AsciiConverter::rowsFor(result.input.width, result.input.height, settings.columns) *
                         AsciiConverter::glyphHeight);
    const auto atlasStart = Clock::now();
    const AsciiConverter prototype(settings);
    result.metrics.atlasMs = ms(atlasStart);
    const auto encoderStart = Clock::now();
    std::unique_ptr<MediaOutput> encoder;
    if (result.input.video)
        encoder = std::make_unique<MediaOutput>(staged, size, result.input.fps, stop, codec);
    result.metrics.encodeMs = ms(encoderStart);
    std::vector<Slot> ring(static_cast<size_t>(settings.queueCapacity));
    std::deque<size_t> jobs;
    std::mutex mutex;
    std::condition_variable changed;
    bool decodingDone = false, aborted = false;
    qint64 decoded = 0;
    int resident = 0;
    double decodeMs = initialDecodeMs;
    std::exception_ptr failure;
    auto fail = [&] {
        std::lock_guard lock(mutex);
        if (!failure)
            failure = std::current_exception();
        aborted = true;
        changed.notify_all();
    };
    auto wait = [&](std::unique_lock<std::mutex> &lock, const auto &predicate) {
        while (!aborted && !stop && !predicate())
            changed.wait_for(lock, std::chrono::milliseconds(50));
        return !aborted && !stop;
    };
    std::vector<std::thread> threads;
    auto join = [&] {
        for (auto &thread : threads)
            if (thread.joinable())
                thread.join();
    };
    try {
        threads.emplace_back([&] {
            try {
                for (qint64 sequence = 0;; ++sequence) {
                    const size_t index = static_cast<size_t>(sequence % settings.queueCapacity);
                    std::unique_lock lock(mutex);
                    if (!wait(lock, [&] { return ring[index].state == State::Free; }))
                        break;
                    lock.unlock();
                    const auto started = Clock::now();
                    cv::Mat frame;
                    const bool read = source.read(frame);
                    const double elapsed = ms(started);
                    lock.lock();
                    decodeMs += elapsed;
                    if (!read)
                        break;
                    ring[index].input = std::move(frame);
                    ring[index].state = State::Queued;
                    jobs.push_back(index);
                    ++decoded;
                    ++resident;
                    result.metrics.peakResidentFrames = std::max(result.metrics.peakResidentFrames, resident);
                    changed.notify_all();
                }
                std::lock_guard lock(mutex);
                decodingDone = true;
                changed.notify_all();
            } catch (...) {
                fail();
            }
        });
        for (int worker = 0; worker < settings.threads; ++worker) {
            threads.emplace_back([&] {
                try {
                    // The unused prototype shares immutable glyph images; scratch Mats start empty.
                    AsciiConverter converter = prototype;
                    while (true) {
                        std::unique_lock lock(mutex);
                        if (!wait(lock, [&] { return !jobs.empty() || decodingDone; }) || jobs.empty())
                            break;
                        const auto index = jobs.front();
                        jobs.pop_front();
                        ring[index].state = State::Working;
                        lock.unlock();
                        auto rendered = converter.convert(ring[index].input);
                        ring[index].input.release();
                        lock.lock();
                        ring[index].output = std::move(rendered);
                        ring[index].state = State::Ready;
                        changed.notify_all();
                    }
                } catch (...) {
                    fail();
                }
            });
        }
        auto lastPreview = begin - std::chrono::seconds(1);
        for (qint64 sequence = 0;; ++sequence) {
            const size_t index = static_cast<size_t>(sequence % settings.queueCapacity);
            std::unique_lock lock(mutex);
            if (!wait(lock, [&] {
                    return ring[index].state == State::Ready || (decodingDone && sequence >= decoded);
                }))
                break;
            if (decodingDone && sequence >= decoded)
                break;
            auto frame = std::move(ring[index].output);
            // Hold the slot until encoding completes so a slow encoder also applies backpressure.
            lock.unlock();
            if (sequence == 0)
                result.poster = frame.image;
            const auto encodeStart = Clock::now();
            if (encoder)
                encoder->write(frame.image);
            else
                saveImage(frame.image, staged);
            result.metrics.encodeMs += ms(encodeStart);
            result.metrics.preprocessMs += frame.preprocessMs;
            result.metrics.renderMs += frame.renderMs;
            ++result.metrics.frames;
            result.metrics.totalMs = ms(begin);
            lock.lock();
            result.metrics.decodeMs = decodeMs;
            const auto snapshot = result.metrics;
            ring[index].state = State::Free;
            --resident;
            changed.notify_all();
            lock.unlock();
            if (progress && ms(lastPreview) >= 1000.0 / settings.previewFps) {
                progress({frame.image, snapshot, result.input.frames, QFileInfo(staged).size()});
                lastPreview = Clock::now();
            }
        }
        join();
        result.metrics.decodeMs = decodeMs;
        if (failure)
            std::rethrow_exception(failure);
        if (stop)
            throw std::runtime_error("Stopped.");
        if (result.metrics.frames == 0)
            throw std::runtime_error("Video contains no decodable frames.");
        // OpenCV cannot distinguish every decoder error from EOF. Detect known premature endings.
        if (result.input.video && result.input.frames > 0 && result.metrics.frames + 1 < result.input.frames)
            throw std::runtime_error(
                "Video ended before its advertised frame count; input may be truncated.");
        const auto finishing = Clock::now();
        if (encoder)
            encoder->finish();
        copyOutput(staged, output);
        result.metrics.encodeMs += ms(finishing);
        result.metrics.totalMs = ms(begin);
        if (progress)
            progress({{}, result.metrics, result.input.frames, QFileInfo(output).size()});
        return result;
    } catch (...) {
        {
            std::lock_guard lock(mutex);
            aborted = true;
            changed.notify_all();
        }
        join();
        throw;
    }
}
} // namespace ascii
