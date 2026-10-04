#pragma once
#include "GitHubExporter.hpp"
#include <QMainWindow>
#include <QTemporaryDir>
#include <atomic>
#include <mutex>
#include <optional>
#include <thread>

class QLabel;
class QPushButton;
class QProgressBar;
class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QTimer;
class QScrollArea;
namespace ascii {
class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    MainWindow();
    ~MainWindow() override;
    void importPath(const QString &path);
    void startConversion();
    void downloadTo(const QString &path);
    void exportTo(const QString &parent, ExportOptions options);
    bool busy() const { return busy_; }
    bool converted() const { return result_.has_value(); }
    QString statusText() const;

  protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    enum class Operation { Import, Preview, Convert, Download, Export };
    struct Mailbox {
        bool done = false;
        QString error, message;
        std::optional<MediaInfo> imported;
        std::optional<ConversionResult> result;
        std::optional<Progress> progress;
        cv::Mat sourceFrame;
        QImage preview;
    };
    QLabel *preview_, *file_, *status_, *stats_;
    QPushButton *import_, *convert_, *download_, *github_, *stopButton_;
    QProgressBar *progress_;
    QSpinBox *columns_, *threads_, *previewFps_;
    QDoubleSpinBox *brightness_, *contrast_;
    QLineEdit *charset_;
    QComboBox *foreground_;
    QCheckBox *color_;
    QCheckBox *sharpen_, *audio_;
    QComboBox *resolution_, *detail_, *zoom_;
    QLabel *sizeLabel_;
    QScrollArea *previewScroll_;
    QTimer *previewTimer_;
    cv::Mat sourceFrame_;
    bool previewPending_ = false;
    QWidget *controls_;
    QTimer *timer_;
    QTemporaryDir outputDir_;
    std::optional<MediaInfo> input_;
    std::optional<ConversionResult> result_;
    QImage previewImage_;
    std::thread worker_;
    std::atomic_bool stop_{false};
    std::mutex mutex_;
    Mailbox mailbox_;
    bool busy_ = false, closePending_ = false;
    Operation operation_ = Operation::Import;
    void launch(Operation operation, std::function<void()> job);
    void poll();
    void updateButtons();
    void showPreview(const QImage &image);
    Settings settings() const;
    void settingsChanged();
    void refreshPreview();
};
} // namespace ascii
