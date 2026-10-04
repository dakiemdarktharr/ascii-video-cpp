#include "MainWindow.hpp"
#include "MediaOutput.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace ascii {
MainWindow::MainWindow() {
    setWindowTitle("ASCII Video");
    resize(1180, 800);
    setMinimumSize(760, 600);
    setAcceptDrops(true);
    for (const auto &folder : QStandardPaths::standardLocations(QStandardPaths::FontsLocation)) {
        const auto path = QDir(folder).filePath("segoeui.ttf");
        if (QFileInfo::exists(path))
            QFontDatabase::addApplicationFont(path);
    }
    for (const auto &family : {"Segoe UI", "Noto Sans", "DejaVu Sans"}) {
        if (QFontDatabase::families().contains(QString::fromLatin1(family))) {
            QFont font(QString::fromLatin1(family));
            font.setPixelSize(14);
            QApplication::setFont(font);
            setFont(font);
            break;
        }
    }
    setWindowIcon(QIcon(":/icons/app.png"));
    setStyleSheet(
        "QWidget{background:#edf1f7;color:#202d46;font-size:14px;}"
        "QLabel#heading{font-size:25px;font-weight:600;}"
        "QPushButton{background:#ffffff;padding:10px 16px;border:1px solid #bcc8da;border-radius:6px;}"
        "QPushButton:hover{border-color:#4166af;} QPushButton:focus{border:2px solid #4166af;}"
        "QPushButton:disabled{color:#8b96a7;background:#e5eaf1;}"
        "QPushButton#convertButton:enabled{background:#355aa2;color:white;border-color:#355aa2;}"
        "QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox{background:white;padding:5px;border:1px solid "
        "#bcc8da;border-radius:4px;}"
        "QProgressBar{border:1px solid #bcc8da;border-radius:4px;text-align:center;min-height:20px;}"
        "QProgressBar::chunk{background:#557cbb;}"
        "QGroupBox{border:0;font-weight:600;margin-top:22px;} QGroupBox::title{subcontrol-origin:margin;}"
        "QScrollArea{border:0;} QCheckBox{spacing:7px;}");
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 18, 24, 18);
    layout->setSpacing(12);
    auto *header = new QHBoxLayout;
    auto *mark = new QLabel;
    mark->setPixmap(QPixmap(":/icons/app.png").scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    header->addWidget(mark);
    auto *title = new QLabel("ASCII Video");
    title->setObjectName("heading");
    header->addWidget(title);
    header->addStretch();
    header->addWidget(new QLabel("Turn video into character art"));
    layout->addLayout(header);
    auto *actions = new QHBoxLayout;
    import_ = new QPushButton("Open video or image");
    import_->setObjectName("importButton");
    import_->setShortcut(QKeySequence::Open);
    convert_ = new QPushButton("Create ASCII video");
    convert_->setObjectName("convertButton");
    download_ = new QPushButton("Save video as...");
    download_->setObjectName("downloadButton");
    download_->setShortcut(QKeySequence::SaveAs);
    github_ = new QPushButton("Export for GitHub...");
    github_->setObjectName("githubButton");
    github_->setToolTip("Save a GIF and README snippet to a folder on your computer.");
    for (auto *button : {import_, convert_, download_, github_})
        actions->addWidget(button);
    actions->addStretch();
    stopButton_ = new QPushButton("Cancel");
    stopButton_->setObjectName("stopButton");
    actions->addWidget(stopButton_);
    layout->addLayout(actions);
    file_ = new QLabel("Drop a video here, or choose Open video or image to get started.");
    file_->setWordWrap(true);
    layout->addWidget(file_);

    auto *body = new QHBoxLayout;
    controls_ = new QWidget;
    controls_->setMaximumWidth(340);
    auto *side = new QVBoxLayout(controls_);
    side->setContentsMargins(0, 0, 16, 0);
    auto *settingsTitle = new QLabel("Output settings");
    settingsTitle->setStyleSheet("font-size:18px;font-weight:600;");
    side->addWidget(settingsTitle);
    auto *form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    resolution_ = new QComboBox;
    resolution_->setObjectName("resolutionCombo");
    resolution_->addItem("HD - 1280 pixels wide", 1280);
    resolution_->addItem("Full HD - 1920 pixels wide", 1920);
    resolution_->addItem("4K - 3840 pixels wide", 3840);
    resolution_->addItem("Custom", 0);
    resolution_->setCurrentIndex(1);
    form->addRow("Video width", resolution_);
    detail_ = new QComboBox;
    detail_->setObjectName("detailCombo");
    detail_->addItems({"Fine characters - clearer text", "Large characters - classic ASCII"});
    form->addRow("Character size", detail_);
    sizeLabel_ = new QLabel;
    sizeLabel_->setWordWrap(true);
    form->addRow(sizeLabel_);
    sharpen_ = new QCheckBox("Make edges and text clearer");
    sharpen_->setChecked(true);
    form->addRow(sharpen_);
    audio_ = new QCheckBox("Keep original sound");
    audio_->setChecked(true);
    form->addRow(audio_);
    foreground_ = new QComboBox;
    foreground_->addItems({"Green", "White", "Gray"});
    foreground_->setCurrentIndex(1);
    form->addRow("Character color", foreground_);
    color_ = new QCheckBox("Use colors from the original");
    form->addRow(color_);
    brightness_ = new QDoubleSpinBox;
    brightness_->setRange(-255, 255);
    brightness_->setDecimals(0);
    form->addRow("Brightness", brightness_);
    contrast_ = new QDoubleSpinBox;
    contrast_->setRange(0, 4);
    contrast_->setSingleStep(0.1);
    contrast_->setValue(1);
    form->addRow("Contrast", contrast_);
    side->addLayout(form);
    auto *advanced = new QGroupBox("More settings");
    advanced->setCheckable(true);
    advanced->setChecked(false);
    auto *advancedLayout = new QVBoxLayout(advanced);
    auto *advancedFields = new QWidget;
    auto *advancedForm = new QFormLayout(advancedFields);
    advancedForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    advancedForm->setContentsMargins(0, 0, 0, 0);
    columns_ = new QSpinBox;
    columns_->setObjectName("columnsSpin");
    columns_->setRange(8, 960);
    columns_->setValue(480);
    advancedForm->addRow("Characters per line", columns_);
    charset_ = new QLineEdit(" .:-=+*#%@");
    charset_->setMaxLength(94);
    charset_->setFont(monospaceFont());
    charset_->setToolTip("Printable ASCII, ordered from sparse to dense. Include a space for black areas.");
    advancedForm->addRow("Characters (light to dense)", charset_);
    threads_ = new QSpinBox;
    threads_->setRange(1, 32);
    threads_->setValue(4);
    advancedForm->addRow("CPU workers", threads_);
    previewFps_ = new QSpinBox;
    previewFps_->setRange(1, 60);
    previewFps_->setValue(12);
    previewFps_->setToolTip(
        "Only changes how often the preview updates. Saved video keeps the original frame rate.");
    advancedForm->addRow("Preview updates per second", previewFps_);
    advancedLayout->addWidget(advancedFields);
    advancedFields->hide();
    connect(advanced, &QGroupBox::toggled, advancedFields, &QWidget::setVisible);
    side->addWidget(advanced);
    auto *hint = new QLabel(
        "For small subtitles, try 4K and Fine characters. Tiny or blurred letters may still lose detail.");
    hint->setWordWrap(true);
    side->addWidget(hint);
    side->addStretch();
    auto *settingsScroll = new QScrollArea;
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setMinimumWidth(300);
    settingsScroll->setMaximumWidth(340);
    settingsScroll->setWidget(controls_);
    body->addWidget(settingsScroll);
    auto *previewLayout = new QVBoxLayout;
    auto *previewHeader = new QHBoxLayout;
    previewHeader->addWidget(new QLabel("ASCII preview"));
    previewHeader->addStretch();
    zoom_ = new QComboBox;
    zoom_->addItems({"Fit to window", "Actual size (100%)"});
    previewHeader->addWidget(zoom_);
    previewLayout->addLayout(previewHeader);
    preview_ = new QLabel(
        "Your ASCII preview will appear here.\nAdjust the settings before creating the full video.");
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(320, 200);
    preview_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    preview_->setStyleSheet("background:#101725;color:#b8c7dd;");
    previewScroll_ = new QScrollArea;
    previewScroll_->setWidgetResizable(true);
    previewScroll_->setAlignment(Qt::AlignCenter);
    previewScroll_->setWidget(preview_);
    previewScroll_->setStyleSheet("QScrollArea,QScrollArea>QWidget>QWidget{background:#101725;}");
    previewLayout->addWidget(previewScroll_, 1);
    body->addLayout(previewLayout, 1);
    layout->addLayout(body, 1);
    progress_ = new QProgressBar;
    progress_->setValue(0);
    layout->addWidget(progress_);
    status_ = new QLabel("Open a file to begin.");
    status_->setWordWrap(true);
    status_->setObjectName("statusLabel");
    stats_ = new QLabel;
    layout->addWidget(status_);
    layout->addWidget(stats_);
    connect(import_, &QPushButton::clicked, this, [this] {
        const auto path =
            QFileDialog::getOpenFileName(this, "Open video or image", {},
                                         "Videos and images (*.mp4 *.mov *.mkv *.avi *.webm *.gif *.png "
                                         "*.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;All files (*)");
        if (!path.isEmpty())
            importPath(path);
    });
    connect(convert_, &QPushButton::clicked, this, &MainWindow::startConversion);
    connect(stopButton_, &QPushButton::clicked, this, [this] {
        stop_ = true;
        status_->setText("Cancelling...");
    });
    connect(download_, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getSaveFileName(
            this, "Save ASCII output", result_->input.video ? "ascii-video.mp4" : "ascii-image.png",
            result_->input.video ? "MP4 video (*.mp4)" : "PNG image (*.png)");
        if (!path.isEmpty())
            downloadTo(QFileInfo(path).suffix().isEmpty() ? path + (result_->input.video ? ".mp4" : ".png")
                                                          : path);
    });
    connect(github_, &QPushButton::clicked, this, [this] {
        QDialog dialog(this);
        dialog.setWindowTitle("Export files for GitHub");
        QFormLayout formLayout(&dialog);
        QSpinBox seconds, budget;
        QLineEdit repository;
        seconds.setRange(1, 15);
        seconds.setValue(8);
        budget.setRange(1, 20);
        budget.setValue(5);
        budget.setSuffix(" MiB");
        repository.setPlaceholderText("username/repository");
        formLayout.addRow("GIF length (seconds)", &seconds);
        formLayout.addRow("Maximum GIF file size", &budget);
        formLayout.addRow("GitHub repository (optional)", &repository);
        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons.button(QDialogButtonBox::Ok)->setText("Choose folder...");
        formLayout.addRow(&buttons);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted)
            return;
        const auto parent = QFileDialog::getExistingDirectory(this, "Save GitHub export folder in...");
        if (parent.isEmpty())
            return;
        ExportOptions options;
        options.previewSeconds = seconds.value();
        options.maxGifBytes = static_cast<qint64>(budget.value()) * 1024 * 1024;
        options.repository = repository.text();
        exportTo(parent, options);
    });
    previewTimer_ = new QTimer(this);
    previewTimer_->setSingleShot(true);
    previewTimer_->setInterval(200);
    connect(previewTimer_, &QTimer::timeout, this, &MainWindow::refreshPreview);
    auto updatePreset = [this] {
        const int width = resolution_->currentData().toInt();
        if (width > 0) {
            const QSignalBlocker blocked(columns_);
            columns_->setValue(width / (detail_->currentIndex() == 0 ? 4 : 8));
        }
        settingsChanged();
    };
    connect(resolution_, &QComboBox::currentIndexChanged, this, updatePreset);
    connect(detail_, &QComboBox::currentIndexChanged, this, updatePreset);
    connect(columns_, &QSpinBox::valueChanged, this, [this] {
        const QSignalBlocker blocked(resolution_);
        resolution_->setCurrentIndex(3);
        settingsChanged();
    });
    connect(brightness_, &QDoubleSpinBox::valueChanged, this, &MainWindow::settingsChanged);
    connect(contrast_, &QDoubleSpinBox::valueChanged, this, &MainWindow::settingsChanged);
    connect(charset_, &QLineEdit::textChanged, this, &MainWindow::settingsChanged);
    connect(foreground_, &QComboBox::currentIndexChanged, this, &MainWindow::settingsChanged);
    for (auto *box : {color_, sharpen_, audio_})
        connect(box, &QCheckBox::toggled, this, &MainWindow::settingsChanged);
    connect(zoom_, &QComboBox::currentIndexChanged, this, [this] {
        if (!previewImage_.isNull())
            showPreview(previewImage_);
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
    s.fineDetail = detail_->currentIndex() == 0;
    s.sharpen = sharpen_->isChecked();
    s.keepAudio = audio_->isChecked();
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
void MainWindow::settingsChanged() {
    result_.reset();
    previewPending_ = true;
    updateButtons();
    previewTimer_->start();
}
void MainWindow::refreshPreview() {
    if (busy_ || !input_ || sourceFrame_.empty())
        return;
    previewTimer_->stop();
    previewPending_ = false;
    try {
        const auto config = settings();
        const auto size = AsciiConverter::outputSize(input_->width, input_->height, config);
        sizeLabel_->setText(QString("%1 x %2 pixels\n%3 characters per line")
                                .arg(size.width())
                                .arg(size.height())
                                .arg(config.columns));
        const auto source = sourceFrame_;
        launch(Operation::Preview, [this, source, config] {
            auto rendered = AsciiConverter(config).convert(source);
            std::lock_guard lock(mutex_);
            mailbox_.preview = std::move(rendered.image);
            mailbox_.message = "Preview ready. Create the full video, then save it to keep a copy.";
        });
    } catch (const std::exception &error) {
        status_->setText(QString::fromUtf8(error.what()));
    }
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
    status_->setText(operation == Operation::Preview   ? "Updating preview..."
                     : operation == Operation::Import  ? "Opening file..."
                     : operation == Operation::Convert ? "Creating ASCII output..."
                                                       : "Saving files...");
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
    sourceFrame_.release();
    previewPending_ = false;
    previewTimer_->stop();
    previewImage_ = {};
    preview_->clear();
    file_->setText(path);
    launch(Operation::Import, [this, path] {
        MediaInput media(path);
        cv::Mat frame;
        media.read(frame);
        std::lock_guard lock(mutex_);
        mailbox_.imported = media.info();
        mailbox_.sourceFrame = std::move(frame);
        mailbox_.message = "File opened. Preparing preview...";
    });
}
void MainWindow::startConversion() {
    if (busy_ || !input_)
        return;
    previewTimer_->stop();
    previewPending_ = false;
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
            mailbox_.message = "ASCII output ready. Use Save as to keep it before closing the app.";
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
        mailbox_.message = "Saved: " + path;
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
        sourceFrame_ = std::move(update.sourceFrame);
        file_->setText(QString("%1 | %2 | %3 x %4%5")
                           .arg(QFileInfo(input_->path).fileName(), input_->video ? "VIDEO" : "IMAGE")
                           .arg(input_->width)
                           .arg(input_->height)
                           .arg(input_->video ? QString(" | %1 s | %2 FPS")
                                                    .arg(input_->duration, 0, 'f', 2)
                                                    .arg(input_->fps, 0, 'f', 3)
                                              : ""));
    }
    if (!update.preview.isNull())
        showPreview(update.preview);
    if (update.result) {
        result_ = std::move(update.result);
        showPreview(result_->poster);
    }
    status_->setText(update.error.isEmpty() ? update.message : "Error: " + update.error);
    progress_->setRange(0, 1000);
    progress_->setValue(
        update.error.isEmpty() && operation_ != Operation::Preview && operation_ != Operation::Import ? 1000
                                                                                                      : 0);
    updateButtons();
    if (closePending_)
        close();
    else if (update.imported || previewPending_)
        refreshPreview();
}
void MainWindow::updateButtons() {
    import_->setEnabled(!busy_);
    convert_->setEnabled(!busy_ && input_.has_value());
    download_->setEnabled(!busy_ && result_.has_value());
    github_->setEnabled(!busy_ && result_.has_value());
    controls_->setEnabled(!busy_ || operation_ == Operation::Preview);
    stopButton_->setEnabled(busy_ && operation_ != Operation::Download);
    foreground_->setEnabled(!color_->isChecked());
    audio_->setEnabled(!input_ || input_->video);
    convert_->setText(input_ && !input_->video ? "Create ASCII image" : "Create ASCII video");
    download_->setText(input_ && !input_->video ? "Save image as..." : "Save video as...");
}
void MainWindow::showPreview(const QImage &image) {
    previewImage_ = image;
    if (zoom_->currentIndex() == 1) {
        previewScroll_->setWidgetResizable(false);
        preview_->resize(image.size());
        preview_->setPixmap(QPixmap::fromImage(image));
    } else {
        previewScroll_->setWidgetResizable(true);
        preview_->setMinimumSize(320, 200);
        preview_->setPixmap(QPixmap::fromImage(image).scaled(previewScroll_->viewport()->size(),
                                                             Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
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
