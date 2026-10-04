#include "MainWindow.hpp"
#include "MediaOutput.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace ascii {
MainWindow::MainWindow() {
    setWindowTitle("ascii-video-cpp");
    resize(1120, 800);
    setAcceptDrops(true);
    QApplication::setFont(monospaceFont());
    setFont(monospaceFont());
    setStyleSheet(
        "QWidget{background:#0b1010;color:#bcc9c1;} QPushButton{padding:9px 20px;border:1px solid #375a46;} "
        "QPushButton:enabled{color:#7df5a2;} QPushButton:disabled{color:#526157;} "
        "QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox{background:#18221c;padding:3px;} "
        "QProgressBar{border:1px solid #375a46;text-align:center;} QProgressBar::chunk{background:#357d51;}");
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);
    auto *title = new QLabel("ASCII / IMAGE + VIDEO");
    title->setStyleSheet("color:#7df5a2;font-size:22px;padding:8px;");
    layout->addWidget(title);
    auto *actions = new QHBoxLayout;
    import_ = new QPushButton("Import");
    import_->setObjectName("importButton");
    convert_ = new QPushButton("Convert");
    convert_->setObjectName("convertButton");
    download_ = new QPushButton("Download");
    download_->setObjectName("downloadButton");
    github_ = new QPushButton("GitHub Profile");
    github_->setObjectName("githubButton");
    for (auto *button : {import_, convert_, download_, github_})
        actions->addWidget(button);
    actions->addStretch();
    stopButton_ = new QPushButton("Stop");
    stopButton_->setObjectName("stopButton");
    actions->addWidget(stopButton_);
    layout->addLayout(actions);
    file_ = new QLabel("Import or drop an image / video.");
    file_->setWordWrap(true);
    layout->addWidget(file_);
    controls_ = new QWidget;
    auto *form = new QFormLayout(controls_);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto *row = new QHBoxLayout;
    columns_ = new QSpinBox;
    columns_->setRange(8, 320);
    columns_->setValue(100);
    threads_ = new QSpinBox;
    threads_->setRange(1, 32);
    threads_->setValue(4);
    previewFps_ = new QSpinBox;
    previewFps_->setRange(1, 60);
    previewFps_->setValue(12);
    row->addWidget(new QLabel("Columns"));
    row->addWidget(columns_);
    row->addWidget(new QLabel("Threads"));
    row->addWidget(threads_);
    row->addWidget(new QLabel("Preview FPS"));
    row->addWidget(previewFps_);
    brightness_ = new QDoubleSpinBox;
    brightness_->setRange(-255, 255);
    contrast_ = new QDoubleSpinBox;
    contrast_->setRange(0, 4);
    contrast_->setSingleStep(0.1);
    contrast_->setValue(1);
    row->addWidget(new QLabel("Brightness"));
    row->addWidget(brightness_);
    row->addWidget(new QLabel("Contrast"));
    row->addWidget(contrast_);
    form->addRow(row);
    auto *style = new QHBoxLayout;
    charset_ = new QLineEdit(" .:-=+*#%@");
    charset_->setMaxLength(94);
    foreground_ = new QComboBox;
    foreground_->addItems({"Green", "White", "Gray"});
    color_ = new QCheckBox("Source colors");
    style->addWidget(new QLabel("Charset"));
    style->addWidget(charset_);
    style->addWidget(new QLabel("Text"));
    style->addWidget(foreground_);
    style->addWidget(color_);
    form->addRow(style);
    layout->addWidget(controls_);
    preview_ = new QLabel("> ready_");
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(320, 200);
    preview_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    preview_->setStyleSheet("background:#000;color:#7df5a2;border:1px solid #243c2d;");
    layout->addWidget(preview_, 1);
    progress_ = new QProgressBar;
    layout->addWidget(progress_);
    status_ = new QLabel("Ready");
    status_->setWordWrap(true);
    status_->setObjectName("statusLabel");
    stats_ = new QLabel("0 FPS | 0.0 s | 0 bytes");
    layout->addWidget(status_);
    layout->addWidget(stats_);
    connect(import_, &QPushButton::clicked, this, [this] {
        const auto path =
            QFileDialog::getOpenFileName(this, "Import media", {},
                                         "Media (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp *.mp4 *.avi "
                                         "*.mov *.mkv *.webm *.gif);;All files (*)");
        if (!path.isEmpty())
            importPath(path);
    });
    connect(convert_, &QPushButton::clicked, this, &MainWindow::startConversion);
    connect(stopButton_, &QPushButton::clicked, this, [this] {
        stop_ = true;
        status_->setText("Stopping...");
    });
    connect(download_, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getSaveFileName(
            this, "Download", result_->input.video ? "ascii-video.mp4" : "ascii-image.png",
            result_->input.video ? "MP4 (*.mp4)" : "PNG (*.png)");
        if (!path.isEmpty())
            downloadTo(path);
    });
    connect(github_, &QPushButton::clicked, this, [this] {
        QDialog dialog(this);
        dialog.setWindowTitle("GitHub Profile export");
        QFormLayout formLayout(&dialog);
        QSpinBox seconds, budget;
        QLineEdit repository;
        seconds.setRange(1, 15);
        seconds.setValue(8);
        budget.setRange(1, 20);
        budget.setValue(5);
        budget.setSuffix(" MiB");
        repository.setPlaceholderText("YOUR_GITHUB_USERNAME/YOUR_REPOSITORY");
        formLayout.addRow("Preview seconds", &seconds);
        formLayout.addRow("Maximum GIF size", &budget);
        formLayout.addRow("Repository URL (optional)", &repository);
        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        formLayout.addRow(&buttons);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted)
            return;
        const auto parent = QFileDialog::getExistingDirectory(this, "Choose parent for github-export");
        if (parent.isEmpty())
            return;
        ExportOptions options;
        options.previewSeconds = seconds.value();
        options.maxGifBytes = static_cast<qint64>(budget.value()) * 1024 * 1024;
        options.repository = repository.text();
        exportTo(parent, options);
    });
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &MainWindow::poll);
    timer_->start(16);
    updateButtons();
}
MainWindow::~MainWindow() {
    stop_ = true;
    if (worker_.joinable())
        worker_.join();
}
QString MainWindow::statusText() const {
    return status_->text();
}
Settings MainWindow::settings() const {
    Settings s;
    s.columns = columns_->value();
    s.threads = threads_->value();
    s.queueCapacity = std::min(16, s.threads * 2);
    s.previewFps = previewFps_->value();
    s.brightness = brightness_->value();
    s.contrast = contrast_->value();
    s.charset = charset_->text().toStdString();
    s.color = color_->isChecked();
    s.foreground = foreground_->currentIndex() == 0   ? QColor(100, 255, 150)
                   : foreground_->currentIndex() == 1 ? QColor(240, 240, 240)
                                                      : QColor(170, 170, 170);
    s.validate();
    return s;
}
void MainWindow::launch(Operation operation, std::function<void()> job) {
    if (busy_)
        return;
    if (worker_.joinable())
        worker_.join();
    operation_ = operation;
    busy_ = true;
    stop_ = false;
    {
        std::lock_guard lock(mutex_);
        mailbox_ = {};
    }
    progress_->setRange(0, 0);
    status_->setText("Working...");
    updateButtons();
    worker_ = std::thread([this, job = std::move(job)] {
        try {
            job();
        } catch (const std::exception &error) {
            std::lock_guard lock(mutex_);
            mailbox_.error = QString::fromUtf8(error.what());
        }
        std::lock_guard lock(mutex_);
        mailbox_.done = true;
    });
}
void MainWindow::importPath(const QString &path) {
    if (busy_)
        return;
    input_.reset();
    result_.reset();
    previewImage_ = {};
    preview_->clear();
    file_->setText(path);
    launch(Operation::Import, [this, path] {
        MediaInput media(path);
        std::lock_guard lock(mutex_);
        mailbox_.imported = media.info();
        mailbox_.message = "Imported. Ready to convert.";
    });
}
void MainWindow::startConversion() {
    if (busy_ || !input_)
        return;
    try {
        const auto config = settings();
        if (!outputDir_.isValid())
            throw std::runtime_error("Temporary output directory is unavailable.");
        const auto path = input_->path;
        const auto output = outputDir_.filePath(input_->video ? "ascii-video.mp4" : "ascii-image.png");
        result_.reset();
        launch(Operation::Convert, [this, path, output, config] {
            auto result = FramePipeline{}.run(path, output, config, stop_, [this](const Progress &update) {
                std::lock_guard lock(mutex_);
                auto merged = update;
                if (merged.preview.isNull() && mailbox_.progress)
                    merged.preview = mailbox_.progress->preview;
                mailbox_.progress = std::move(merged);
            });
            std::lock_guard lock(mutex_);
            mailbox_.result = std::move(result);
            mailbox_.message = "Conversion complete.";
        });
    } catch (const std::exception &error) {
        status_->setText(QString::fromUtf8(error.what()));
    }
}
void MainWindow::downloadTo(const QString &path) {
    if (busy_ || !result_)
        return;
    const auto source = result_->output;
    launch(Operation::Download, [this, source, path] {
        copyOutput(source, path);
        std::lock_guard lock(mutex_);
        mailbox_.message = "Downloaded: " + path;
    });
}
void MainWindow::exportTo(const QString &parent, ExportOptions options) {
    if (busy_ || !result_)
        return;
    const auto result = *result_;
    launch(Operation::Export, [this, result, parent, options] {
        const auto folder = GitHubExporter::create(result, parent, options, stop_);
        std::lock_guard lock(mutex_);
        mailbox_.message = "Exported: " + folder;
    });
}
void MainWindow::poll() {
    Mailbox update;
    {
        std::lock_guard lock(mutex_);
        if (mailbox_.done) {
            update = std::move(mailbox_);
            mailbox_ = {};
        } else {
            update.progress = std::move(mailbox_.progress);
            mailbox_.progress.reset();
        }
    }
    if (update.progress) {
        const auto &p = *update.progress;
        if (!p.preview.isNull())
            showPreview(p.preview);
        if (p.expectedFrames > 0) {
            progress_->setRange(0, 1000);
            progress_->setValue(
                static_cast<int>(std::min(1000.0, static_cast<double>(p.metrics.frames) * 1000 /
                                                      static_cast<double>(p.expectedFrames))));
        }
        stats_->setText(QString("%1 FPS | %2 s elapsed | %3 frames | %4 MiB")
                            .arg(p.metrics.fps(), 0, 'f', 1)
                            .arg(p.metrics.totalMs / 1000, 0, 'f', 1)
                            .arg(p.metrics.frames)
                            .arg(static_cast<double>(p.outputBytes) / 1048576, 0, 'f', 2));
    }
    if (!update.done)
        return;
    if (worker_.joinable())
        worker_.join();
    busy_ = false;
    if (update.imported) {
        input_ = update.imported;
        file_->setText(QString("%1 | %2 | %3 x %4%5")
                           .arg(QFileInfo(input_->path).fileName(), input_->video ? "VIDEO" : "IMAGE")
                           .arg(input_->width)
                           .arg(input_->height)
                           .arg(input_->video ? QString(" | %1 s | %2 FPS")
                                                    .arg(input_->duration, 0, 'f', 2)
                                                    .arg(input_->fps, 0, 'f', 3)
                                              : ""));
    }
    if (update.result) {
        result_ = std::move(update.result);
        showPreview(result_->poster);
    }
    status_->setText(update.error.isEmpty() ? update.message : "Error: " + update.error);
    progress_->setRange(0, 1000);
    progress_->setValue(update.error.isEmpty() ? 1000 : 0);
    updateButtons();
    if (closePending_)
        close();
}
void MainWindow::updateButtons() {
    import_->setEnabled(!busy_);
    convert_->setEnabled(!busy_ && input_.has_value());
    download_->setEnabled(!busy_ && result_.has_value());
    github_->setEnabled(!busy_ && result_.has_value());
    controls_->setEnabled(!busy_);
    stopButton_->setEnabled(busy_ && operation_ != Operation::Download);
}
void MainWindow::showPreview(const QImage &image) {
    previewImage_ = image;
    preview_->setPixmap(
        QPixmap::fromImage(image).scaled(preview_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (!busy_ && event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1 &&
        event->mimeData()->urls().first().isLocalFile())
        event->acceptProposedAction();
}
void MainWindow::dropEvent(QDropEvent *event) {
    if (!busy_ && event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1)
        importPath(event->mimeData()->urls().first().toLocalFile());
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (busy_) {
        closePending_ = true;
        stop_ = true;
        status_->setText("Stopping before closing...");
        event->ignore();
    } else
        event->accept();
}
void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (!previewImage_.isNull())
        showPreview(previewImage_);
}
} // namespace ascii
