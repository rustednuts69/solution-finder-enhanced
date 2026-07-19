#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QImage>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QRubberBand>
#include <QSaveFile>
#include <QScreen>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QSplitter>
#include <QSysInfo>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QStyleFactory>

#include <array>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <vector>

#include "game_core.h"

namespace {

constexpr int kColumns = 10;
constexpr int kRows = 20;
constexpr int kFumenRows = 24;
constexpr int kFumenBlocks = kColumns * kFumenRows;
constexpr int kVisibleTopRow = 3;
constexpr int kVisibleBottomRow = kVisibleTopRow + kRows;

struct Opener {
    QString id;
    QString openerName;
    QString variationName;
    QString code;
    bool earlyVariantDetection = false;
};

struct OpeningDetectionResult {
    Opener opener;
    double occupancyScore = 0.0;
    double colorScore = 0.0;
    int comparableColorCells = 0;
    bool mirrored = false;
    int pageIndex = 0;
    int rowOffset = 0;

    double overallScore() const {
        return comparableColorCells >= 6 ? occupancyScore * 0.82 + colorScore * 0.18 : occupancyScore;
    }

    QString displayName() const {
        QString name = opener.variationName == "Base" ? opener.openerName : opener.openerName + " - " + opener.variationName;
        if (mirrored) {
            name += " mirrored";
        }
        return name;
    }
};

struct FumenOperation {
    int type = 0;
    int rotation = 0;
    int position = kVisibleTopRow * kColumns + 4;
};

struct DecodedFumen {
    std::vector<std::array<int, kFumenBlocks>> pages;
    std::vector<FumenOperation> operations;
    int pageCount = 0;
};

struct PCScoutTarget {
    int pieces = 0;
    int clearLines = 0;
};

struct SpinScoutChoice {
    QString code;
    int holes = 0;
    int pieces = 0;
    int pillarPenalty = 0;
    int bumpiness = 0;
    qint64 score = 0;
};

struct HeldPlayInput {
    int command = 0;
    int pressOrder = 0;
    qint64 pressedAtMs = 0;
    qint64 nextRepeatAtMs = 0;
    bool repeated = false;
};

struct PlayUndoSnapshot {
    SFTGameState game{};
    int openingCycleStartPieces = 0;
    bool openerDetectionDone = false;
    bool variantDetectionDone = false;
    bool earlyVariantDetection = false;
    QString detectedOpenerName;
    std::optional<bool> detectedOpenerMirrored;
    QString detectionLabel;
};

class DiagnosticLog {
public:
    static DiagnosticLog &instance() {
        static DiagnosticLog log;
        return log;
    }

    void append(const QString &message) {
        const QString trimmed = message.trimmed();
        if (trimmed.isEmpty()) {
            return;
        }
        const QString line = QString("[%1] %2")
                                 .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), trimmed);
        text_ += text_.isEmpty() ? line : "\n" + line;
        refreshViews();
    }

    void appendBlock(const QString &title, const QStringList &lines) {
        QStringList block{title};
        for (const QString &line : lines) {
            block << "  " + line;
        }
        append(block.join('\n'));
    }

    void attach(QPlainTextEdit *view) {
        if (!view) {
            return;
        }
        views_.erase(
            std::remove_if(views_.begin(), views_.end(), [](const QPointer<QPlainTextEdit> &item) {
                return item.isNull();
            }),
            views_.end());
        const auto existing = std::find_if(
            views_.begin(), views_.end(), [view](const QPointer<QPlainTextEdit> &item) {
                return item.data() == view;
            });
        if (existing == views_.end()) {
            views_.push_back(view);
        }
        view->setPlainText(text_);
        view->moveCursor(QTextCursor::End);
    }

    void clear() {
        text_.clear();
        refreshViews();
    }

    const QString &text() const {
        return text_;
    }

private:
    void refreshViews() {
        views_.erase(
            std::remove_if(views_.begin(), views_.end(), [](const QPointer<QPlainTextEdit> &item) {
                return item.isNull();
            }),
            views_.end());
        for (const QPointer<QPlainTextEdit> &view : views_) {
            view->setPlainText(text_);
            view->moveCursor(QTextCursor::End);
        }
    }

    QString text_;
    std::vector<QPointer<QPlainTextEdit>> views_;
};

class OutputLogDialog : public QDialog {
public:
    explicit OutputLogDialog(QWidget *parent = nullptr)
        : QDialog(parent) {
        setWindowTitle("Output Log");
        setMinimumSize(720, 420);
        resize(860, 540);

        auto *layout = new QVBoxLayout(this);
        auto *toolbar = new QHBoxLayout();
        auto *title = new QLabel("Application diagnostics", this);
        title->setObjectName("paneTitle");
        auto *copyButton = new QPushButton("Copy", this);
        auto *clearButton = new QPushButton("Clear", this);
        toolbar->addWidget(title);
        toolbar->addStretch(1);
        toolbar->addWidget(copyButton);
        toolbar->addWidget(clearButton);
        layout->addLayout(toolbar);

        output_ = new QPlainTextEdit(this);
        output_->setReadOnly(true);
        output_->setPlaceholderText("Screenshot, opener detection, fumen, and process diagnostics will appear here.");
        output_->setStyleSheet(
            "font-family: 'Menlo', 'SF Mono', 'DejaVu Sans Mono', monospace; font-size: 13px;");
        layout->addWidget(output_, 1);
        DiagnosticLog::instance().attach(output_);

        connect(copyButton, &QPushButton::clicked, this, []() {
            QApplication::clipboard()->setText(DiagnosticLog::instance().text());
        });
        connect(clearButton, &QPushButton::clicked, this, []() {
            DiagnosticLog::instance().clear();
        });
    }

private:
    QPlainTextEdit *output_ = nullptr;
};

QColor cellColor(int value) {
    switch (value) {
    case 1: return QColor("#18c9d2");
    case 2: return QColor("#f0aa18");
    case 3: return QColor("#efe700");
    case 4: return QColor("#e81c25");
    case 5: return QColor("#bb13d4");
    case 6: return QColor("#2044df");
    case 7: return QColor("#18c93f");
    case 8: return QColor("#9f9fa5");
    default: return QColor("#090909");
    }
}

QString cellName(int value) {
    switch (value) {
    case 1: return "I";
    case 2: return "L";
    case 3: return "O";
    case 4: return "Z";
    case 5: return "T";
    case 6: return "J";
    case 7: return "S";
    case 8: return "Gray";
    default: return "Empty";
    }
}

int mirrorColor(int value) {
    if (value == 2) return 6;
    if (value == 6) return 2;
    if (value == 4) return 7;
    if (value == 7) return 4;
    return value;
}

const std::array<std::array<std::array<QPoint, 4>, 4>, 8> &fumenPieceOffsets() {
    static const std::array<std::array<std::array<QPoint, 4>, 4>, 8> offsets = {{
        {{{QPoint(0, 0), QPoint(0, 0), QPoint(0, 0), QPoint(0, 0)},
          {QPoint(0, 0), QPoint(0, 0), QPoint(0, 0), QPoint(0, 0)},
          {QPoint(0, 0), QPoint(0, 0), QPoint(0, 0), QPoint(0, 0)},
          {QPoint(0, 0), QPoint(0, 0), QPoint(0, 0), QPoint(0, 0)}}},
        {{{QPoint(0, 1), QPoint(1, 1), QPoint(2, 1), QPoint(3, 1)},
          {QPoint(1, 0), QPoint(1, 1), QPoint(1, 2), QPoint(1, 3)},
          {QPoint(0, 1), QPoint(1, 1), QPoint(2, 1), QPoint(3, 1)},
          {QPoint(1, 0), QPoint(1, 1), QPoint(1, 2), QPoint(1, 3)}}},
        {{{QPoint(0, 1), QPoint(1, 1), QPoint(2, 1), QPoint(0, 2)},
          {QPoint(1, 0), QPoint(1, 1), QPoint(1, 2), QPoint(2, 2)},
          {QPoint(2, 0), QPoint(0, 1), QPoint(1, 1), QPoint(2, 1)},
          {QPoint(0, 0), QPoint(1, 0), QPoint(1, 1), QPoint(1, 2)}}},
        {{{QPoint(1, 1), QPoint(2, 1), QPoint(1, 2), QPoint(2, 2)},
          {QPoint(1, 1), QPoint(2, 1), QPoint(1, 2), QPoint(2, 2)},
          {QPoint(1, 1), QPoint(2, 1), QPoint(1, 2), QPoint(2, 2)},
          {QPoint(1, 1), QPoint(2, 1), QPoint(1, 2), QPoint(2, 2)}}},
        {{{QPoint(0, 1), QPoint(1, 1), QPoint(1, 2), QPoint(2, 2)},
          {QPoint(2, 0), QPoint(1, 1), QPoint(2, 1), QPoint(1, 2)},
          {QPoint(0, 1), QPoint(1, 1), QPoint(1, 2), QPoint(2, 2)},
          {QPoint(2, 0), QPoint(1, 1), QPoint(2, 1), QPoint(1, 2)}}},
        {{{QPoint(0, 1), QPoint(1, 1), QPoint(2, 1), QPoint(1, 2)},
          {QPoint(1, 0), QPoint(1, 1), QPoint(2, 1), QPoint(1, 2)},
          {QPoint(1, 0), QPoint(0, 1), QPoint(1, 1), QPoint(2, 1)},
          {QPoint(1, 0), QPoint(0, 1), QPoint(1, 1), QPoint(1, 2)}}},
        {{{QPoint(0, 1), QPoint(1, 1), QPoint(2, 1), QPoint(2, 2)},
          {QPoint(1, 0), QPoint(2, 0), QPoint(1, 1), QPoint(1, 2)},
          {QPoint(0, 0), QPoint(0, 1), QPoint(1, 1), QPoint(2, 1)},
          {QPoint(1, 0), QPoint(1, 1), QPoint(0, 2), QPoint(1, 2)}}},
        {{{QPoint(1, 1), QPoint(2, 1), QPoint(0, 2), QPoint(1, 2)},
          {QPoint(1, 0), QPoint(1, 1), QPoint(2, 1), QPoint(2, 2)},
          {QPoint(1, 1), QPoint(2, 1), QPoint(0, 2), QPoint(1, 2)},
          {QPoint(1, 0), QPoint(1, 1), QPoint(2, 1), QPoint(2, 2)}}}
    }};
    return offsets;
}

std::vector<int> fumenOperationCells(const FumenOperation &operation) {
    std::vector<int> cells;
    if (operation.type <= 0 || operation.type >= 8) {
        return cells;
    }
    const int originX = operation.position % kColumns;
    const int originY = operation.position / kColumns;
    const auto &offsets = fumenPieceOffsets()[operation.type][operation.rotation % 4];
    for (const QPoint &offset : offsets) {
        const int x = originX + offset.x() - 1;
        const int y = originY + offset.y() - 1;
        if (0 <= x && x < kColumns && 0 <= y && y < kFumenRows - 1) {
            cells.push_back(y * kColumns + x);
        }
    }
    return cells;
}

QString findRepoRoot(QString start) {
    QDir dir(start);
    while (true) {
        if (QFileInfo::exists(dir.filePath("shared/openers.json")) &&
            QFileInfo::exists(dir.filePath("solution-finder-1.43/sfinder.jar"))) {
            return dir.absolutePath();
        }
        const QString installedRoot = QDir::cleanPath(dir.filePath("../share/solution-finder-enhanced"));
        if (QFileInfo::exists(installedRoot + "/shared/openers.json") &&
            QFileInfo::exists(installedRoot + "/solution-finder-1.43/sfinder.jar")) {
            return installedRoot;
        }
        if (!dir.cdUp()) {
            return QDir::currentPath();
        }
    }
}

QString appDataDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.local/share/solution-finder-enhanced";
    }
    QDir().mkpath(base);
    return base;
}

QString editableOpenerDatabasePath() {
    return QDir(appDataDir()).filePath("openers.json");
}

QString openerDatabasePath(const QString &repoRoot) {
    const QString editable = editableOpenerDatabasePath();
    if (QFileInfo::exists(editable)) {
        return editable;
    }
    return QDir(repoRoot).filePath("shared/openers.json");
}

double hueDistance(double lhs, double rhs) {
    const double distance = std::fmod(std::abs(lhs - rhs), 360.0);
    return std::min(distance, 360.0 - distance);
}

double colorHue(double red, double green, double blue, double maximum, double chroma) {
    if (chroma <= 0.0) {
        return 0.0;
    }
    double hue = 0.0;
    if (maximum == red) {
        hue = 60.0 * std::fmod((green - blue) / chroma, 6.0);
    } else if (maximum == green) {
        hue = 60.0 * ((blue - red) / chroma + 2.0);
    } else {
        hue = 60.0 * ((red - green) / chroma + 4.0);
    }
    return hue < 0.0 ? hue + 360.0 : hue;
}

bool isRecognizedGray(double red, double green, double blue, double brightness, double chroma, double saturation) {
    if (brightness <= 0.32 || brightness >= 0.82 || saturation >= 0.12 || chroma >= 0.09) {
        return false;
    }
    const double average = (red + green + blue) / 3.0;
    return std::max({std::abs(red - average), std::abs(green - average), std::abs(blue - average)}) < 0.045;
}

int classifiedFumenValue(double red, double green, double blue, double alpha) {
    if (alpha <= 0.2) {
        return 0;
    }
    const double maximum = std::max({red, green, blue});
    const double minimum = std::min({red, green, blue});
    const double brightness = maximum;
    const double chroma = maximum - minimum;
    const double saturation = maximum == 0.0 ? 0.0 : chroma / maximum;
    if (brightness < 0.14) {
        return 0;
    }

    const double hue = colorHue(red, green, blue, maximum, chroma);
    if (saturation >= 0.26 && chroma >= 0.11) {
        const std::vector<std::pair<int, double>> pieceHues = {
            {1, 186.0}, {2, 34.0}, {3, 58.0}, {4, 358.0}, {5, 292.0}, {6, 232.0}, {7, 124.0}
        };
        int bestValue = 0;
        double bestScore = 360.0;
        for (const auto &[value, pieceHue] : pieceHues) {
            const double score = hueDistance(hue, pieceHue);
            if (score < bestScore) {
                bestScore = score;
                bestValue = value;
            }
        }
        if (bestScore <= 42.0 && brightness > 0.20) {
            return bestValue;
        }
    }

    if (isRecognizedGray(red, green, blue, brightness, chroma, saturation)) {
        return 8;
    }
    if (saturation < 0.30 || chroma < 0.13) {
        return 0;
    }
    if (hue >= 165.0 && hue <= 205.0 && blue > red * 1.35) {
        return 1;
    }
    if (green == maximum && blue < green * 0.82) {
        if (hue >= 72.0 && hue < 158.0) {
            return 7;
        }
        if (hue >= 45.0 && hue < 72.0 && red > green * 0.74) {
            return 3;
        }
    }
    return 0;
}

int sampledFumenValue(const QImage &image, int centerX, int centerY, int radius) {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    double alpha = 0.0;
    double count = 0.0;
    for (int y = std::max(0, centerY - radius); y <= std::min(image.height() - 1, centerY + radius); ++y) {
        for (int x = std::max(0, centerX - radius); x <= std::min(image.width() - 1, centerX + radius); ++x) {
            const QColor color = image.pixelColor(x, y).convertTo(QColor::Rgb);
            red += color.redF();
            green += color.greenF();
            blue += color.blueF();
            alpha += color.alphaF();
            count += 1.0;
        }
    }
    if (count <= 0.0) {
        return 0;
    }
    return classifiedFumenValue(red / count, green / count, blue / count, alpha / count);
}

std::optional<std::array<int, kFumenBlocks>> fumenCellsFromBoardImage(const QImage &source, bool preservingColors) {
    const QImage image = source.convertToFormat(QImage::Format_RGBA8888);
    if (image.width() < 10 || image.height() < 10) {
        return std::nullopt;
    }
    const int inferredRows = std::min(kRows, std::max(1, static_cast<int>(std::round((static_cast<double>(image.height()) / image.width()) * 10.0))));
    const int targetTopRow = kVisibleBottomRow - inferredRows;
    const int sampleRadius = std::max(1, std::min(image.width() / 120, image.height() / std::max(inferredRows * 12, 1)));
    std::array<int, kFumenBlocks> cells{};
    cells.fill(0);
    for (int sourceRow = 0; sourceRow < inferredRows; ++sourceRow) {
        for (int column = 0; column < kColumns; ++column) {
            const int centerX = static_cast<int>((column + 0.5) * image.width() / 10.0);
            const int centerY = static_cast<int>((sourceRow + 0.5) * image.height() / static_cast<double>(inferredRows));
            const int value = sampledFumenValue(image, centerX, centerY, sampleRadius);
            cells[(targetTopRow + sourceRow) * kColumns + column] = preservingColors || value == 0 ? value : 8;
        }
    }
    return cells;
}

std::optional<QImage> runScreenshotCommand(const QString &program,
                                           const QStringList &arguments,
                                           const QString &path,
                                           QWidget *parent,
                                           QString *errorMessage = nullptr) {
    QProcess process(parent);
    process.start(program, arguments);
    if (!process.waitForStarted(3000)) {
        if (errorMessage) {
            *errorMessage = "could not start";
        }
        DiagnosticLog::instance().append("Screenshot tool could not start: " + program);
        return std::nullopt;
    }
    while (!process.waitForFinished(100)) {
        QApplication::processEvents(QEventLoop::AllEvents, 100);
    }
    QFileInfo file(path);
    if (!file.exists() || file.size() <= 0) {
        if (errorMessage) {
            const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *errorMessage = QString("exit %1%2")
                                .arg(process.exitCode())
                                .arg(stderrText.isEmpty() ? "" : ": " + stderrText);
        }
        DiagnosticLog::instance().append(
            QString("Screenshot tool returned no image: %1 (exit %2)")
                .arg(program)
                .arg(process.exitCode()));
        QFile::remove(path);
        return std::nullopt;
    }
    QImage image(path);
    QFile::remove(path);
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = "screenshot file was not a readable image";
        }
        DiagnosticLog::instance().append("Screenshot tool produced an unreadable image: " + program);
        return std::nullopt;
    }
    DiagnosticLog::instance().append(
        QString("Screenshot captured with %1 (%2x%3)")
            .arg(program)
            .arg(image.width())
            .arg(image.height()));
    return image;
}

QString findProgram(const QString &name) {
    const QString fromPath = QStandardPaths::findExecutable(name);
    if (!fromPath.isEmpty()) {
        return fromPath;
    }
    const QStringList dirs = {
        "/usr/bin",
        "/bin",
        "/usr/local/bin",
        "/snap/bin",
        "/var/lib/flatpak/exports/bin",
        QDir::homePath() + "/.local/bin"
    };
    for (const QString &dir : dirs) {
        const QString candidate = QDir(dir).filePath(name);
        if (QFileInfo(candidate).isExecutable()) {
            return candidate;
        }
    }
    return QString();
}

class ScreenshotSelectionDialog : public QDialog {
public:
    ScreenshotSelectionDialog(const QPixmap &desktop, const QRect &screenGeometry, QWidget *parent = nullptr)
        : QDialog(parent), desktop_(desktop), screenGeometry_(screenGeometry), rubberBand_(QRubberBand::Rectangle, this) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_DeleteOnClose, false);
        setCursor(Qt::CrossCursor);
        setGeometry(screenGeometry_);
    }

    std::optional<QImage> selectedImage() const {
        if (!selection_.isValid() || selection_.width() < 4 || selection_.height() < 4) {
            return std::nullopt;
        }
        const QImage source = desktop_.toImage();
        const double scaleX = static_cast<double>(source.width()) / qMax(1, width());
        const double scaleY = static_cast<double>(source.height()) / qMax(1, height());
        const QRect sourceRect(qRound(selection_.x() * scaleX),
                               qRound(selection_.y() * scaleY),
                               qRound(selection_.width() * scaleX),
                               qRound(selection_.height() * scaleY));
        return source.copy(sourceRect.intersected(source.rect()));
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.drawPixmap(rect(), desktop_);
        painter.fillRect(rect(), QColor(0, 0, 0, 72));
        if (selection_.isValid()) {
            painter.save();
            painter.setClipRect(selection_);
            painter.drawPixmap(rect(), desktop_);
            painter.restore();
        }
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) {
            return;
        }
        origin_ = event->position().toPoint();
        selection_ = QRect(origin_, QSize());
        rubberBand_.setGeometry(selection_);
        rubberBand_.show();
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (!rubberBand_.isVisible()) {
            return;
        }
        selection_ = QRect(origin_, event->position().toPoint()).normalized().intersected(rect());
        rubberBand_.setGeometry(selection_);
        update();
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton || !rubberBand_.isVisible()) {
            return;
        }
        selection_ = QRect(origin_, event->position().toPoint()).normalized().intersected(rect());
        selection_.width() >= 4 && selection_.height() >= 4 ? accept() : reject();
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape) {
            reject();
            return;
        }
        QDialog::keyPressEvent(event);
    }

private:
    QPixmap desktop_;
    QRect screenGeometry_;
    QRubberBand rubberBand_;
    QPoint origin_;
    QRect selection_;
};

std::optional<QImage> captureWindowsRegion(QWidget *parent) {
    QWidget *window = parent ? parent->window() : nullptr;
    const bool wasVisible = window && window->isVisible();
    if (wasVisible) {
        window->hide();
        QApplication::processEvents(QEventLoop::AllEvents, 200);
    }

    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        if (wasVisible) {
            window->show();
        }
        QMessageBox::warning(parent, "Screenshot", "Windows did not report an available display.");
        DiagnosticLog::instance().append("Windows screenshot failed: no display was available.");
        return std::nullopt;
    }

    const QPixmap desktop = screen->grabWindow(0);
    if (desktop.isNull()) {
        if (wasVisible) {
            window->show();
        }
        QMessageBox::warning(parent, "Screenshot", "Windows could not capture the selected display.");
        DiagnosticLog::instance().append("Windows screenshot failed while reading the display.");
        return std::nullopt;
    }

    ScreenshotSelectionDialog selector(desktop, screen->geometry(), nullptr);
    const int result = selector.exec();
    std::optional<QImage> image;
    if (result == QDialog::Accepted) {
        image = selector.selectedImage();
        if (image.has_value()) {
            DiagnosticLog::instance().append(
                QString("Windows screenshot region captured (%1x%2)")
                    .arg(image->width())
                    .arg(image->height()));
        }
    } else {
        DiagnosticLog::instance().append("Windows screenshot selection was canceled.");
    }
    if (wasVisible) {
        window->show();
        window->raise();
        window->activateWindow();
    }
    return image;
}

std::optional<QImage> captureBoardScreenshot(QWidget *parent = nullptr) {
#ifdef Q_OS_WIN
    return captureWindowsRegion(parent);
#else
    QTemporaryFile temp(QDir::tempPath() + "/solution-finder-screenshot-XXXXXX.png");
    temp.setAutoRemove(false);
        if (!temp.open()) {
            QMessageBox::warning(parent, "Screenshot", "Could not create a temporary screenshot file.");
            DiagnosticLog::instance().append("Screenshot failed: could not create a temporary file.");
            return std::nullopt;
    }
    const QString path = temp.fileName();
    temp.close();
    QFile::remove(path);
    QStringList attempted;

#ifdef Q_OS_MAC
    if (QFileInfo::exists("/usr/sbin/screencapture")) {
        QString error;
        const auto image = runScreenshotCommand("/usr/sbin/screencapture", {"-i", path}, path, parent, &error);
        if (image.has_value()) {
            return image;
        }
        QMessageBox::warning(parent, "Screenshot", "screencapture failed: " + error);
        return std::nullopt;
    }
#else
    const auto tryTool = [&](const QString &label, const QString &program, const QStringList &arguments) -> std::optional<QImage> {
        if (program.isEmpty()) {
            return std::nullopt;
        }
        QString error;
        attempted << (label + " (" + program + ")");
        const auto image = runScreenshotCommand(program, arguments, path, parent, &error);
        if (image.has_value()) {
            return image;
        }
        attempted.last() += ": " + (error.isEmpty() ? "failed" : error);
        return std::nullopt;
    };

    const QString flameshot = findProgram("flameshot");
    if (!flameshot.isEmpty()) {
        if (const auto image = tryTool("flameshot", flameshot, {"gui", "-p", path}); image.has_value()) {
            return image;
        }
    }

    const QString grim = findProgram("grim");
    const QString slurp = findProgram("slurp");
    const QString shell = findProgram("sh").isEmpty() ? "/bin/sh" : findProgram("sh");
    if (!grim.isEmpty() && !slurp.isEmpty() && !shell.isEmpty()) {
        const QString script = "geometry=\"$(slurp)\" || exit 1\n"
                               "grim -g \"$geometry\" \"$1\"";
        if (const auto image = tryTool("grim+slurp", shell, {"-c", script, "solution-finder-screenshot", path}); image.has_value()) {
            return image;
        }
    }

    const QString gnomeScreenshot = findProgram("gnome-screenshot");
    if (!gnomeScreenshot.isEmpty()) {
        if (const auto image = tryTool("gnome-screenshot", gnomeScreenshot, {"-a", "-f", path}); image.has_value()) {
            return image;
        }
    }

    const QString spectacle = findProgram("spectacle");
    if (!spectacle.isEmpty()) {
        if (const auto image = tryTool("spectacle", spectacle, {"-r", "-b", "-n", "-o", path}); image.has_value()) {
            return image;
        }
    }

    const QString maim = findProgram("maim");
    if (!maim.isEmpty()) {
        if (const auto image = tryTool("maim", maim, {"-s", path}); image.has_value()) {
            return image;
        }
    }

    const QString scrot = findProgram("scrot");
    if (!scrot.isEmpty()) {
        if (const auto image = tryTool("scrot", scrot, {"-s", path}); image.has_value()) {
            return image;
        }
    }
#endif

    const QString detail = attempted.isEmpty()
        ? "No compatible region screenshot tool was found."
        : "Screenshot tools were found, but none produced an image:\n\n" + attempted.join("\n");
    QMessageBox::warning(parent,
                         "Screenshot",
                         detail + "\n\n"
                         "On Linux, install one of: gnome-screenshot, grim + slurp, spectacle, flameshot, maim, or scrot.");
    DiagnosticLog::instance().append(detail);
    return std::nullopt;
#endif
}

std::optional<DecodedFumen> decodeFumenV115(QString code) {
    code = code.trimmed();
    const int prefix = code.indexOf("v115@");
    if (prefix >= 0) {
        code = code.mid(prefix + 5);
    }
    code.remove(QRegularExpression("[^A-Za-z0-9+/]"));
    if (code.isEmpty()) {
        return std::nullopt;
    }

    const QString table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<int> enc;
    enc.reserve(code.size() + 16);
    for (QChar ch : code) {
        const int value = table.indexOf(ch);
        if (value >= 0) {
            enc.push_back(value);
        }
    }
    if (enc.empty()) {
        return std::nullopt;
    }

    std::array<int, kFumenBlocks> field{};
    field.fill(0);
    DecodedFumen decoded;
    int cursor = 0;
    int repeatCount = 0;
    int pages = 0;

    auto take = [&]() -> int {
        if (cursor >= static_cast<int>(enc.size())) {
            return 0;
        }
        return enc[cursor++];
    };

    while (cursor < static_cast<int>(enc.size()) && pages < 2000) {
        if (repeatCount < 1) {
            int j = 0;
            while (j < kFumenBlocks) {
                int tmp = take();
                tmp += take() * 64;
                int run = tmp % kFumenBlocks;
                tmp /= kFumenBlocks;
                int diff = tmp % 17;
                for (int i = 0; i <= run && j < kFumenBlocks; ++i) {
                    field[j++] += diff - 8;
                }
                if (diff * kFumenBlocks + run == 9 * kFumenBlocks - 1) {
                    repeatCount = take();
                }
            }
        } else {
            repeatCount--;
        }

        std::array<int, kFumenBlocks> pageField = field;

        int tmp = take();
        tmp += take() * 64;
        tmp += take() * 4096;
        const int piece = tmp % 8;
        tmp /= 8;
        const int rotation = tmp % 4;
        tmp /= 4;
        const int position = tmp % kFumenBlocks;
        tmp /= kFumenBlocks;
        const bool rise = (tmp % 2) != 0;
        tmp /= 2;
        const bool mirror = (tmp % 2) != 0;
        tmp /= 2;
        tmp /= 2; // color mode
        const bool hasComment = (tmp % 2) != 0;
        tmp /= 2;
        const bool noLock = (tmp % 2) != 0;

        FumenOperation operation;
        operation.type = qBound(0, piece, 7);
        operation.rotation = qBound(0, rotation, 3);
        operation.position = qBound(0, position, kFumenBlocks - 1);

        if (hasComment) {
            int commentHeader = take();
            commentHeader += take() * 64;
            const int length = commentHeader % 4096;
            for (int i = 0; i < length; i += 4) {
                take();
                take();
                take();
                take();
                take();
            }
        }

        for (int index = 0; index < kFumenBlocks; ++index) {
            pageField[index] = qBound(0, pageField[index], 8);
        }
        decoded.pages.push_back(pageField);
        decoded.operations.push_back(operation);

        if (!noLock) {
            if (piece > 0) {
                for (int index : fumenOperationCells(operation)) {
                    field[index] = piece;
                }
            }

            int writeRow = kFumenRows - 2;
            for (int readRow = kFumenRows - 2; readRow >= 0; --readRow) {
                bool full = true;
                for (int col = 0; col < kColumns; ++col) {
                    if (field[readRow * kColumns + col] <= 0) {
                        full = false;
                        break;
                    }
                }
                if (!full) {
                    for (int col = 0; col < kColumns; ++col) {
                        field[writeRow * kColumns + col] = field[readRow * kColumns + col];
                    }
                    --writeRow;
                }
            }
            for (; writeRow >= 0; --writeRow) {
                for (int col = 0; col < kColumns; ++col) {
                    field[writeRow * kColumns + col] = 0;
                }
            }

            if (rise) {
                for (int i = 0; i < kFumenBlocks - kColumns; ++i) {
                    field[i] = field[i + kColumns];
                }
                for (int i = kFumenBlocks - kColumns; i < kFumenBlocks; ++i) {
                    field[i] = 0;
                }
            }
            if (mirror) {
                for (int row = 0; row < kFumenRows - 1; ++row) {
                    for (int col = 0; col < kColumns / 2; ++col) {
                        std::swap(field[row * kColumns + col], field[row * kColumns + (kColumns - 1 - col)]);
                    }
                }
            }
        }
        ++pages;
    }

    decoded.pageCount = pages;
    if (decoded.pages.empty()) {
        std::array<int, kFumenBlocks> blank{};
        blank.fill(0);
        decoded.pages.push_back(blank);
        decoded.operations.push_back(FumenOperation());
    }
    return decoded;
}

class BoardWidget : public QWidget {
public:
    explicit BoardWidget(QWidget *parent = nullptr)
        : QWidget(parent) {
        cells_.fill(0);
        ghostCells_.fill(0);
        solutionCells_.fill(0);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setMinimumSize(260, 520);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setObjectName("boardWidget");
    }

    QSize sizeHint() const override {
        return QSize(320, 640);
    }

    const std::array<int, kColumns * kRows> &cells() const {
        return cells_;
    }

    void clearBoard() {
        cells_.fill(0);
        ghostCells_.fill(0);
        solutionCells_.fill(0);
        update();
        if (onChanged) {
            onChanged();
        }
    }

    void setCells(const std::array<int, kColumns * kRows> &cells) {
        if (cells_ == cells) {
            return;
        }
        cells_ = cells;
        update();
    }

    void setGhostCells(const std::array<int, kColumns * kRows> &cells) {
        if (ghostCells_ == cells) {
            return;
        }
        ghostCells_ = cells;
        update();
    }

    void setSolutionCells(const std::array<int, kColumns * kRows> &cells) {
        if (solutionCells_ == cells) {
            return;
        }
        solutionCells_ = cells;
        update();
    }

    void clearSolutionCells() {
        std::array<int, kColumns * kRows> blank{};
        blank.fill(0);
        setSolutionCells(blank);
    }

    void setRenderCells(const std::array<int, kColumns * kRows> &cells,
                        const std::array<int, kColumns * kRows> &ghostCells) {
        if (cells_ == cells && ghostCells_ == ghostCells) {
            return;
        }
        cells_ = cells;
        ghostCells_ = ghostCells;
        update();
    }

    void setPaintValue(int value) {
        paintValue_ = value;
    }

    int paintValue() const {
        return paintValue_;
    }

    void mirror() {
        std::array<int, kColumns * kRows> next{};
        next.fill(0);
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                next[row * kColumns + (kColumns - 1 - col)] = mirrorColor(cells_[row * kColumns + col]);
            }
        }
        cells_ = next;
        update();
        if (onChanged) {
            onChanged();
        }
    }

    std::function<void()> onChanged;
    std::function<bool(int)> onCellPressed;
    std::function<void(int, Qt::KeyboardModifiers)> onKeyPressed;
    std::function<void(int, Qt::KeyboardModifiers)> onKeyReleased;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.fillRect(rect(), QColor("#303030"));

        const QRect board = boardRect();
        const int cell = qMax(1, board.width() / kColumns);
        painter.fillRect(board, QColor("#050505"));
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                const int index = row * kColumns + col;
                const QRect r(
                    board.left() + col * cell,
                    board.top() + row * cell,
                    cell,
                    cell
                );
                const int value = cells_[index];
                const int ghostValue = ghostCells_[index];
                const int solutionValue = solutionCells_[index];
                if (value == 0) {
                    painter.fillRect(r, QColor("#080808"));
                    bool drewOverlay = false;
                    if (solutionValue > 0) {
                        const QColor base = cellColor(solutionValue);
                        QColor fill = base;
                        fill.setAlpha(82);
                        painter.fillRect(r.adjusted(1, 1, -1, -1), fill);

                        QColor highlight = base.lighter(135);
                        highlight.setAlpha(145);
                        painter.setPen(QPen(highlight, 2));
                        painter.drawLine(r.left() + 2, r.top() + 2, r.right() - 2, r.top() + 2);
                        painter.drawLine(r.left() + 2, r.top() + 2, r.left() + 2, r.bottom() - 2);

                        QColor border("#050505");
                        border.setAlpha(155);
                        painter.setPen(QPen(border, 1));
                        painter.drawRect(r.adjusted(0, 0, -1, -1));
                        drewOverlay = true;
                    }
                    if (ghostValue > 0) {
                        QColor ghost = cellColor(ghostValue);
                        ghost.setAlpha(78);
                        painter.fillRect(r.adjusted(3, 3, -3, -3), ghost);
                        QColor ghostOutline = cellColor(ghostValue).lighter(135);
                        ghostOutline.setAlpha(235);
                        painter.setPen(QPen(ghostOutline, 2));
                        painter.drawRect(r.adjusted(2, 2, -3, -3));
                        drewOverlay = true;
                    }
                    if (!drewOverlay) {
                        painter.setPen(QPen(QColor("#2a2a2a"), 1));
                        painter.drawRect(r.adjusted(0, 0, -1, -1));
                    }
                    continue;
                }

                const QColor fill = cellColor(value);
                painter.fillRect(r.adjusted(1, 1, -1, -1), fill);
                painter.setPen(QPen(fill.lighter(135), 2));
                painter.drawLine(r.left() + 2, r.top() + 2, r.right() - 2, r.top() + 2);
                painter.drawLine(r.left() + 2, r.top() + 2, r.left() + 2, r.bottom() - 2);
                painter.setPen(QPen(QColor("#050505"), 1));
                painter.drawRect(r.adjusted(0, 0, -1, -1));
            }
        }

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor("#050505"), 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(board.adjusted(1, 1, -1, -1), 7, 7);
    }

    void mousePressEvent(QMouseEvent *event) override {
        setFocus(Qt::MouseFocusReason);
        if (event->button() != Qt::LeftButton) {
            return;
        }
        painting_ = true;
        eraseStroke_ = cellValueAt(event->position().toPoint()) == paintValue_;
        paintCellAt(event->position().toPoint());
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (painting_) {
            paintCellAt(event->position().toPoint());
        }
    }

    void mouseReleaseEvent(QMouseEvent *) override {
        painting_ = false;
        eraseStroke_ = false;
        lastPainted_ = -1;
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (onKeyPressed) {
            if (!event->isAutoRepeat()) {
                onKeyPressed(event->key(), event->modifiers());
            }
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void keyReleaseEvent(QKeyEvent *event) override {
        if (onKeyReleased) {
            if (!event->isAutoRepeat()) {
                onKeyReleased(event->key(), event->modifiers());
            }
            event->accept();
            return;
        }
        QWidget::keyReleaseEvent(event);
    }

private:
    QRect boardRect() const {
        int side = qMin(width(), height() / 2);
        side = qMax(side, 240);
        int boardWidth = qMin(width() - 12, side);
        int boardHeight = boardWidth * 2;
        if (boardHeight > height() - 12) {
            boardHeight = height() - 12;
            boardWidth = boardHeight / 2;
        }
        boardWidth = qMax(10, (boardWidth / kColumns) * kColumns);
        boardHeight = boardWidth * 2;
        return QRect((width() - boardWidth) / 2, (height() - boardHeight) / 2, boardWidth, boardHeight);
    }

    void paintCellAt(const QPoint &point) {
        const QRect board = boardRect();
        if (!board.contains(point)) {
            return;
        }
        const double cell = static_cast<double>(board.width()) / kColumns;
        const int col = qBound(0, static_cast<int>((point.x() - board.left()) / cell), kColumns - 1);
        const int row = qBound(0, static_cast<int>((point.y() - board.top()) / cell), kRows - 1);
        const int index = row * kColumns + col;
        if (index == lastPainted_) {
            return;
        }
        lastPainted_ = index;
        if (onCellPressed && onCellPressed(index)) {
            return;
        }
        const int nextValue = eraseStroke_ ? 0 : paintValue_;
        if (cells_[index] == nextValue) {
            return;
        }
        cells_[index] = nextValue;
        update();
        if (onChanged) {
            onChanged();
        }
    }

    int cellValueAt(const QPoint &point) const {
        const QRect board = boardRect();
        if (!board.contains(point)) {
            return -1;
        }
        const double cell = static_cast<double>(board.width()) / kColumns;
        const int col = qBound(0, static_cast<int>((point.x() - board.left()) / cell), kColumns - 1);
        const int row = qBound(0, static_cast<int>((point.y() - board.top()) / cell), kRows - 1);
        return cells_[row * kColumns + col];
    }

    std::array<int, kColumns * kRows> cells_{};
    std::array<int, kColumns * kRows> ghostCells_{};
    std::array<int, kColumns * kRows> solutionCells_{};
    int paintValue_ = 8;
    int lastPainted_ = -1;
    bool painting_ = false;
    bool eraseStroke_ = false;
};

class PiecePreviewWidget : public QWidget {
public:
    explicit PiecePreviewWidget(QWidget *parent = nullptr)
        : QWidget(parent) {
        setMinimumSize(56, 56);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void setPiece(int piece) {
        if (piece_ == piece) {
            return;
        }
        piece_ = piece;
        update();
    }

    void setCellSize(int size) {
        cellSize_ = qBound(8, size, 20);
        const int side = cellSize_ * 4 + 6;
        setFixedSize(side, side);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#303030"));
        if (piece_ <= 0 || piece_ >= 8) {
            return;
        }
        const int spacing = qMax(1, cellSize_ / 8);
        const int gridSize = cellSize_ * 4 + spacing * 3;
        const int left = (width() - gridSize) / 2;
        const int top = (height() - gridSize) / 2;
        for (int row = 0; row < 4; ++row) {
            const int gameY = 3 - row;
            for (int column = 0; column < 4; ++column) {
                if (!sft_game_piece_cell(piece_, 0, column, gameY)) {
                    continue;
                }
                const int x = left + column * (cellSize_ + spacing);
                const int y = top + row * (cellSize_ + spacing);
                painter.fillRect(QRect(x, y, cellSize_, cellSize_), cellColor(piece_));
            }
        }
    }

private:
    int piece_ = 0;
    int cellSize_ = 14;
};

QString encodeFumenFields(const std::vector<std::array<int, kFumenBlocks>> &sourcePages) {
    QString output = "v115@";
    const QString table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::array<int, kFumenBlocks> previous{};
    previous.fill(0);
    std::vector<std::array<int, kFumenBlocks>> pages = sourcePages;
    if (pages.empty()) {
        std::array<int, kFumenBlocks> blank{};
        blank.fill(0);
        pages.push_back(blank);
    }

    for (int pageIndex = 0; pageIndex < static_cast<int>(pages.size()); ++pageIndex) {
        std::array<int, kFumenBlocks> page = pages[pageIndex];
        for (int index = 230; index < kFumenBlocks; ++index) {
            page[index] = 0;
        }
        int index = 0;
        while (index < kFumenBlocks) {
            const int diff = qBound(-8, page[index] - previous[index], 8);
            int run = 0;
            while (index + run + 1 < kFumenBlocks &&
                   run + 1 < kFumenBlocks &&
                   qBound(-8, page[index + run + 1] - previous[index + run + 1], 8) == diff) {
                ++run;
            }
            const int value = (diff + 8) * kFumenBlocks + run;
            output += table[value % 64];
            output += table[(value / 64) % 64];
            index += run + 1;
        }

        previous = page;
        const int action = 0
                           + 0 * 8
                           + 0 * 8 * 4
                           + 0 * 8 * 4 * kFumenBlocks
                           + 0 * 8 * 4 * kFumenBlocks * 2
                           + (pageIndex == 0 ? 1 : 0) * 8 * 4 * kFumenBlocks * 4
                           + 0 * 8 * 4 * kFumenBlocks * 8
                           + 1 * 8 * 4 * kFumenBlocks * 16;
        output += table[action % 64];
        output += table[(action / 64) % 64];
        output += table[(action / 4096) % 64];
    }
    return output;
}

std::array<int, kColumns * kRows> visibleFumenCells(const std::array<int, kFumenBlocks> &page) {
    std::array<int, kColumns * kRows> visible{};
    visible.fill(0);
    for (int row = 0; row < kRows; ++row) {
        for (int column = 0; column < kColumns; ++column) {
            visible[row * kColumns + column] =
                qBound(0, page[(kVisibleTopRow + row) * kColumns + column], 8);
        }
    }
    return visible;
}

QString openerSlug(const QString &name) {
    QString slug = name.toLower();
    slug.replace(QRegularExpression("[^a-z0-9]+"), "-");
    slug.remove(QRegularExpression("^-+|-+$"));
    return slug.isEmpty() ? "opener" : slug;
}

struct OpenerVariationDraft {
    QString name;
    QString code;
    std::vector<std::array<int, kFumenBlocks>> pages;
    int selectedPage = 0;
};

struct OpenerGroupDraft {
    QString name;
    bool earlyVariantDetection = false;
    OpenerVariationDraft base{"Base", "", {}, 0};
    std::vector<OpenerVariationDraft> variations;
};

class OpenerImporterDialog : public QDialog {
public:
    OpenerImporterDialog(QString repoRoot,
                         std::function<void()> databaseChanged,
                         QWidget *parent = nullptr)
        : QDialog(parent),
          repoRoot_(std::move(repoRoot)),
          databaseChanged_(std::move(databaseChanged)) {
        setWindowTitle("Opener Importer");
        setMinimumSize(820, 640);
        resize(1120, 760);
        buildUi();
        loadBook();
    }

private:
    void buildUi() {
        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(12, 12, 12, 12);
        rootLayout->setSpacing(10);

        auto *toolbar = new QHBoxLayout();
        auto *title = new QLabel("Opener Importer", this);
        title->setObjectName("paneTitle");
        auto *loadButton = new QPushButton("Load Book", this);
        auto *newButton = new QPushButton("New Group", this);
        auto *saveButton = new QPushButton("Save Book", this);
        saveButton->setObjectName("primaryButton");
        toolbar->addWidget(title);
        toolbar->addStretch(1);
        toolbar->addWidget(loadButton);
        toolbar->addWidget(newButton);
        toolbar->addWidget(saveButton);
        rootLayout->addLayout(toolbar);

        auto *splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setChildrenCollapsible(false);
        rootLayout->addWidget(splitter, 1);

        auto *editorScroll = new QScrollArea(splitter);
        editorScroll->setWidgetResizable(true);
        editorScroll->setFrameShape(QFrame::NoFrame);
        auto *editor = new QWidget(editorScroll);
        auto *editorLayout = new QVBoxLayout(editor);
        editorLayout->setContentsMargins(4, 4, 8, 4);
        editorLayout->setSpacing(10);

        auto *groupBox = new QGroupBox("Opener Group", editor);
        auto *groupLayout = new QFormLayout(groupBox);
        openerNameEdit_ = new QLineEdit(groupBox);
        openerNameEdit_->setPlaceholderText("Example: TKI 3");
        earlyVariantCheck_ = new QCheckBox("Detect variant after 6 pieces", groupBox);
        auto *updateButton = new QPushButton("Add / Update Group", groupBox);
        groupLayout->addRow("Name", openerNameEdit_);
        groupLayout->addRow("", earlyVariantCheck_);
        groupLayout->addRow("", updateButton);
        editorLayout->addWidget(groupBox);

        auto *baseBox = new QGroupBox("Base Fumen", editor);
        auto *baseLayout = new QVBoxLayout(baseBox);
        baseCodeEdit_ = new QPlainTextEdit(baseBox);
        baseCodeEdit_->setPlaceholderText("v115@...");
        baseCodeEdit_->setMaximumHeight(76);
        baseLayout->addWidget(baseCodeEdit_);
        auto *baseActions = new QHBoxLayout();
        auto *baseDecodeButton = new QPushButton("Decode", baseBox);
        auto *baseShotButton = new QPushButton("Screenshot", baseBox);
        basePageSpin_ = new QSpinBox(baseBox);
        basePageSpin_->setPrefix("Page ");
        basePageSpin_->setRange(1, 1);
        baseActions->addWidget(baseDecodeButton);
        baseActions->addWidget(baseShotButton);
        baseActions->addStretch(1);
        baseActions->addWidget(basePageSpin_);
        baseLayout->addLayout(baseActions);
        editorLayout->addWidget(baseBox);

        auto *variationBox = new QGroupBox("Variations", editor);
        auto *variationLayout = new QVBoxLayout(variationBox);
        auto *variationSelectorRow = new QHBoxLayout();
        variationBox_ = new QComboBox(variationBox);
        auto *addVariationButton = new QPushButton("Add", variationBox);
        removeVariationButton_ = new QPushButton("Remove", variationBox);
        variationSelectorRow->addWidget(variationBox_, 1);
        variationSelectorRow->addWidget(addVariationButton);
        variationSelectorRow->addWidget(removeVariationButton_);
        variationLayout->addLayout(variationSelectorRow);
        variationNameEdit_ = new QLineEdit(variationBox);
        variationNameEdit_->setPlaceholderText("Variation name");
        variationLayout->addWidget(variationNameEdit_);
        variationCodeEdit_ = new QPlainTextEdit(variationBox);
        variationCodeEdit_->setPlaceholderText("v115@...");
        variationCodeEdit_->setMaximumHeight(76);
        variationLayout->addWidget(variationCodeEdit_);
        auto *variationActions = new QHBoxLayout();
        auto *variationDecodeButton = new QPushButton("Decode", variationBox);
        auto *variationShotButton = new QPushButton("Screenshot", variationBox);
        variationPageSpin_ = new QSpinBox(variationBox);
        variationPageSpin_->setPrefix("Page ");
        variationPageSpin_->setRange(1, 1);
        variationActions->addWidget(variationDecodeButton);
        variationActions->addWidget(variationShotButton);
        variationActions->addStretch(1);
        variationActions->addWidget(variationPageSpin_);
        variationLayout->addLayout(variationActions);
        editorLayout->addWidget(variationBox);
        editorLayout->addStretch(1);
        editorScroll->setWidget(editor);

        auto *info = new QWidget(splitter);
        auto *infoLayout = new QVBoxLayout(info);
        infoLayout->setContentsMargins(8, 4, 4, 4);
        infoLayout->setSpacing(10);

        auto *previewBox = new QGroupBox("Preview", info);
        auto *previewLayout = new QVBoxLayout(previewBox);
        previewSourceBox_ = new QComboBox(previewBox);
        previewLayout->addWidget(previewSourceBox_);
        previewBoard_ = new BoardWidget(previewBox);
        previewBoard_->setMinimumSize(220, 440);
        previewBoard_->setMaximumSize(270, 540);
        previewBoard_->onCellPressed = [](int) { return true; };
        previewLayout->addWidget(previewBoard_, 1, Qt::AlignHCenter);
        infoLayout->addWidget(previewBox, 1);

        auto *bookBox = new QGroupBox("Opener Book", info);
        auto *bookLayout = new QVBoxLayout(bookBox);
        bookList_ = new QListWidget(bookBox);
        bookLayout->addWidget(bookList_, 1);
        auto *bookActions = new QHBoxLayout();
        auto *editButton = new QPushButton("Edit", bookBox);
        auto *removeButton = new QPushButton("Remove", bookBox);
        bookActions->addWidget(editButton);
        bookActions->addWidget(removeButton);
        bookActions->addStretch(1);
        bookLayout->addLayout(bookActions);
        infoLayout->addWidget(bookBox, 1);

        statusLabel_ = new QLabel("Ready", info);
        statusLabel_->setWordWrap(true);
        statusLabel_->setObjectName("paneSubtitle");
        infoLayout->addWidget(statusLabel_);

        splitter->addWidget(editorScroll);
        splitter->addWidget(info);
        splitter->setSizes({650, 430});

        connect(loadButton, &QPushButton::clicked, this, [this]() { loadBook(); });
        connect(newButton, &QPushButton::clicked, this, [this]() { newGroup(); });
        connect(saveButton, &QPushButton::clicked, this, [this]() { saveBook(); });
        connect(updateButton, &QPushButton::clicked, this, [this]() { addOrUpdateCurrentGroup(); });
        connect(baseDecodeButton, &QPushButton::clicked, this, [this]() { decodeBase(); });
        connect(baseShotButton, &QPushButton::clicked, this, [this]() { screenshotBase(); });
        connect(variationDecodeButton, &QPushButton::clicked, this, [this]() { decodeVariation(); });
        connect(variationShotButton, &QPushButton::clicked, this, [this]() { screenshotVariation(); });
        connect(addVariationButton, &QPushButton::clicked, this, [this]() { addVariation(); });
        connect(removeVariationButton_, &QPushButton::clicked, this, [this]() { removeVariation(); });
        connect(variationBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            selectVariation(index);
        });
        connect(basePageSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int page) {
            current_.base.selectedPage = page - 1;
            refreshPreview();
        });
        connect(variationPageSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int page) {
            if (selectedVariation_ >= 0 && selectedVariation_ < static_cast<int>(current_.variations.size())) {
                current_.variations[selectedVariation_].selectedPage = page - 1;
                refreshPreview();
            }
        });
        connect(previewSourceBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            refreshPreview();
        });
        connect(editButton, &QPushButton::clicked, this, [this]() { editSelectedBookGroup(); });
        connect(removeButton, &QPushButton::clicked, this, [this]() { removeSelectedBookGroup(); });
        connect(bookList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
            editSelectedBookGroup();
        });
    }

    bool parseBook(const QString &path, std::vector<OpenerGroupDraft> *groups, QString *error) const {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            *error = "Could not open " + path;
            return false;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            *error = "Invalid opener JSON: " + parseError.errorString();
            return false;
        }

        groups->clear();
        QHash<QString, int> indexes;
        const QJsonArray records = document.object().value("openers").toArray();
        for (const QJsonValue &value : records) {
            const QJsonObject object = value.toObject();
            const QString openerName =
                object.value("openerName").toString(object.value("name").toString()).trimmed();
            const QString id = object.value("id").toString();
            if (openerName.isEmpty() || id == "empty" || openerName == "Empty board") {
                continue;
            }
            const QString key = openerName.toLower();
            if (!indexes.contains(key)) {
                indexes.insert(key, static_cast<int>(groups->size()));
                OpenerGroupDraft group;
                group.name = openerName;
                group.earlyVariantDetection = object.value("earlyVariantDetection").toBool(false);
                groups->push_back(group);
            }
            OpenerGroupDraft &group = (*groups)[indexes.value(key)];
            group.earlyVariantDetection =
                group.earlyVariantDetection || object.value("earlyVariantDetection").toBool(false);
            OpenerVariationDraft variation;
            variation.name = object.value("variationName").toString("Base");
            variation.code = object.value("code").toString().trimmed();
            if (variation.code.startsWith("v115@")) {
                if (const auto decoded = decodeFumenV115(variation.code); decoded.has_value()) {
                    variation.pages = decoded->pages;
                }
            }
            if (variation.name.compare("Base", Qt::CaseInsensitive) == 0) {
                variation.name = "Base";
                group.base = variation;
            } else {
                group.variations.push_back(variation);
            }
        }
        std::sort(groups->begin(), groups->end(), [](const OpenerGroupDraft &lhs, const OpenerGroupDraft &rhs) {
            return lhs.name.compare(rhs.name, Qt::CaseInsensitive) < 0;
        });
        return true;
    }

    void loadBook() {
        const QString path = openerDatabasePath(repoRoot_);
        QString error;
        std::vector<OpenerGroupDraft> loaded;
        if (!parseBook(path, &loaded, &error)) {
            statusLabel_->setText(error);
            DiagnosticLog::instance().append("Opener importer load failed: " + error);
            return;
        }
        book_ = std::move(loaded);
        refreshBookList();
        newGroup();
        statusLabel_->setText(QString("Loaded %1 opener groups from %2").arg(book_.size()).arg(path));
        DiagnosticLog::instance().append(
            QString("Opener importer loaded %1 groups from %2").arg(book_.size()).arg(path));
    }

    void newGroup() {
        updating_ = true;
        current_ = OpenerGroupDraft();
        current_.variations.push_back(OpenerVariationDraft{"Standard", "", {}, 0});
        selectedVariation_ = 0;
        openerNameEdit_->clear();
        earlyVariantCheck_->setChecked(false);
        baseCodeEdit_->clear();
        basePageSpin_->setRange(1, 1);
        refreshVariationSelector();
        updating_ = false;
        showSelectedVariation();
        refreshPreviewSources();
        statusLabel_->setText("New opener group");
    }

    void commitCurrentEdits() {
        current_.name = openerNameEdit_->text().trimmed();
        current_.earlyVariantDetection = earlyVariantCheck_->isChecked();
        current_.base.code = baseCodeEdit_->toPlainText().trimmed();
        if (selectedVariation_ >= 0 && selectedVariation_ < static_cast<int>(current_.variations.size())) {
            current_.variations[selectedVariation_].name = variationNameEdit_->text().trimmed();
            current_.variations[selectedVariation_].code = variationCodeEdit_->toPlainText().trimmed();
        }
    }

    bool decodeDraft(OpenerVariationDraft *draft, const QString &label, bool showError = true) {
        draft->code = draft->code.trimmed();
        if (!draft->code.startsWith("v115@")) {
            if (showError) {
                statusLabel_->setText(label + " must begin with v115@");
            }
            return false;
        }
        const auto decoded = decodeFumenV115(draft->code);
        if (!decoded.has_value() || decoded->pages.empty()) {
            if (showError) {
                statusLabel_->setText("Could not decode " + label);
            }
            return false;
        }
        draft->pages = decoded->pages;
        draft->selectedPage = qBound(0, draft->selectedPage, static_cast<int>(draft->pages.size()) - 1);
        return true;
    }

    bool validateCurrent(QString *error) {
        commitCurrentEdits();
        if (current_.name.isEmpty()) {
            *error = "Add an opener name.";
            return false;
        }
        if (!decodeDraft(&current_.base, "Base fumen", false)) {
            *error = "Add a valid base fumen beginning with v115@.";
            return false;
        }
        QSet<QString> names;
        QSet<QString> codes{current_.base.code};
        for (OpenerVariationDraft &variation : current_.variations) {
            if (variation.name.isEmpty() && variation.code.isEmpty()) {
                continue;
            }
            if (variation.name.isEmpty() || !decodeDraft(&variation, "Variation fumen", false)) {
                *error = "Every variation needs a name and valid fumen code.";
                return false;
            }
            const QString nameKey = variation.name.toLower();
            if (names.contains(nameKey) || codes.contains(variation.code)) {
                *error = "Variation names and fumen codes must be unique within the group.";
                return false;
            }
            names.insert(nameKey);
            codes.insert(variation.code);
        }
        current_.variations.erase(
            std::remove_if(
                current_.variations.begin(),
                current_.variations.end(),
                [](const OpenerVariationDraft &variation) {
                    return variation.name.isEmpty() && variation.code.isEmpty();
                }),
            current_.variations.end());
        return true;
    }

    void addOrUpdateCurrentGroup() {
        QString error;
        if (!validateCurrent(&error)) {
            statusLabel_->setText(error);
            return;
        }
        book_.erase(
            std::remove_if(book_.begin(), book_.end(), [this](const OpenerGroupDraft &group) {
                return group.name.compare(current_.name, Qt::CaseInsensitive) == 0;
            }),
            book_.end());
        book_.push_back(current_);
        std::sort(book_.begin(), book_.end(), [](const OpenerGroupDraft &lhs, const OpenerGroupDraft &rhs) {
            return lhs.name.compare(rhs.name, Qt::CaseInsensitive) < 0;
        });
        const QString name = current_.name;
        refreshBookList();
        newGroup();
        statusLabel_->setText("Added " + name + " to the book draft");
    }

    void saveBook() {
        commitCurrentEdits();
        const bool hasDraft = !current_.name.isEmpty() || !current_.base.code.isEmpty();
        if (hasDraft) {
            QString error;
            if (!validateCurrent(&error)) {
                statusLabel_->setText("Current draft was not saved: " + error);
                return;
            }
            book_.erase(
                std::remove_if(book_.begin(), book_.end(), [this](const OpenerGroupDraft &group) {
                    return group.name.compare(current_.name, Qt::CaseInsensitive) == 0;
                }),
                book_.end());
            book_.push_back(current_);
        }
        if (book_.empty()) {
            statusLabel_->setText("The opener book cannot be empty.");
            return;
        }
        std::sort(book_.begin(), book_.end(), [](const OpenerGroupDraft &lhs, const OpenerGroupDraft &rhs) {
            return lhs.name.compare(rhs.name, Qt::CaseInsensitive) < 0;
        });

        QJsonArray records;
        QJsonObject empty;
        empty.insert("id", "empty");
        empty.insert("name", "Empty board");
        empty.insert("openerName", "Empty board");
        empty.insert("variationName", "Empty");
        empty.insert("cells", QJsonArray());
        records.append(empty);
        for (const OpenerGroupDraft &group : book_) {
            auto appendRecord = [&](const OpenerVariationDraft &variation, bool base) {
                QJsonObject record;
                const QString variationName = base ? "Base" : variation.name;
                record.insert(
                    "id",
                    openerSlug(group.name) + "-" + (base ? "base" : openerSlug(variationName)));
                record.insert("name", group.name + " " + variationName);
                record.insert("openerName", group.name);
                record.insert("variationName", variationName);
                record.insert("code", variation.code);
                if (group.earlyVariantDetection) {
                    record.insert("earlyVariantDetection", true);
                }
                records.append(record);
            };
            appendRecord(group.base, true);
            for (const OpenerVariationDraft &variation : group.variations) {
                appendRecord(variation, false);
            }
        }

        QJsonObject database;
        database.insert("version", 1);
        database.insert("openers", records);
        QSaveFile file(editableOpenerDatabasePath());
        if (!file.open(QIODevice::WriteOnly)) {
            statusLabel_->setText("Could not open the editable opener database for writing.");
            return;
        }
        file.write(QJsonDocument(database).toJson(QJsonDocument::Indented));
        if (!file.commit()) {
            statusLabel_->setText("Could not finish saving the opener database.");
            return;
        }
        refreshBookList();
        if (databaseChanged_) {
            databaseChanged_();
        }
        const QString message =
            QString("Saved %1 opener groups to %2").arg(book_.size()).arg(editableOpenerDatabasePath());
        statusLabel_->setText(message);
        DiagnosticLog::instance().append(message);
    }

    void decodeBase() {
        current_.base.code = baseCodeEdit_->toPlainText().trimmed();
        if (!decodeDraft(&current_.base, "Base fumen")) {
            return;
        }
        updatePageSpin(basePageSpin_, current_.base);
        refreshPreviewSources();
        previewSourceBox_->setCurrentIndex(0);
        refreshPreview();
        statusLabel_->setText(
            QString("Decoded base: %1 page%2")
                .arg(current_.base.pages.size())
                .arg(current_.base.pages.size() == 1 ? "" : "s"));
    }

    void decodeVariation() {
        if (selectedVariation_ < 0 || selectedVariation_ >= static_cast<int>(current_.variations.size())) {
            return;
        }
        OpenerVariationDraft &variation = current_.variations[selectedVariation_];
        variation.name = variationNameEdit_->text().trimmed();
        variation.code = variationCodeEdit_->toPlainText().trimmed();
        if (!decodeDraft(&variation, "Variation fumen")) {
            return;
        }
        updatePageSpin(variationPageSpin_, variation);
        refreshVariationSelector();
        refreshPreviewSources();
        previewSourceBox_->setCurrentIndex(selectedVariation_ + 1);
        refreshPreview();
        statusLabel_->setText(
            QString("Decoded %1: %2 page%3")
                .arg(variation.name)
                .arg(variation.pages.size())
                .arg(variation.pages.size() == 1 ? "" : "s"));
    }

    void screenshotBase() {
        const auto image = captureBoardScreenshot(this);
        if (!image.has_value()) {
            return;
        }
        const auto cells = fumenCellsFromBoardImage(*image, true);
        if (!cells.has_value()) {
            statusLabel_->setText("Could not read the screenshot as a Tetris board.");
            DiagnosticLog::instance().append("Opener importer could not classify the base screenshot.");
            return;
        }
        current_.base.pages = {*cells};
        current_.base.selectedPage = 0;
        current_.base.code = encodeFumenFields(current_.base.pages);
        baseCodeEdit_->setPlainText(current_.base.code);
        updatePageSpin(basePageSpin_, current_.base);
        refreshPreviewSources();
        previewSourceBox_->setCurrentIndex(0);
        refreshPreview();
        statusLabel_->setText("Imported opener base screenshot.");
        DiagnosticLog::instance().append("Opener importer captured a base screenshot.");
    }

    void screenshotVariation() {
        if (selectedVariation_ < 0 || selectedVariation_ >= static_cast<int>(current_.variations.size())) {
            return;
        }
        const auto image = captureBoardScreenshot(this);
        if (!image.has_value()) {
            return;
        }
        auto cells = fumenCellsFromBoardImage(*image, true);
        if (!cells.has_value()) {
            statusLabel_->setText("Could not read the screenshot as a Tetris board.");
            DiagnosticLog::instance().append("Opener importer could not classify the variation screenshot.");
            return;
        }
        if (current_.base.pages.empty() && baseCodeEdit_->toPlainText().trimmed().startsWith("v115@")) {
            current_.base.code = baseCodeEdit_->toPlainText().trimmed();
            decodeDraft(&current_.base, "Base fumen", false);
        }
        if (!current_.base.pages.empty()) {
            const int page = qBound(
                0, current_.base.selectedPage, static_cast<int>(current_.base.pages.size()) - 1);
            const auto &base = current_.base.pages[page];
            for (int index = 0; index < kFumenBlocks; ++index) {
                if (base[index] != 0 && (*cells)[index] != 0) {
                    (*cells)[index] = 8;
                }
            }
        }
        OpenerVariationDraft &variation = current_.variations[selectedVariation_];
        variation.name = variationNameEdit_->text().trimmed();
        variation.pages = {*cells};
        variation.selectedPage = 0;
        variation.code = encodeFumenFields(variation.pages);
        variationCodeEdit_->setPlainText(variation.code);
        updatePageSpin(variationPageSpin_, variation);
        refreshPreviewSources();
        previewSourceBox_->setCurrentIndex(selectedVariation_ + 1);
        refreshPreview();
        statusLabel_->setText("Imported variation screenshot; overlapping base cells are gray.");
        DiagnosticLog::instance().append("Opener importer captured a variation screenshot.");
    }

    void addVariation() {
        commitCurrentEdits();
        current_.variations.push_back(
            OpenerVariationDraft{QString("Variation %1").arg(current_.variations.size() + 1), "", {}, 0});
        selectedVariation_ = static_cast<int>(current_.variations.size()) - 1;
        refreshVariationSelector();
        showSelectedVariation();
        refreshPreviewSources();
    }

    void removeVariation() {
        if (selectedVariation_ < 0 || selectedVariation_ >= static_cast<int>(current_.variations.size())) {
            return;
        }
        current_.variations.erase(current_.variations.begin() + selectedVariation_);
        selectedVariation_ = qMin(selectedVariation_, static_cast<int>(current_.variations.size()) - 1);
        refreshVariationSelector();
        showSelectedVariation();
        refreshPreviewSources();
    }

    void selectVariation(int index) {
        if (updating_) {
            return;
        }
        if (selectedVariation_ >= 0 && selectedVariation_ < static_cast<int>(current_.variations.size())) {
            current_.variations[selectedVariation_].name = variationNameEdit_->text().trimmed();
            current_.variations[selectedVariation_].code = variationCodeEdit_->toPlainText().trimmed();
        }
        selectedVariation_ = index;
        showSelectedVariation();
    }

    void showSelectedVariation() {
        updating_ = true;
        const bool valid =
            selectedVariation_ >= 0 &&
            selectedVariation_ < static_cast<int>(current_.variations.size());
        variationNameEdit_->setEnabled(valid);
        variationCodeEdit_->setEnabled(valid);
        variationPageSpin_->setEnabled(valid);
        removeVariationButton_->setEnabled(valid);
        if (valid) {
            const OpenerVariationDraft &variation = current_.variations[selectedVariation_];
            variationNameEdit_->setText(variation.name);
            variationCodeEdit_->setPlainText(variation.code);
            updatePageSpin(variationPageSpin_, variation);
        } else {
            variationNameEdit_->clear();
            variationCodeEdit_->clear();
            variationPageSpin_->setRange(1, 1);
        }
        updating_ = false;
    }

    void refreshVariationSelector() {
        updating_ = true;
        variationBox_->clear();
        for (const OpenerVariationDraft &variation : current_.variations) {
            variationBox_->addItem(variation.name.isEmpty() ? "Unnamed variation" : variation.name);
        }
        if (!current_.variations.empty()) {
            selectedVariation_ = qBound(
                0, selectedVariation_, static_cast<int>(current_.variations.size()) - 1);
            variationBox_->setCurrentIndex(selectedVariation_);
        } else {
            selectedVariation_ = -1;
        }
        updating_ = false;
    }

    void refreshPreviewSources() {
        const int previous = previewSourceBox_->currentIndex();
        previewSourceBox_->blockSignals(true);
        previewSourceBox_->clear();
        previewSourceBox_->addItem("Base", -1);
        for (int index = 0; index < static_cast<int>(current_.variations.size()); ++index) {
            const QString name = current_.variations[index].name.isEmpty()
                ? QString("Variation %1").arg(index + 1)
                : current_.variations[index].name;
            previewSourceBox_->addItem(name, index);
        }
        previewSourceBox_->setCurrentIndex(qBound(0, previous, previewSourceBox_->count() - 1));
        previewSourceBox_->blockSignals(false);
        refreshPreview();
    }

    void refreshPreview() {
        std::array<int, kColumns * kRows> blank{};
        blank.fill(0);
        if (!previewBoard_ || !previewSourceBox_) {
            return;
        }
        const int variationIndex = previewSourceBox_->currentData().toInt();
        const OpenerVariationDraft *source = variationIndex < 0
            ? &current_.base
            : (variationIndex < static_cast<int>(current_.variations.size())
                   ? &current_.variations[variationIndex]
                   : nullptr);
        if (!source || source->pages.empty()) {
            previewBoard_->setCells(blank);
            return;
        }
        const int page =
            qBound(0, source->selectedPage, static_cast<int>(source->pages.size()) - 1);
        previewBoard_->setCells(visibleFumenCells(source->pages[page]));
    }

    void updatePageSpin(QSpinBox *spin, const OpenerVariationDraft &draft) {
        const int count = qMax(1, static_cast<int>(draft.pages.size()));
        spin->blockSignals(true);
        spin->setRange(1, count);
        spin->setValue(qBound(1, draft.selectedPage + 1, count));
        spin->blockSignals(false);
    }

    void refreshBookList() {
        bookList_->clear();
        for (const OpenerGroupDraft &group : book_) {
            auto *item = new QListWidgetItem(
                QString("%1\n%2 fumen code%3%4")
                    .arg(group.name)
                    .arg(1 + group.variations.size())
                    .arg(group.variations.empty() ? "" : "s")
                    .arg(group.earlyVariantDetection ? " | variant at 6" : ""),
                bookList_);
            item->setData(Qt::UserRole, group.name);
        }
    }

    void editSelectedBookGroup() {
        const int row = bookList_->currentRow();
        if (row < 0 || row >= static_cast<int>(book_.size())) {
            return;
        }
        current_ = book_[row];
        selectedVariation_ = current_.variations.empty() ? -1 : 0;
        updating_ = true;
        openerNameEdit_->setText(current_.name);
        earlyVariantCheck_->setChecked(current_.earlyVariantDetection);
        baseCodeEdit_->setPlainText(current_.base.code);
        updatePageSpin(basePageSpin_, current_.base);
        refreshVariationSelector();
        updating_ = false;
        showSelectedVariation();
        refreshPreviewSources();
        statusLabel_->setText("Editing " + current_.name);
    }

    void removeSelectedBookGroup() {
        const int row = bookList_->currentRow();
        if (row < 0 || row >= static_cast<int>(book_.size())) {
            return;
        }
        const QString name = book_[row].name;
        const auto answer = QMessageBox::question(
            this,
            "Remove Opener Group",
            "Remove " + name + " from the opener book draft?");
        if (answer != QMessageBox::Yes) {
            return;
        }
        book_.erase(book_.begin() + row);
        refreshBookList();
        statusLabel_->setText("Removed " + name + " from the book draft");
    }

    QString repoRoot_;
    std::function<void()> databaseChanged_;
    std::vector<OpenerGroupDraft> book_;
    OpenerGroupDraft current_;
    int selectedVariation_ = -1;
    bool updating_ = false;

    QLineEdit *openerNameEdit_ = nullptr;
    QCheckBox *earlyVariantCheck_ = nullptr;
    QPlainTextEdit *baseCodeEdit_ = nullptr;
    QSpinBox *basePageSpin_ = nullptr;
    QComboBox *variationBox_ = nullptr;
    QPushButton *removeVariationButton_ = nullptr;
    QLineEdit *variationNameEdit_ = nullptr;
    QPlainTextEdit *variationCodeEdit_ = nullptr;
    QSpinBox *variationPageSpin_ = nullptr;
    QComboBox *previewSourceBox_ = nullptr;
    BoardWidget *previewBoard_ = nullptr;
    QListWidget *bookList_ = nullptr;
    QLabel *statusLabel_ = nullptr;
};

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr)
        : QMainWindow(parent),
          repoRoot_(findRepoRoot(QCoreApplication::applicationDirPath())) {
        setWindowTitle("Solution Finder Enhanced - Qt");
        setMinimumSize(980, 700);
        resize(1250, 800);
        buildMenus();
        buildUi();
        ensureFumenState();
        loadOpeners();
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
        updateGeneratedField();
    }

    ~MainWindow() override {
        if (process_ && process_->state() != QProcess::NotRunning) {
            process_->kill();
            process_->waitForFinished(2000);
        }
        if (pcScoutProcess_ && pcScoutProcess_->state() != QProcess::NotRunning) {
            pcScoutProcess_->kill();
            pcScoutProcess_->waitForFinished(2000);
        }
        if (auxiliaryScoutProcess_ && auxiliaryScoutProcess_->state() != QProcess::NotRunning) {
            auxiliaryScoutProcess_->kill();
            auxiliaryScoutProcess_->waitForFinished(2000);
        }
    }

private:
    void buildMenus() {
#ifdef Q_OS_MAC
        menuBar()->setNativeMenuBar(true);
#else
        menuBar()->setNativeMenuBar(false);
#endif
        auto *toolsMenu = menuBar()->addMenu("Tools");
        auto *importerAction = toolsMenu->addAction("Opener Importer");
        importerAction->setShortcut(QKeySequence("Ctrl+Shift+I"));
        auto *logAction = toolsMenu->addAction("Output Log");
        logAction->setShortcut(QKeySequence("Ctrl+Shift+D"));
        toolsMenu->addSeparator();
        auto *installDatabaseAction = toolsMenu->addAction("Install Editable Opener Database");
        auto *showDatabaseAction = toolsMenu->addAction("Show Opener Database Folder");

        connect(importerAction, &QAction::triggered, this, [this]() {
            showOpenerImporter();
        });
        connect(logAction, &QAction::triggered, this, [this]() {
            showOutputLog();
        });
        connect(installDatabaseAction, &QAction::triggered, this, [this]() {
            installEditableOpenerDatabase();
        });
        connect(showDatabaseAction, &QAction::triggered, this, [this]() {
            showOpenerDatabaseFolder();
        });
    }

    void showOpenerImporter() {
        if (!openerImporterDialog_) {
            openerImporterDialog_ = new OpenerImporterDialog(
                repoRoot_,
                [this]() {
                    loadOpeners(false);
                    DiagnosticLog::instance().append("Reloaded the opener database after saving.");
                },
                this);
        }
        openerImporterDialog_->show();
        openerImporterDialog_->raise();
        openerImporterDialog_->activateWindow();
    }

    void showOutputLog() {
        if (!outputLogDialog_) {
            outputLogDialog_ = new OutputLogDialog(this);
        }
        outputLogDialog_->show();
        outputLogDialog_->raise();
        outputLogDialog_->activateWindow();
    }

    void installEditableOpenerDatabase() {
        const QString destination = editableOpenerDatabasePath();
        if (!QFileInfo::exists(destination)) {
            const QString source = QDir(repoRoot_).filePath("shared/openers.json");
            QDir().mkpath(QFileInfo(destination).absolutePath());
            if (!QFile::copy(source, destination)) {
                QMessageBox::warning(
                    this,
                    "Editable Opener Database",
                    "Could not copy the bundled opener database to:\n" + destination);
                DiagnosticLog::instance().append(
                    "Failed to install editable opener database at " + destination);
                return;
            }
            loadOpeners(false);
            DiagnosticLog::instance().append("Installed editable opener database at " + destination);
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(destination).absolutePath()));
    }

    void showOpenerDatabaseFolder() {
        const QString folder = QFileInfo(editableOpenerDatabasePath()).absolutePath();
        QDir().mkpath(folder);
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    }

    void buildUi() {
        auto *root = new QWidget(this);
        auto *mainLayout = new QVBoxLayout(root);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        auto *splitter = new QSplitter(Qt::Horizontal, root);
        splitter->setObjectName("mainSplitter");
        splitter->setChildrenCollapsible(false);
        splitter->setHandleWidth(1);

        auto makeSidebar = [splitter](QWidget *content) {
            auto *scroll = new QScrollArea(splitter);
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scroll->setMinimumWidth(260);
            scroll->setMaximumWidth(380);
            scroll->setWidget(content);
            return scroll;
        };

        auto *left = makeSidebar(buildSettingsPanel());
        auto *center = buildBoardPanel();
        center->setMinimumWidth(320);
        auto *right = makeSidebar(buildOutputPanel());

        splitter->addWidget(left);
        splitter->addWidget(center);
        splitter->addWidget(right);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setStretchFactor(2, 0);
        splitter->setSizes({315, 620, 315});
        mainLayout->addWidget(splitter, 1);

        setCentralWidget(root);
    }

    QWidget *buildSettingsPanel() {
        auto *panel = new QWidget(this);
        panel->setMinimumWidth(260);
        auto *outerLayout = new QVBoxLayout(panel);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->setSpacing(0);

        searchSettingsPanel_ = new QWidget(panel);
        auto *layout = new QVBoxLayout(searchSettingsPanel_);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);

        auto *commandGroup = new QGroupBox("Search Settings", panel);
        searchForm_ = new QFormLayout(commandGroup);
        commandBox_ = new QComboBox(commandGroup);
        commandBox_->addItems({"percent", "path", "tetris", "tetris-path", "setup", "cover", "ren", "spin"});
        holdBox_ = new QComboBox(commandGroup);
        holdBox_->addItems({"use", "avoid"});
        dropBox_ = new QComboBox(commandGroup);
        dropBox_->addItems({"softdrop", "harddrop"});
        linesSpin_ = new QSpinBox(commandGroup);
        linesSpin_->setRange(1, 20);
        linesSpin_->setValue(4);
        spinHeightEdit_ = new QLineEdit(commandGroup);
        spinHeightEdit_->setReadOnly(true);
        spinHeightEdit_->setFocusPolicy(Qt::NoFocus);
        patternsEdit_ = new QLineEdit("t,*p5", commandGroup);
        searchForm_->addRow("Command", commandBox_);
        searchForm_->addRow("Hold", holdBox_);
        searchForm_->addRow("Drop", dropBox_);
        searchForm_->addRow("Lines", linesSpin_);
        searchForm_->addRow("Board height", spinHeightEdit_);
        searchForm_->addRow("Patterns", patternsEdit_);
        layout->addWidget(commandGroup);

        paintGroup_ = new QGroupBox("Paint", panel);
        auto *paintLayout = new QGridLayout(paintGroup_);
        const std::vector<int> palette = {0, 8, 1, 2, 3, 4, 5, 6, 7};
        int column = 0;
        for (int value : palette) {
            auto *button = new QPushButton(cellName(value), paintGroup_);
            button->setCheckable(true);
            button->setMinimumHeight(34);
            button->setStyleSheet(QString(
                "QPushButton { background: %1; color: %2; border: 1px solid #242424; border-radius: 5px; font-weight: 650; }"
                "QPushButton:checked { border: 3px solid #0a84ff; }")
                                      .arg(cellColor(value).name(), value == 0 ? "#d8d8d8" : "#ffffff"));
            paintValues_.push_back(value);
            paintButtons_.push_back(button);
            paintLayout->addWidget(button, column / 3, column % 3);
            connect(button, &QPushButton::clicked, this, [this, value]() {
                selectPaint(value);
            });
            ++column;
        }
        layout->addWidget(paintGroup_);

        auto *openerGroup = new QGroupBox("Openers", panel);
        auto *openerLayout = new QFormLayout(openerGroup);
        openerGroupBox_ = new QComboBox(openerGroup);
        openerVariationBox_ = new QComboBox(openerGroup);
        openerLayout->addRow("Base", openerGroupBox_);
        openerLayout->addRow("Variation", openerVariationBox_);
        auto *openerActions = new QHBoxLayout();
        auto *detectorButton = new QPushButton("Opener Detector", openerGroup);
        openerActions->addWidget(detectorButton);
        openerLayout->addRow("", openerActions);
        layout->addWidget(openerGroup);

        auto *fumenCodeGroup = new QGroupBox("Fumen Code", panel);
        auto *fumenCodeLayout = new QVBoxLayout(fumenCodeGroup);
        fumenEdit_ = new QPlainTextEdit(fumenCodeGroup);
        fumenEdit_->setPlaceholderText("Paste or select a fumen code.");
        fumenEdit_->setMaximumHeight(76);
        fumenCodeLayout->addWidget(fumenEdit_);
        layout->addWidget(fumenCodeGroup);

        auto *fieldGroup = new QGroupBox("sfinder Field", panel);
        auto *fieldLayout = new QVBoxLayout(fieldGroup);
        generatedField_ = new QPlainTextEdit(fieldGroup);
        generatedField_->setReadOnly(true);
        generatedField_->setMaximumHeight(120);
        fieldLayout->addWidget(generatedField_);
        layout->addWidget(fieldGroup);

        layout->addStretch(1);

        connect(openerGroupBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            populateVariations();
        });
        connect(openerVariationBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            loadSelectedOpener();
        });
        connect(detectorButton, &QPushButton::clicked, this, [this]() {
            detectOpeningFromScreenshot();
        });
        connect(commandBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            updateCommandUi();
        });
        connect(fumenEdit_, &QPlainTextEdit::textChanged, this, [this]() {
            loadFumenCodeFromText();
        });
        selectPaint(8);
        updateCommandUi();

        playSettingsPanel_ = buildPlaySettingsPanel(panel);
        playSettingsPanel_->setVisible(false);
        outerLayout->addWidget(searchSettingsPanel_);
        outerLayout->addWidget(playSettingsPanel_);
        return panel;
    }

    QWidget *buildPlaySettingsPanel(QWidget *parent) {
        auto *panel = new QWidget(parent);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);

        auto *gameGroup = new QGroupBox("Game Settings", panel);
        auto *gameLayout = new QVBoxLayout(gameGroup);
        playQueueEdit_ = new QLineEdit(gameGroup);
        playQueueEdit_->setPlaceholderText("Queue, e.g. TILJSZO");
        gameLayout->addWidget(playQueueEdit_);
        auto *holdRow = new QHBoxLayout();
        holdRow->addWidget(new QLabel("Hold", gameGroup));
        playHoldBox_ = new QComboBox(gameGroup);
        playHoldBox_->addItems({"-", "I", "L", "O", "Z", "T", "J", "S"});
        auto *applyButton = new QPushButton("Apply", gameGroup);
        holdRow->addWidget(playHoldBox_, 1);
        holdRow->addWidget(applyButton);
        gameLayout->addLayout(holdRow);
        auto *randomButton = new QPushButton("Random 7-bag", gameGroup);
        gameLayout->addWidget(randomButton);
        auto *queueHelp = new QLabel("Queue accepts piece letters; spaces and commas are ignored.", gameGroup);
        queueHelp->setWordWrap(true);
        queueHelp->setObjectName("paneSubtitle");
        gameLayout->addWidget(queueHelp);
        layout->addWidget(gameGroup);

        auto *controlsGroup = new QGroupBox("Controls", panel);
        auto *controlsForm = new QFormLayout(controlsGroup);
        const std::array<QString, 10> labels = {"Left", "Right", "Soft", "Hard", "CW", "CCW", "180", "Hold", "Undo", "Reset"};
        const std::array<QString, 10> defaults = {"a", "d", "s", "space", "w", "q", "e", "c", "z", "r"};
        for (int i = 0; i < static_cast<int>(playControlEdits_.size()); ++i) {
            playControlEdits_[i] = new QLineEdit(controlsGroup);
            playControlEdits_[i]->setMaximumWidth(110);
            playControlEdits_[i]->setText(defaults[i]);
            controlsForm->addRow(labels[i], playControlEdits_[i]);
            connect(playControlEdits_[i], &QLineEdit::editingFinished, this, [this]() { savePlaySettings(); });
        }
        layout->addWidget(controlsGroup);

        auto *tuningGroup = new QGroupBox("Tuning", panel);
        auto *tuningForm = new QFormLayout(tuningGroup);
        auto makeTuning = [tuningGroup, tuningForm](const QString &label, int minimum, int maximum, int value) {
            auto *spin = new QSpinBox(tuningGroup);
            spin->setRange(minimum, maximum);
            spin->setValue(value);
            spin->setSuffix(" ms");
            tuningForm->addRow(label, spin);
            return spin;
        };
        playDasSpin_ = makeTuning("DAS", 0, 300, 130);
        playArrSpin_ = makeTuning("ARR", 0, 120, 28);
        playSoftSpin_ = makeTuning("Soft", 0, 250, 75);
        playGravityLevelBox_ = new QComboBox(tuningGroup);
        for (int level = 1; level <= 30; ++level) {
            playGravityLevelBox_->addItem(QString("Level %1").arg(level), level);
        }
        tuningForm->addRow("Start level", playGravityLevelBox_);
        playLockSpin_ = makeTuning("Lock", 0, 1000, 500);
        playMoveResetLimitSpin_ = new QSpinBox(tuningGroup);
        playMoveResetLimitSpin_->setRange(1, 99);
        playMoveResetLimitSpin_->setValue(15);
        tuningForm->addRow("Move limit", playMoveResetLimitSpin_);
        playPreviewSpin_ = new QSpinBox(tuningGroup);
        playPreviewSpin_->setRange(8, 20);
        playPreviewSpin_->setValue(14);
        playPreviewSpin_->setSuffix(" px");
        tuningForm->addRow("Preview", playPreviewSpin_);
        playGravityCheck_ = new QCheckBox("Gravity", tuningGroup);
        playGravityCheck_->setChecked(true);
        playLevelProgressionCheck_ = new QCheckBox("Level progression", tuningGroup);
        playMoveResetCheck_ = new QCheckBox("Move reset", tuningGroup);
        playMoveResetCheck_->setChecked(true);
        playStepResetCheck_ = new QCheckBox("Step reset", tuningGroup);
        playInfiniteLockCheck_ = new QCheckBox("Infinite lock delay", tuningGroup);
        playInfiniteHoldCheck_ = new QCheckBox("Infinite hold", tuningGroup);
        playExportActiveCheck_ = new QCheckBox("Export active piece", tuningGroup);
        auto *rules = new QWidget(tuningGroup);
        auto *rulesLayout = new QVBoxLayout(rules);
        rulesLayout->setContentsMargins(0, 0, 0, 0);
        rulesLayout->addWidget(playGravityCheck_);
        rulesLayout->addWidget(playLevelProgressionCheck_);
        rulesLayout->addWidget(playMoveResetCheck_);
        rulesLayout->addWidget(playStepResetCheck_);
        rulesLayout->addWidget(playInfiniteLockCheck_);
        rulesLayout->addWidget(playInfiniteHoldCheck_);
        rulesLayout->addWidget(playExportActiveCheck_);
        tuningForm->addRow("Rules", rules);
        layout->addWidget(tuningGroup);
        layout->addStretch(1);

        const std::array<QSpinBox *, 5> tuningSpins = {
            playDasSpin_, playArrSpin_, playSoftSpin_, playMoveResetLimitSpin_, playPreviewSpin_
        };
        for (auto *spin : tuningSpins) {
            connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this]() {
                savePlaySettings();
                applyPlayTuning();
                updatePiecePreviewSizes();
            });
        }
        connect(playLockSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
            if (updatingPlayLevelLock_) {
                return;
            }
            playConfiguredLockDelay_ = value;
            const int level = playGravityLevelBox_->currentData().toInt();
            if (level > 20 && value != sft_game_lock_delay_for_level(level)) {
                playConfiguredGravityLevel_ = 20;
                sft_game_reset_level_progression(&playGame_, 20);
                updatingPlayLevelLock_ = true;
                playGravityLevelBox_->setCurrentIndex(19);
                updatingPlayLevelLock_ = false;
            }
            savePlaySettings();
            applyPlayTuning();
        });
        connect(playGravityLevelBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            if (updatingPlayLevelLock_) {
                return;
            }
            const int level = playGravityLevelBox_->currentData().toInt();
            playConfiguredGravityLevel_ = level;
            if (loadingPlaySettings_) {
                return;
            }
            sft_game_reset_level_progression(&playGame_, level);
            if (level >= 20) {
                playConfiguredLockDelay_ = sft_game_lock_delay_for_level(level);
                updatingPlayLevelLock_ = true;
                playLockSpin_->setValue(playConfiguredLockDelay_);
                updatingPlayLevelLock_ = false;
            }
            savePlaySettings();
            applyPlayTuning();
        });
        connect(playMoveResetCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
            if (enabled && playStepResetCheck_->isChecked()) {
                playStepResetCheck_->setChecked(false);
            }
            playMoveResetLimitSpin_->setEnabled(enabled);
        });
        connect(playStepResetCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
            if (enabled && playMoveResetCheck_->isChecked()) {
                playMoveResetCheck_->setChecked(false);
            }
        });
        const std::array<QCheckBox *, 7> ruleChecks = {
            playGravityCheck_, playLevelProgressionCheck_, playMoveResetCheck_, playStepResetCheck_,
            playInfiniteLockCheck_, playInfiniteHoldCheck_, playExportActiveCheck_
        };
        for (auto *check : ruleChecks) {
            connect(check, &QCheckBox::toggled, this, [this]() {
                savePlaySettings();
                applyPlayTuning();
            });
        }
        connect(applyButton, &QPushButton::clicked, this, [this]() { applyPlayQueueAndHold(); });
        connect(randomButton, &QPushButton::clicked, this, [this]() {
            playQueueEdit_->clear();
            playHoldBox_->setCurrentIndex(0);
            resetPlayGame();
        });
        connect(playQueueEdit_, &QLineEdit::editingFinished, this, [this]() { savePlaySettings(); });
        connect(playHoldBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { savePlaySettings(); });

        loadPlaySettings();
        return panel;
    }

    QWidget *buildBoardPanel() {
        auto *panel = new QWidget(this);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);

        auto *titleRow = new QHBoxLayout();
        sectionTabs_ = new QTabBar(panel);
        sectionTabs_->setObjectName("sectionTabs");
        sectionTabs_->addTab("Editor");
        sectionTabs_->addTab("Play");
        sectionTabs_->addTab("Output");
        sectionTabs_->addTab("Preview");
        sectionTabs_->setExpanding(false);
        titleRow->addStretch(1);
        titleRow->addWidget(sectionTabs_);
        titleRow->addStretch(1);
        layout->addLayout(titleRow);

        auto *actionRow = new QHBoxLayout();
        screenshotButton_ = new QPushButton("Screenshot", panel);
        runButton_ = new QPushButton("Run Search", panel);
        cancelButton_ = new QPushButton("Cancel", panel);
        runButton_->setObjectName("primaryButton");
        cancelButton_->setEnabled(false);
        actionRow->addWidget(screenshotButton_);
        actionRow->addWidget(runButton_);
        actionRow->addWidget(cancelButton_);
        actionRow->addStretch(1);
        layout->addLayout(actionRow);

        centerStack_ = new QStackedWidget(panel);
        auto *editorPage = new QWidget(centerStack_);
        auto *editorLayout = new QVBoxLayout(editorPage);
        editorLayout->setContentsMargins(0, 0, 0, 0);
        editorLayout->setSpacing(8);

        auto *toolbar = new QHBoxLayout();
        auto *clearButton = new QPushButton("Clear Page", panel);
        auto *mirrorButton = new QPushButton("Mirror", panel);
        toolbar->addWidget(clearButton);
        toolbar->addWidget(mirrorButton);
        toolbar->addStretch(1);
        editorLayout->addLayout(toolbar);

        auto *editorContent = new QHBoxLayout();
        editorContent->setSpacing(12);
        board_ = new BoardWidget(panel);
        board_->onChanged = [this]() {
            syncCurrentPageFromBoard();
            updateGeneratedField();
            updateFumenCodeFromPages();
        };
        board_->onCellPressed = [this](int visibleIndex) {
            return handleBoardCellPressed(visibleIndex);
        };
        editorContent->addWidget(board_, 1);
        editorContent->addWidget(buildFumenControlsPanel(panel), 0);
        editorLayout->addLayout(editorContent, 1);

        centerStack_->addWidget(editorPage);
        centerStack_->addWidget(buildPlayPage(centerStack_));
        centerStack_->addWidget(buildCenterOutputPage(centerStack_));
        centerStack_->addWidget(buildPreviewPage(centerStack_));
        layout->addWidget(centerStack_, 1);

        connect(sectionTabs_, &QTabBar::currentChanged, this, [this](int index) {
            centerStack_->setCurrentIndex(index);
            runButton_->setVisible(index != 1);
            cancelButton_->setVisible(index != 1);
            screenshotButton_->setVisible(index == 0);
            if (searchSettingsPanel_) {
                searchSettingsPanel_->setVisible(index != 1);
            }
            if (playSettingsPanel_) {
                playSettingsPanel_->setVisible(index == 1);
            }
            if (index == 1) {
                playBoard_->setFocus();
                schedulePCScout(true);
            } else {
                heldPlayInputs_.clear();
                if (index == 0) {
                    synchronizePlayFumenEditor();
                } else if (index == 2) {
                    refreshGeneratedFiles();
                } else if (index == 3) {
                    refreshPreviewCodes();
                }
            }
            updatePlayTimerState();
        });

        connect(screenshotButton_, &QPushButton::clicked, this, [this]() {
            const bool imported = importBoardScreenshot();
            if (imported && sectionTabs_ && sectionTabs_->currentIndex() == 1) {
                loadEditorBoardIntoPlay();
            }
        });
        connect(runButton_, &QPushButton::clicked, this, [this]() {
            runSearch();
        });
        connect(cancelButton_, &QPushButton::clicked, this, [this]() {
            cancelSearch();
        });

        connect(clearButton, &QPushButton::clicked, this, [this]() {
            clearCurrentPage();
        });
        connect(mirrorButton, &QPushButton::clicked, this, [this]() {
            mirrorCurrentPage();
        });
        return panel;
    }

    QWidget *buildFumenControlsPanel(QWidget *parent) {
        auto *panel = new QWidget(parent);
        panel->setMinimumWidth(240);
        panel->setMaximumWidth(300);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);

        minoGroup_ = new QGroupBox("Mino", panel);
        auto *minoLayout = new QVBoxLayout(minoGroup_);
        placeMinoCheck_ = new QCheckBox("Place mino", minoGroup_);
        minoPieceBox_ = new QComboBox(minoGroup_);
        minoPieceBox_->addItem("I", 1);
        minoPieceBox_->addItem("L", 2);
        minoPieceBox_->addItem("O", 3);
        minoPieceBox_->addItem("Z", 4);
        minoPieceBox_->addItem("T", 5);
        minoPieceBox_->addItem("J", 6);
        minoPieceBox_->addItem("S", 7);
        minoLayout->addWidget(placeMinoCheck_);
        minoLayout->addWidget(minoPieceBox_);
        auto *rotateRow = new QHBoxLayout();
        auto *ccwButton = new QPushButton("CCW", minoGroup_);
        auto *cwButton = new QPushButton("CW", minoGroup_);
        auto *clearMinoButton = new QPushButton("Clear", minoGroup_);
        rotateRow->addWidget(ccwButton);
        rotateRow->addWidget(cwButton);
        rotateRow->addWidget(clearMinoButton);
        minoLayout->addLayout(rotateRow);
        auto *moveGrid = new QGridLayout();
        auto *upButton = new QPushButton("Up", minoGroup_);
        auto *leftButton = new QPushButton("Left", minoGroup_);
        auto *downButton = new QPushButton("Down", minoGroup_);
        auto *rightButton = new QPushButton("Right", minoGroup_);
        moveGrid->addWidget(upButton, 0, 1);
        moveGrid->addWidget(leftButton, 1, 0);
        moveGrid->addWidget(downButton, 1, 1);
        moveGrid->addWidget(rightButton, 1, 2);
        minoLayout->addLayout(moveGrid);
        layout->addWidget(minoGroup_);

        pagesGroup_ = new QGroupBox("Pages", panel);
        auto *pagesLayout = new QVBoxLayout(pagesGroup_);
        auto *navRow = new QHBoxLayout();
        prevPageButton_ = new QPushButton("Previous", pagesGroup_);
        pageLabel_ = new QLabel("1/1", pagesGroup_);
        pageLabel_->setAlignment(Qt::AlignCenter);
        nextPageButton_ = new QPushButton("Next", pagesGroup_);
        navRow->addWidget(prevPageButton_);
        navRow->addWidget(pageLabel_);
        navRow->addWidget(nextPageButton_);
        pagesLayout->addLayout(navRow);
        auto *pageActions = new QGridLayout();
        addPageButton_ = new QPushButton("Add", pagesGroup_);
        trimBeforePagesButton_ = new QPushButton("Trim Before", pagesGroup_);
        trimPagesButton_ = new QPushButton("Trim After", pagesGroup_);
        pageActions->addWidget(addPageButton_, 0, 0, 1, 2);
        pageActions->addWidget(trimBeforePagesButton_, 1, 0);
        pageActions->addWidget(trimPagesButton_, 1, 1);
        pagesLayout->addLayout(pageActions);
        layout->addWidget(pagesGroup_);

        auto *outputCodeButton = new QPushButton("Output Code", panel);
        layout->addWidget(outputCodeButton);
        layout->addStretch(1);

        connect(placeMinoCheck_, &QCheckBox::toggled, this, [this](bool checked) {
            if (updatingFumenControls_) {
                return;
            }
            if (!checked) {
                currentOperation_ = FumenOperation();
            } else if (currentOperation_.type == 0) {
                currentOperation_.type = minoPieceBox_->currentData().toInt();
            }
            saveCurrentFumenPage();
            updateBoardFromFumenState();
            updateFumenCodeFromPages();
        });
        connect(minoPieceBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            if (updatingFumenControls_) {
                return;
            }
            currentOperation_.type = minoPieceBox_->currentData().toInt();
            placeMinoCheck_->setChecked(true);
            saveCurrentFumenPage();
            updateBoardFromFumenState();
            updateFumenCodeFromPages();
        });
        connect(ccwButton, &QPushButton::clicked, this, [this]() { rotateCurrentOperation(-1); });
        connect(cwButton, &QPushButton::clicked, this, [this]() { rotateCurrentOperation(1); });
        connect(clearMinoButton, &QPushButton::clicked, this, [this]() { clearCurrentOperation(); });
        connect(upButton, &QPushButton::clicked, this, [this]() { moveCurrentOperation(0, -1); });
        connect(leftButton, &QPushButton::clicked, this, [this]() { moveCurrentOperation(-1, 0); });
        connect(downButton, &QPushButton::clicked, this, [this]() { moveCurrentOperation(0, 1); });
        connect(rightButton, &QPushButton::clicked, this, [this]() { moveCurrentOperation(1, 0); });
        connect(prevPageButton_, &QPushButton::clicked, this, [this]() { goToFumenPage(currentFumenPage_ - 1); });
        connect(nextPageButton_, &QPushButton::clicked, this, [this]() { goToFumenPage(currentFumenPage_ + 1); });
        connect(addPageButton_, &QPushButton::clicked, this, [this]() { addFumenPage(); });
        connect(trimBeforePagesButton_, &QPushButton::clicked, this, [this]() { trimPreviousFumenPages(); });
        connect(trimPagesButton_, &QPushButton::clicked, this, [this]() { trimFollowingFumenPages(); });
        connect(outputCodeButton, &QPushButton::clicked, this, [this]() { updateFumenCodeFromPages(); });

        return panel;
    }

    QWidget *buildPlayPage(QWidget *parent) {
        auto *scroll = new QScrollArea(parent);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *page = new QWidget(scroll);
        page->setMinimumWidth(560);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);

        auto *stage = new QWidget(page);
        auto *stageLayout = new QHBoxLayout(stage);
        stageLayout->setContentsMargins(0, 0, 0, 0);
        stageLayout->setSpacing(8);
        stageLayout->addStretch(1);

        auto *holdColumn = new QWidget(page);
        holdColumn->setFixedWidth(64);
        auto *holdLayout = new QVBoxLayout(holdColumn);
        holdLayout->setContentsMargins(0, 0, 0, 0);
        holdLayout->addWidget(new QLabel("Hold", holdColumn), 0, Qt::AlignHCenter);
        playHoldPreview_ = new PiecePreviewWidget(holdColumn);
        holdLayout->addWidget(playHoldPreview_, 0, Qt::AlignTop | Qt::AlignHCenter);
        holdLayout->addStretch(1);
        stageLayout->addWidget(holdColumn, 0);

        auto *boardColumn = new QWidget(page);
        boardColumn->setMinimumWidth(260);
        boardColumn->setMaximumWidth(420);
        auto *boardLayout = new QVBoxLayout(boardColumn);
        boardLayout->setContentsMargins(0, 0, 0, 0);
        boardLayout->setSpacing(8);
        playBoard_ = new BoardWidget(boardColumn);
        playBoard_->setMaximumSize(400, 800);
        playBoard_->onCellPressed = [](int) { return true; };
        playBoard_->onKeyPressed = [this](int key, Qt::KeyboardModifiers modifiers) {
            handlePlayKeyPressed(key, modifiers);
        };
        playBoard_->onKeyReleased = [this](int key, Qt::KeyboardModifiers modifiers) {
            handlePlayKeyReleased(key, modifiers);
        };
        boardLayout->addWidget(playBoard_, 1, Qt::AlignHCenter);

        auto *boardActions = new QGridLayout();
        boardActions->setSpacing(6);
        auto *loadButton = new QPushButton("Load", boardColumn);
        auto *shotButton = new QPushButton("Screenshot", boardColumn);
        auto *sendButton = new QPushButton("Export", boardColumn);
        auto *newButton = new QPushButton("New", boardColumn);
        playUndoButton_ = new QPushButton("Undo", boardColumn);
        auto *focusButton = new QPushButton("Focus", boardColumn);
        const std::array<QPushButton *, 6> boardButtons = {
            loadButton, shotButton, sendButton, newButton, playUndoButton_, focusButton
        };
        for (int index = 0; index < static_cast<int>(boardButtons.size()); ++index) {
            auto *button = boardButtons[index];
            button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            boardActions->addWidget(button, index / 2, index % 2);
        }
        boardLayout->addLayout(boardActions);

        auto *pcScoutGroup = new QGroupBox("PC Scout  |  Uses extra CPU and may cause lag", boardColumn);
        auto *pcScoutLayout = new QVBoxLayout(pcScoutGroup);
        auto *pcScoutSourceRow = new QHBoxLayout();
        playPCEnabledCheck_ = new QCheckBox("Enabled", pcScoutGroup);
        playPCSourceBox_ = new QComboBox(pcScoutGroup);
        playPCSourceBox_->addItem("Active queue", "active");
        playPCSourceBox_->addItem("Random bag", "random");
        pcScoutSourceRow->addWidget(playPCEnabledCheck_);
        pcScoutSourceRow->addStretch(1);
        pcScoutSourceRow->addWidget(playPCSourceBox_);
        pcScoutLayout->addLayout(pcScoutSourceRow);
        auto *pcScoutActions = new QHBoxLayout();
        playPCDropBox_ = new QComboBox(pcScoutGroup);
        playPCDropBox_->addItem("Hard drop", "harddrop");
        playPCDropBox_->addItem("Soft drop", "softdrop");
        playPCDropBox_->setCurrentIndex(1);
        playPCShowSolutionButton_ = new QPushButton("Show Solution", pcScoutGroup);
        playPCShowSolutionButton_->setObjectName("primaryButton");
        playPCShowSolutionButton_->setEnabled(false);
        playPCCancelButton_ = new QPushButton("Cancel", pcScoutGroup);
        playPCCancelButton_->setEnabled(false);
        pcScoutActions->addWidget(playPCDropBox_, 1);
        pcScoutActions->addWidget(playPCShowSolutionButton_);
        pcScoutActions->addWidget(playPCCancelButton_);
        pcScoutLayout->addLayout(pcScoutActions);
        playPCResultsLabel_ = new QLabel(pcScoutGroup);
        playPCResultsLabel_->setWordWrap(true);
        playPCResultsLabel_->setTextFormat(Qt::RichText);
        pcScoutLayout->addWidget(playPCResultsLabel_);
        playPCStatusLabel_ = new QLabel("PC Scout is off", pcScoutGroup);
        playPCStatusLabel_->setWordWrap(true);
        playPCStatusLabel_->setObjectName("paneSubtitle");
        pcScoutLayout->addWidget(playPCStatusLabel_);
        pcScoutGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto *spinScoutPage = new QWidget(page);
        auto *spinScoutLayout = new QVBoxLayout(spinScoutPage);
        spinScoutLayout->setContentsMargins(10, 10, 10, 10);
        auto *spinScoutControls = new QHBoxLayout();
        playSpinScoutLinesBox_ = new QComboBox(spinScoutPage);
        playSpinScoutLinesBox_->addItem("T-Spin Single", 1);
        playSpinScoutLinesBox_->addItem("T-Spin Double", 2);
        playSpinScoutLinesBox_->addItem("T-Spin Triple", 3);
        playSpinScoutLinesBox_->setCurrentIndex(1);
        playSpinScoutPiecesSpin_ = new QSpinBox(spinScoutPage);
        playSpinScoutPiecesSpin_->setRange(2, 7);
        playSpinScoutPiecesSpin_->setValue(6);
        playSpinScoutPiecesSpin_->setPrefix("Depth ");
        playSpinScoutAutoCheck_ = new QCheckBox("Automatic", spinScoutPage);
        playSpinScoutButton_ = new QPushButton("Scout", spinScoutPage);
        playSpinScoutButton_->setObjectName("primaryButton");
        playSpinScoutCancelButton_ = new QPushButton("Cancel", spinScoutPage);
        playSpinScoutCancelButton_->setEnabled(false);
        playSpinScoutPreviewButton_ = new QPushButton("Show Preview", spinScoutPage);
        playSpinScoutPreviewButton_->setEnabled(false);
        spinScoutControls->addWidget(playSpinScoutLinesBox_, 1);
        spinScoutControls->addWidget(playSpinScoutPiecesSpin_);
        spinScoutControls->addWidget(playSpinScoutAutoCheck_);
        spinScoutControls->addWidget(playSpinScoutButton_);
        spinScoutControls->addWidget(playSpinScoutCancelButton_);
        spinScoutLayout->addLayout(spinScoutControls);
        playSpinScoutResultLabel_ = new QLabel(spinScoutPage);
        playSpinScoutResultLabel_->setWordWrap(true);
        spinScoutLayout->addWidget(playSpinScoutResultLabel_);
        auto *spinScoutBottom = new QHBoxLayout();
        playSpinScoutStatusLabel_ = new QLabel(
            "Uses the live field and exact active queue. Roof search is disabled for speed.",
            spinScoutPage);
        playSpinScoutStatusLabel_->setWordWrap(true);
        playSpinScoutStatusLabel_->setObjectName("paneSubtitle");
        spinScoutBottom->addWidget(playSpinScoutStatusLabel_, 1);
        spinScoutBottom->addWidget(playSpinScoutPreviewButton_);
        spinScoutLayout->addLayout(spinScoutBottom);

        auto *renScoutPage = new QWidget(page);
        auto *renScoutLayout = new QVBoxLayout(renScoutPage);
        renScoutLayout->setContentsMargins(10, 10, 10, 10);
        auto *renScoutControls = new QHBoxLayout();
        playRenScoutPiecesSpin_ = new QSpinBox(renScoutPage);
        playRenScoutPiecesSpin_->setRange(2, 7);
        playRenScoutPiecesSpin_->setValue(7);
        playRenScoutPiecesSpin_->setPrefix("Depth ");
        playRenScoutDropBox_ = new QComboBox(renScoutPage);
        playRenScoutDropBox_->addItem("Hard drop", "hard");
        playRenScoutDropBox_->addItem("Soft drop", "soft");
        playRenScoutDropBox_->setCurrentIndex(1);
        playRenScoutAutoCheck_ = new QCheckBox("Automatic", renScoutPage);
        playRenScoutButton_ = new QPushButton("Scout", renScoutPage);
        playRenScoutButton_->setObjectName("primaryButton");
        playRenScoutCancelButton_ = new QPushButton("Cancel", renScoutPage);
        playRenScoutCancelButton_->setEnabled(false);
        playRenScoutPreviewButton_ = new QPushButton("Show Preview", renScoutPage);
        playRenScoutPreviewButton_->setEnabled(false);
        renScoutControls->addWidget(playRenScoutDropBox_, 1);
        renScoutControls->addWidget(playRenScoutPiecesSpin_);
        renScoutControls->addWidget(playRenScoutAutoCheck_);
        renScoutControls->addWidget(playRenScoutButton_);
        renScoutControls->addWidget(playRenScoutCancelButton_);
        renScoutLayout->addLayout(renScoutControls);
        playRenScoutResultLabel_ = new QLabel(renScoutPage);
        playRenScoutResultLabel_->setWordWrap(true);
        renScoutLayout->addWidget(playRenScoutResultLabel_);
        auto *renScoutBottom = new QHBoxLayout();
        playRenScoutStatusLabel_ = new QLabel(
            "Finds the longest available combo using the live field and active queue.",
            renScoutPage);
        playRenScoutStatusLabel_->setWordWrap(true);
        playRenScoutStatusLabel_->setObjectName("paneSubtitle");
        renScoutBottom->addWidget(playRenScoutStatusLabel_, 1);
        renScoutBottom->addWidget(playRenScoutPreviewButton_);
        renScoutLayout->addLayout(renScoutBottom);

        playScoutTabs_ = new QTabWidget(page);
        playScoutTabs_->addTab(pcScoutGroup, "Perfect Clear");
        playScoutTabs_->addTab(spinScoutPage, "Spin");
        playScoutTabs_->addTab(renScoutPage, "REN");
        playScoutTabs_->setMinimumWidth(540);
        playScoutTabs_->setMaximumWidth(900);
        playScoutTabs_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        stageLayout->addWidget(boardColumn, 0);

        auto *nextColumn = new QWidget(page);
        nextColumn->setFixedWidth(64);
        auto *nextLayout = new QVBoxLayout(nextColumn);
        nextLayout->setContentsMargins(0, 0, 0, 0);
        nextLayout->addWidget(new QLabel("Next", nextColumn), 0, Qt::AlignHCenter);
        for (auto &preview : playNextPreviews_) {
            preview = new PiecePreviewWidget(nextColumn);
            nextLayout->addWidget(preview, 0, Qt::AlignHCenter);
        }
        nextLayout->addStretch(1);
        stageLayout->addWidget(nextColumn, 0);

        auto *side = new QWidget(page);
        side->setMinimumWidth(190);
        side->setMaximumWidth(240);
        auto *sideLayout = new QVBoxLayout(side);
        sideLayout->setContentsMargins(0, 0, 0, 0);
        sideLayout->setSpacing(10);

        auto *moveGroup = new QGroupBox("Moves", side);
        auto *moveGrid = new QGridLayout(moveGroup);
        auto *leftButton = new QPushButton("Left", moveGroup);
        auto *rightButton = new QPushButton("Right", moveGroup);
        auto *softButton = new QPushButton("Soft", moveGroup);
        auto *cwButton = new QPushButton("CW", moveGroup);
        auto *ccwButton = new QPushButton("CCW", moveGroup);
        auto *rotate180Button = new QPushButton("180", moveGroup);
        auto *holdButton = new QPushButton("Hold", moveGroup);
        auto *hardButton = new QPushButton("Hard Drop", moveGroup);
        moveGrid->addWidget(leftButton, 0, 0);
        moveGrid->addWidget(rightButton, 0, 1);
        moveGrid->addWidget(softButton, 0, 2);
        moveGrid->addWidget(cwButton, 1, 0);
        moveGrid->addWidget(ccwButton, 1, 1);
        moveGrid->addWidget(rotate180Button, 1, 2);
        moveGrid->addWidget(holdButton, 2, 0);
        moveGrid->addWidget(hardButton, 2, 1, 1, 2);
        sideLayout->addWidget(moveGroup);

        auto *statsGroup = new QGroupBox("Stats", side);
        auto *statsLayout = new QVBoxLayout(statsGroup);
        playPiecesLabel_ = new QLabel("Pieces: 0", statsGroup);
        playLinesLabel_ = new QLabel("Lines: 0", statsGroup);
        playLevelLabel_ = new QLabel("Level: 1", statsGroup);
        playPpsLabel_ = new QLabel("PPS: 0.00", statsGroup);
        playClearLabel_ = new QLabel(statsGroup);
        playClearLabel_->setStyleSheet("font-weight: 650;");
        playDetectionLabel_ = new QLabel(statsGroup);
        playDetectionLabel_->setWordWrap(true);
        playDetectionLabel_->setStyleSheet("font-size: 15px; font-weight: 650;");
        playStatusLabel_ = new QLabel(statsGroup);
        playStatusLabel_->setWordWrap(true);
        playStatusLabel_->setObjectName("paneSubtitle");
        statsLayout->addWidget(playPiecesLabel_);
        statsLayout->addWidget(playLinesLabel_);
        statsLayout->addWidget(playLevelLabel_);
        statsLayout->addWidget(playClearLabel_);
        statsLayout->addWidget(playPpsLabel_);
        statsLayout->addWidget(playDetectionLabel_);
        statsLayout->addWidget(playStatusLabel_);
        sideLayout->addWidget(statsGroup);
        sideLayout->addStretch(1);
        stageLayout->addWidget(side, 0);
        stageLayout->addStretch(1);
        layout->addWidget(stage, 0, Qt::AlignTop);
        layout->addWidget(playScoutTabs_, 0, Qt::AlignTop | Qt::AlignHCenter);
        layout->addStretch(1);

        connect(leftButton, &QPushButton::clicked, this, [this]() { runPlayCommand(SFT_CMD_LEFT); });
        connect(rightButton, &QPushButton::clicked, this, [this]() { runPlayCommand(SFT_CMD_RIGHT); });
        connect(softButton, &QPushButton::clicked, this, [this]() { runPlayCommand(SFT_CMD_SOFT_DROP); });
        connect(cwButton, &QPushButton::clicked, this, [this]() { runPlayCommand(SFT_CMD_ROTATE_CW); });
        connect(ccwButton, &QPushButton::clicked, this, [this]() { runPlayCommand(SFT_CMD_ROTATE_CCW); });
        connect(rotate180Button, &QPushButton::clicked, this, [this]() { runPlayCommand(SFT_CMD_ROTATE_180); });
        connect(holdButton, &QPushButton::clicked, this, [this]() { runPlayCommand(SFT_CMD_HOLD); });
        connect(hardButton, &QPushButton::clicked, this, [this]() { runPlayCommand(SFT_CMD_HARD_DROP); });
        connect(loadButton, &QPushButton::clicked, this, [this]() { loadEditorBoardIntoPlay(); });
        connect(shotButton, &QPushButton::clicked, this, [this]() {
            if (importBoardScreenshot()) {
                loadEditorBoardIntoPlay();
            }
        });
        connect(sendButton, &QPushButton::clicked, this, [this]() { exportPlayBoardToEditor(true); });
        connect(newButton, &QPushButton::clicked, this, [this]() { resetPlayGame(); });
        connect(playUndoButton_, &QPushButton::clicked, this, [this]() { undoPlayPlacement(); });
        connect(focusButton, &QPushButton::clicked, this, [this]() { playBoard_->setFocus(); });
        connect(playPCShowSolutionButton_, &QPushButton::clicked, this, [this]() {
            togglePCScoutSolution();
        });
        connect(playPCCancelButton_, &QPushButton::clicked, this, [this]() { cancelPCScout(); });
        connect(playSpinScoutButton_, &QPushButton::clicked, this, [this]() {
            runAuxiliaryScout("spin");
        });
        connect(playRenScoutButton_, &QPushButton::clicked, this, [this]() {
            runAuxiliaryScout("ren");
        });
        connect(playSpinScoutCancelButton_, &QPushButton::clicked, this, [this]() {
            cancelAuxiliaryScout();
        });
        connect(playRenScoutCancelButton_, &QPushButton::clicked, this, [this]() {
            cancelAuxiliaryScout();
        });
        connect(playSpinScoutPreviewButton_, &QPushButton::clicked, this, [this]() {
            previewAuxiliaryScoutResult("spin");
        });
        connect(playRenScoutPreviewButton_, &QPushButton::clicked, this, [this]() {
            previewAuxiliaryScoutResult("ren");
        });
        connect(playSpinScoutAutoCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
            QSettings settings;
            settings.setValue("play/spinScout/automatic", enabled);
            if (!enabled && auxiliaryScoutAutomaticRun_ && auxiliaryScoutMode_ == "spin") {
                cancelAuxiliaryScout();
            }
            scheduleAuxiliaryScouts(true);
        });
        connect(playRenScoutAutoCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
            QSettings settings;
            settings.setValue("play/renScout/automatic", enabled);
            if (!enabled && auxiliaryScoutAutomaticRun_ && auxiliaryScoutMode_ == "ren") {
                cancelAuxiliaryScout();
            }
            scheduleAuxiliaryScouts(true);
        });
        connect(playSpinScoutLinesBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            scheduleAuxiliaryScouts(true);
        });
        connect(playSpinScoutPiecesSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this]() {
            scheduleAuxiliaryScouts(true);
        });
        connect(playRenScoutDropBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            scheduleAuxiliaryScouts(true);
        });
        connect(playRenScoutPiecesSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this]() {
            scheduleAuxiliaryScouts(true);
        });
        connect(playPCEnabledCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
            QSettings settings;
            settings.setValue("play/pcScout/enabled", enabled);
            if (enabled) {
                schedulePCScout(true);
            } else {
                invalidatePCScout("PC Scout is off");
            }
            updatePCScoutControls();
        });
        connect(playPCSourceBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            QSettings settings;
            settings.setValue("play/pcScout/source", playPCSourceBox_->currentData().toString());
            schedulePCScout(true);
        });
        connect(playPCDropBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            QSettings settings;
            settings.setValue("play/pcScout/drop", playPCDropBox_->currentData().toString());
            schedulePCScout(true);
        });
        {
            QSettings settings;
            playPCEnabledCheck_->setChecked(settings.value("play/pcScout/enabled", false).toBool());
            const QString storedSource = settings.value("play/pcScout/source", "active").toString();
            const int sourceIndex = playPCSourceBox_->findData(storedSource);
            playPCSourceBox_->setCurrentIndex(sourceIndex >= 0 ? sourceIndex : 0);
            const QString storedDrop = settings.value("play/pcScout/drop", "softdrop").toString();
            const int storedIndex = playPCDropBox_->findData(storedDrop);
            playPCDropBox_->setCurrentIndex(storedIndex >= 0 ? storedIndex : 1);
            playSpinScoutAutoCheck_->setChecked(
                settings.value("play/spinScout/automatic", false).toBool());
            playRenScoutAutoCheck_->setChecked(
                settings.value("play/renScout/automatic", false).toBool());
        }
        pcScoutRefreshTimer_ = new QTimer(page);
        pcScoutRefreshTimer_->setSingleShot(true);
        connect(pcScoutRefreshTimer_, &QTimer::timeout, this, [this]() { runPCScout(); });
        auxiliaryScoutRefreshTimer_ = new QTimer(page);
        auxiliaryScoutRefreshTimer_->setSingleShot(true);
        connect(auxiliaryScoutRefreshTimer_, &QTimer::timeout, this, [this]() {
            runScheduledAuxiliaryScout();
        });
        updatePCScoutControls();
        scheduleAuxiliaryScouts();

        playTimer_ = new QTimer(page);
        playTimer_->setTimerType(Qt::PreciseTimer);
        playTimer_->setInterval(16);
        connect(playTimer_, &QTimer::timeout, this, [this]() { advancePlayFrame(); });
        playInputClock_.start();
        playInputTimer_ = new QTimer(page);
        playInputTimer_->setSingleShot(true);
        playInputTimer_->setTimerType(Qt::PreciseTimer);
        connect(playInputTimer_, &QTimer::timeout, this, [this]() {
            processPlayInputDeadlines();
        });
        playFumenSyncTimer_ = new QTimer(page);
        playFumenSyncTimer_->setSingleShot(true);
        connect(playFumenSyncTimer_, &QTimer::timeout, this, [this]() {
            updateFumenCodeFromPages();
        });

        scroll->setWidget(page);
        updatePiecePreviewSizes();
        initializePlayGame();
        return scroll;
    }

    QWidget *buildCenterOutputPage(QWidget *parent) {
        auto *page = new QWidget(parent);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        auto *toolbar = new QHBoxLayout();
        outputFileBox_ = new QComboBox(page);
        outputFileBox_->addItem("Command Output", "__command_output__");
        auto *refreshButton = new QPushButton("Refresh", page);
        toolbar->addWidget(new QLabel("Generated File", page));
        toolbar->addWidget(outputFileBox_, 1);
        toolbar->addWidget(refreshButton);
        layout->addLayout(toolbar);

        centerOutputBrowser_ = new QTextBrowser(page);
        centerOutputBrowser_->setOpenExternalLinks(false);
        centerOutputBrowser_->setOpenLinks(false);
        centerOutputBrowser_->setStyleSheet("font-family: 'Menlo', 'SF Mono', 'DejaVu Sans Mono', monospace; font-size: 13px;");
        layout->addWidget(centerOutputBrowser_, 1);

        connect(refreshButton, &QPushButton::clicked, this, [this]() { refreshGeneratedFiles(); });
        connect(outputFileBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { showSelectedGeneratedFile(); });
        connect(centerOutputBrowser_, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
            openFumenLinkInPreview(url);
        });
        return page;
    }

    QWidget *buildPreviewPage(QWidget *parent) {
        auto *page = new QWidget(parent);
        auto *layout = new QHBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);

        previewBoard_ = new BoardWidget(page);
        previewBoard_->onCellPressed = [](int) { return true; };
        layout->addWidget(previewBoard_, 1);

        auto *side = new QWidget(page);
        side->setMinimumWidth(240);
        side->setMaximumWidth(300);
        auto *sideLayout = new QVBoxLayout(side);
        sideLayout->setContentsMargins(0, 0, 0, 0);
        sideLayout->setSpacing(10);
        previewCodeBox_ = new QComboBox(side);
        previewPageLabel_ = new QLabel("1/1", side);
        previewPageLabel_->setAlignment(Qt::AlignCenter);
        auto *prevButton = new QPushButton("Previous", side);
        auto *nextButton = new QPushButton("Next", side);
        auto *loadCurrentButton = new QPushButton("Load Editor Code", side);
        auto *sendButton = new QPushButton("Send To Editor", side);
        sideLayout->addWidget(new QLabel("Fumen Preview", side));
        sideLayout->addWidget(previewCodeBox_);
        auto *navRow = new QHBoxLayout();
        navRow->addWidget(prevButton);
        navRow->addWidget(previewPageLabel_);
        navRow->addWidget(nextButton);
        sideLayout->addLayout(navRow);
        sideLayout->addWidget(loadCurrentButton);
        sideLayout->addWidget(sendButton);
        sideLayout->addStretch(1);
        layout->addWidget(side, 0);

        connect(previewCodeBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            loadPreviewCode(previewCodeBox_->currentText());
        });
        connect(prevButton, &QPushButton::clicked, this, [this]() {
            goToPreviewPage(currentPreviewPage_ - 1);
        });
        connect(nextButton, &QPushButton::clicked, this, [this]() {
            goToPreviewPage(currentPreviewPage_ + 1);
        });
        connect(loadCurrentButton, &QPushButton::clicked, this, [this]() {
            loadPreviewCode(fumenEdit_->toPlainText().trimmed());
        });
        connect(sendButton, &QPushButton::clicked, this, [this]() {
            sendPreviewToEditor();
        });

        loadPreviewCode(fumenEdit_ ? fumenEdit_->toPlainText().trimmed() : QString());
        return page;
    }

    QWidget *makePlaceholderPage(const QString &title, const QString &body, QWidget *parent) {
        auto *page = new QWidget(parent);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(24, 24, 24, 24);
        auto *titleLabel = new QLabel(title, page);
        titleLabel->setObjectName("paneTitle");
        auto *bodyLabel = new QLabel(body, page);
        bodyLabel->setObjectName("paneSubtitle");
        bodyLabel->setWordWrap(true);
        layout->addWidget(titleLabel);
        layout->addWidget(bodyLabel);
        layout->addStretch(1);
        return page;
    }

    QWidget *buildOutputPanel() {
        auto *panel = new QWidget(this);
        panel->setMinimumWidth(260);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);

        auto *header = new QHBoxLayout();
        auto *title = new QLabel("Command Output", panel);
        title->setObjectName("paneTitle");
        verboseCheck_ = new QCheckBox("Verbose", panel);
        auto *clearButton = new QPushButton("Clear", panel);
        header->addWidget(title);
        header->addStretch(1);
        header->addWidget(verboseCheck_);
        header->addWidget(clearButton);
        layout->addLayout(header);

        outputEdit_ = new QPlainTextEdit(panel);
        outputEdit_->setReadOnly(true);
        outputEdit_->setPlaceholderText("Run a search to see output here.");
        outputEdit_->setStyleSheet("font-family: 'Menlo', 'SF Mono', 'DejaVu Sans Mono', monospace; font-size: 13px;");
        layout->addWidget(outputEdit_, 1);

        auto *filesGroup = new QGroupBox("Generated Files", panel);
        auto *filesLayout = new QVBoxLayout(filesGroup);
        filesLayout->setSpacing(8);

        auto *filesHeader = new QHBoxLayout();
        auto *refreshButton = new QPushButton("Refresh", filesGroup);
        filesHeader->addStretch(1);
        filesHeader->addWidget(refreshButton);
        filesLayout->addLayout(filesHeader);

        outputFilesList_ = new QListWidget(filesGroup);
        outputFilesList_->setMinimumHeight(92);
        filesLayout->addWidget(outputFilesList_, 1);

        auto *fileActions = new QHBoxLayout();
        auto *previewButton = new QPushButton("Preview", filesGroup);
        auto *openButton = new QPushButton("Open", filesGroup);
        auto *openFolderButton = new QPushButton("Open Folder", filesGroup);
        fileActions->addWidget(previewButton);
        fileActions->addWidget(openButton);
        fileActions->addWidget(openFolderButton);
        filesLayout->addLayout(fileActions);
        layout->addWidget(filesGroup, 0);

        connect(verboseCheck_, &QCheckBox::toggled, this, [this]() {
            refreshDisplayedOutput();
        });
        connect(clearButton, &QPushButton::clicked, this, [this]() {
            showingOutputFileContent_ = false;
            rawOutputLog_.clear();
            refreshDisplayedOutput();
        });
        connect(refreshButton, &QPushButton::clicked, this, [this]() {
            refreshGeneratedFiles();
        });
        connect(previewButton, &QPushButton::clicked, this, [this]() {
            previewSelectedOutputFile();
        });
        connect(openButton, &QPushButton::clicked, this, [this]() {
            openSelectedOutputFile();
        });
        connect(openFolderButton, &QPushButton::clicked, this, [this]() {
            openOutputFolder();
        });
        connect(outputFilesList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
            openSelectedOutputFile();
        });

        return panel;
    }

    void ensureFumenState() {
        if (!fumenPages_.empty()) {
            return;
        }
        std::array<int, kFumenBlocks> blank{};
        blank.fill(0);
        fumenPages_.push_back(blank);
        fumenOperations_.push_back(FumenOperation());
        currentFumenPage_ = 0;
        currentOperation_ = FumenOperation();
    }

    std::array<int, kColumns * kRows> visibleCellsForPage() const {
        std::array<int, kColumns * kRows> visible{};
        visible.fill(0);
        if (fumenPages_.empty()) {
            return visible;
        }
        std::array<int, kFumenBlocks> page = fumenPages_[currentFumenPage_];
        if (placeMinoCheck_ && placeMinoCheck_->isChecked() && currentOperation_.type > 0) {
            for (int index : fumenOperationCells(currentOperation_)) {
                if (0 <= index && index < kFumenBlocks) {
                    page[index] = currentOperation_.type;
                }
            }
        }
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                const int source = (kVisibleTopRow + row) * kColumns + col;
                visible[row * kColumns + col] = qBound(0, page[source], 8);
            }
        }
        return visible;
    }

    void syncCurrentPageFromBoard() {
        ensureFumenState();
        const auto &visible = board_->cells();
        std::array<int, kFumenBlocks> &page = fumenPages_[currentFumenPage_];
        std::vector<int> protectedCells;
        if (placeMinoCheck_ && placeMinoCheck_->isChecked() && currentOperation_.type > 0) {
            protectedCells = fumenOperationCells(currentOperation_);
        }
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                const int source = (kVisibleTopRow + row) * kColumns + col;
                if (std::find(protectedCells.begin(), protectedCells.end(), source) == protectedCells.end()) {
                    page[source] = visible[row * kColumns + col];
                }
            }
        }
        for (int index = 230; index < kFumenBlocks; ++index) {
            page[index] = 0;
        }
        saveCurrentFumenPage();
        updatePageControls();
    }

    void saveCurrentFumenPage() {
        ensureFumenState();
        if (currentFumenPage_ < 0 || currentFumenPage_ >= static_cast<int>(fumenPages_.size())) {
            return;
        }
        for (int index = 230; index < kFumenBlocks; ++index) {
            fumenPages_[currentFumenPage_][index] = 0;
        }
        if (currentFumenPage_ >= static_cast<int>(fumenOperations_.size())) {
            fumenOperations_.resize(fumenPages_.size());
        }
        fumenOperations_[currentFumenPage_] = (placeMinoCheck_ && placeMinoCheck_->isChecked()) ? currentOperation_ : FumenOperation();
    }

    void replaceFumenPages(const std::vector<std::array<int, kFumenBlocks>> &pages,
                           const std::vector<FumenOperation> &operations) {
        fumenPages_ = pages;
        if (fumenPages_.empty()) {
            std::array<int, kFumenBlocks> blank{};
            blank.fill(0);
            fumenPages_.push_back(blank);
        }
        for (auto &page : fumenPages_) {
            for (int index = 230; index < kFumenBlocks; ++index) {
                page[index] = 0;
            }
        }
        fumenOperations_ = operations;
        fumenOperations_.resize(fumenPages_.size());
        currentFumenPage_ = 0;
        currentOperation_ = fumenOperations_[currentFumenPage_];
        if (placeMinoCheck_) {
            updatingFumenControls_ = true;
            placeMinoCheck_->setChecked(currentOperation_.type > 0);
            updatingFumenControls_ = false;
        }
        updateMinoControls();
        updateBoardFromFumenState();
    }

    void resetToEmptyFumen() {
        std::array<int, kFumenBlocks> blank{};
        blank.fill(0);
        fumenPages_ = {blank};
        fumenOperations_ = {FumenOperation()};
        currentFumenPage_ = 0;
        currentOperation_ = FumenOperation();
        if (placeMinoCheck_) {
            updatingFumenControls_ = true;
            placeMinoCheck_->setChecked(false);
            updatingFumenControls_ = false;
        }
        updateMinoControls();
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
    }

    void updateBoardFromFumenState() {
        ensureFumenState();
        if (board_) {
            board_->setCells(visibleCellsForPage());
        }
        updateGeneratedField();
        updatePageControls();
    }

    bool handleBoardCellPressed(int visibleIndex) {
        if (!placeMinoCheck_ || !placeMinoCheck_->isChecked()) {
            return false;
        }
        const int visibleRow = visibleIndex / kColumns;
        const int col = visibleIndex % kColumns;
        currentOperation_.type = minoPieceBox_ ? minoPieceBox_->currentData().toInt() : qMax(1, currentOperation_.type);
        currentOperation_.position = (kVisibleTopRow + visibleRow) * kColumns + col;
        saveCurrentFumenPage();
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
        return true;
    }

    void updateMinoControls() {
        if (!minoPieceBox_) {
            return;
        }
        const int type = currentOperation_.type > 0 ? currentOperation_.type : 1;
        updatingFumenControls_ = true;
        for (int i = 0; i < minoPieceBox_->count(); ++i) {
            if (minoPieceBox_->itemData(i).toInt() == type) {
                minoPieceBox_->setCurrentIndex(i);
                break;
            }
        }
        updatingFumenControls_ = false;
    }

    void updatePageControls() {
        ensureFumenState();
        if (pageLabel_) {
            pageLabel_->setText(QString("%1/%2").arg(currentFumenPage_ + 1).arg(fumenPages_.size()));
        }
        if (prevPageButton_) {
            prevPageButton_->setEnabled(currentFumenPage_ > 0);
        }
        if (nextPageButton_) {
            nextPageButton_->setEnabled(currentFumenPage_ + 1 < static_cast<int>(fumenPages_.size()));
        }
        if (trimBeforePagesButton_) {
            trimBeforePagesButton_->setEnabled(currentFumenPage_ > 0);
        }
        if (trimPagesButton_) {
            trimPagesButton_->setEnabled(currentFumenPage_ + 1 < static_cast<int>(fumenPages_.size()));
        }
    }

    bool commandSupportsHoldDropKicks(const QString &command) const {
        return command != "spin";
    }

    bool commandSupportsLines(const QString &command) const {
        return command == "percent" || command == "path" || command == "tetris" || command == "tetris-path"
            || command == "setup" || command == "spin";
    }

    void setFormRowVisible(QWidget *field, bool visible) {
        if (!field) {
            return;
        }
        field->setVisible(visible);
        if (searchForm_) {
            if (QWidget *label = searchForm_->labelForField(field)) {
                label->setVisible(visible);
            }
        }
    }

    int autoCommandHeight() const {
        if (!board_) {
            return 1;
        }
        const auto &cells = board_->cells();
        int topOccupied = kRows;
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                if (cells[row * kColumns + col] != 0) {
                    topOccupied = qMin(topOccupied, row);
                }
            }
        }
        return topOccupied == kRows ? 1 : kRows - topOccupied;
    }

    void updateAutoHeightFields() {
        const int height = autoCommandHeight();
        if (spinHeightEdit_) {
            spinHeightEdit_->setText(QString::number(height));
        }
        if (commandBox_ && commandBox_->currentText() == "setup" && linesSpin_) {
            linesSpin_->setValue(qMin(12, height));
        }
    }

    void convertFumenToSetupGray() {
        if (!board_) {
            return;
        }
        syncCurrentPageFromBoard();
        ensureFumenState();
        for (auto &page : fumenPages_) {
            for (int index = 0; index < kFumenBlocks; ++index) {
                if (page[index] != 0 && page[index] != 8) {
                    page[index] = 8;
                }
            }
        }
        currentOperation_ = FumenOperation();
        if (placeMinoCheck_) {
            updatingFumenControls_ = true;
            placeMinoCheck_->setChecked(false);
            updatingFumenControls_ = false;
        }
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
    }

    void updateCommandUi() {
        if (!commandBox_) {
            return;
        }
        const QString command = commandBox_->currentText();
        const bool setupMode = command == "setup";
        const bool spinMode = command == "spin";
        const bool supportsHoldDrop = commandSupportsHoldDropKicks(command);
        const bool supportsLines = commandSupportsLines(command);

        setFormRowVisible(holdBox_, supportsHoldDrop);
        setFormRowVisible(dropBox_, supportsHoldDrop);
        setFormRowVisible(linesSpin_, supportsLines);
        setFormRowVisible(spinHeightEdit_, spinMode);
        if (QWidget *label = searchForm_ ? searchForm_->labelForField(linesSpin_) : nullptr) {
            auto *labelWidget = qobject_cast<QLabel *>(label);
            if (labelWidget) {
                if (setupMode) {
                    labelWidget->setText("Auto height");
                } else if (spinMode) {
                    labelWidget->setText("T lines");
                } else {
                    labelWidget->setText("Lines");
                }
            }
        }
        linesSpin_->setEnabled(!setupMode);
        linesSpin_->setRange(spinMode ? 1 : 1, spinMode ? 3 : 20);
        if (spinMode && linesSpin_->value() > 3) {
            linesSpin_->setValue(2);
        }
        if (setupMode && !setupModeActive_) {
            convertFumenToSetupGray();
        }
        setupModeActive_ = setupMode;
        if (setupMode) {
            if (board_ && board_->paintValue() != 1 && board_->paintValue() != 3 && board_->paintValue() != 8) {
                selectPaint(8);
            }
        }
        updateAutoHeightFields();

        for (int i = 0; i < static_cast<int>(paintButtons_.size()); ++i) {
            const int value = i < static_cast<int>(paintValues_.size()) ? paintValues_[i] : 0;
            paintButtons_[i]->setVisible(!setupMode || value == 1 || value == 3 || value == 8);
        }
        if (minoGroup_) {
            minoGroup_->setEnabled(!setupMode);
        }
        if (pagesGroup_) {
            pagesGroup_->setEnabled(!setupMode);
        }
        updatePageControls();
    }

    void rotateCurrentOperation(int delta) {
        if (!placeMinoCheck_) {
            return;
        }
        placeMinoCheck_->setChecked(true);
        currentOperation_.type = minoPieceBox_ ? minoPieceBox_->currentData().toInt() : qMax(1, currentOperation_.type);
        currentOperation_.rotation = (currentOperation_.rotation + delta + 4) % 4;
        saveCurrentFumenPage();
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
    }

    void moveCurrentOperation(int dx, int dy) {
        if (!placeMinoCheck_) {
            return;
        }
        placeMinoCheck_->setChecked(true);
        currentOperation_.type = minoPieceBox_ ? minoPieceBox_->currentData().toInt() : qMax(1, currentOperation_.type);
        const int x = currentOperation_.position % kColumns;
        const int y = currentOperation_.position / kColumns;
        const int nextX = qBound(0, x + dx, kColumns - 1);
        const int nextY = qBound(kVisibleTopRow, y + dy, kVisibleBottomRow - 1);
        currentOperation_.position = nextY * kColumns + nextX;
        saveCurrentFumenPage();
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
    }

    void clearCurrentOperation() {
        currentOperation_ = FumenOperation();
        if (placeMinoCheck_) {
            updatingFumenControls_ = true;
            placeMinoCheck_->setChecked(false);
            updatingFumenControls_ = false;
        }
        saveCurrentFumenPage();
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
    }

    std::array<int, kFumenBlocks> currentPageAfterLockAndLineClear() {
        ensureFumenState();
        std::array<int, kFumenBlocks> cells = fumenPages_[currentFumenPage_];
        if (placeMinoCheck_ && placeMinoCheck_->isChecked() && currentOperation_.type > 0) {
            for (int index : fumenOperationCells(currentOperation_)) {
                if (0 <= index && index < kFumenBlocks) {
                    cells[index] = currentOperation_.type;
                }
            }
        }
        std::array<int, kFumenBlocks> result{};
        result.fill(0);
        int writeRow = kFumenRows - 2;
        for (int readRow = kFumenRows - 2; readRow >= 0; --readRow) {
            bool full = true;
            for (int col = 0; col < kColumns; ++col) {
                if (cells[readRow * kColumns + col] == 0) {
                    full = false;
                    break;
                }
            }
            if (!full) {
                for (int col = 0; col < kColumns; ++col) {
                    result[writeRow * kColumns + col] = cells[readRow * kColumns + col];
                }
                --writeRow;
            }
        }
        for (int index = 230; index < kFumenBlocks; ++index) {
            result[index] = 0;
        }
        return result;
    }

    void addFumenPage() {
        ensureFumenState();
        syncCurrentPageFromBoard();
        const int insertIndex = currentFumenPage_ + 1;
        fumenPages_.insert(fumenPages_.begin() + insertIndex, currentPageAfterLockAndLineClear());
        fumenOperations_.insert(fumenOperations_.begin() + insertIndex, FumenOperation());
        goToFumenPage(insertIndex);
        updateFumenCodeFromPages();
    }

    void goToFumenPage(int page) {
        ensureFumenState();
        if (page < 0 || page >= static_cast<int>(fumenPages_.size())) {
            return;
        }
        syncCurrentPageFromBoard();
        currentFumenPage_ = page;
        currentOperation_ = fumenOperations_[currentFumenPage_];
        if (placeMinoCheck_) {
            updatingFumenControls_ = true;
            placeMinoCheck_->setChecked(currentOperation_.type > 0);
            updatingFumenControls_ = false;
        }
        updateMinoControls();
        updateBoardFromFumenState();
    }

    void trimFollowingFumenPages() {
        ensureFumenState();
        syncCurrentPageFromBoard();
        if (currentFumenPage_ + 1 >= static_cast<int>(fumenPages_.size())) {
            return;
        }
        fumenPages_.erase(fumenPages_.begin() + currentFumenPage_ + 1, fumenPages_.end());
        fumenOperations_.erase(fumenOperations_.begin() + currentFumenPage_ + 1, fumenOperations_.end());
        updatePageControls();
        updateFumenCodeFromPages();
    }

    void trimPreviousFumenPages() {
        ensureFumenState();
        syncCurrentPageFromBoard();
        if (currentFumenPage_ <= 0) {
            return;
        }
        fumenPages_.erase(fumenPages_.begin(), fumenPages_.begin() + currentFumenPage_);
        fumenOperations_.erase(fumenOperations_.begin(), fumenOperations_.begin() + currentFumenPage_);
        currentFumenPage_ = 0;
        currentOperation_ = fumenOperations_[currentFumenPage_];
        if (placeMinoCheck_) {
            updatingFumenControls_ = true;
            placeMinoCheck_->setChecked(currentOperation_.type > 0);
            updatingFumenControls_ = false;
        }
        updateMinoControls();
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
    }

    void clearCurrentPage() {
        ensureFumenState();
        fumenPages_[currentFumenPage_].fill(0);
        currentOperation_ = FumenOperation();
        if (placeMinoCheck_) {
            updatingFumenControls_ = true;
            placeMinoCheck_->setChecked(false);
            updatingFumenControls_ = false;
        }
        saveCurrentFumenPage();
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
    }

    void mirrorCurrentPage() {
        ensureFumenState();
        std::array<int, kFumenBlocks> mirrored{};
        mirrored.fill(0);
        const auto &source = fumenPages_[currentFumenPage_];
        for (int row = 0; row < kFumenRows; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                mirrored[row * kColumns + (kColumns - 1 - col)] = mirrorColor(source[row * kColumns + col]);
            }
        }
        fumenPages_[currentFumenPage_] = mirrored;
        if (currentOperation_.type > 0) {
            currentOperation_.type = mirrorColor(currentOperation_.type);
            currentOperation_.position = (currentOperation_.position / kColumns) * kColumns + (kColumns - 1 - (currentOperation_.position % kColumns));
        }
        saveCurrentFumenPage();
        updateMinoControls();
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
    }

    QString encodeFumenPages() {
        ensureFumenState();
        std::vector<std::array<int, kFumenBlocks>> pages = fumenPages_;
        if (currentFumenPage_ >= 0 && currentFumenPage_ < static_cast<int>(pages.size()) &&
            placeMinoCheck_ && placeMinoCheck_->isChecked() && currentOperation_.type > 0) {
            for (int index : fumenOperationCells(currentOperation_)) {
                if (0 <= index && index < kFumenBlocks) {
                    pages[currentFumenPage_][index] = currentOperation_.type;
                }
            }
        }
        return encodeFumenFields(pages);
    }

    void updateFumenCodeFromPages() {
        if (!fumenEdit_ || updatingFumenEdit_) {
            return;
        }
        updatingFumenEdit_ = true;
        fumenEdit_->setPlainText(encodeFumenPages());
        updatingFumenEdit_ = false;
    }

    void loadFumenCodeFromText() {
        if (updatingFumenEdit_ || !fumenEdit_) {
            return;
        }
        const QString code = fumenEdit_->toPlainText().trimmed();
        if (!code.startsWith("v115@")) {
            return;
        }
        const auto decoded = decodeFumenV115(code);
        if (!decoded.has_value()) {
            return;
        }
        replaceFumenPages(decoded->pages, decoded->operations);
        if (commandBox_ && commandBox_->currentText() == "setup") {
            convertFumenToSetupGray();
        }
        updateGeneratedField();
    }

    std::array<int, kColumns * kRows> visibleCellsFromFumenPage(const std::vector<std::array<int, kFumenBlocks>> &pages,
                                                                 const std::vector<FumenOperation> &operations,
                                                                 int page) const {
        std::array<int, kColumns * kRows> visible{};
        visible.fill(0);
        if (page < 0 || page >= static_cast<int>(pages.size())) {
            return visible;
        }
        std::array<int, kFumenBlocks> field = pages[page];
        if (page < static_cast<int>(operations.size()) && operations[page].type > 0) {
            for (int index : fumenOperationCells(operations[page])) {
                if (0 <= index && index < kFumenBlocks) {
                    field[index] = operations[page].type;
                }
            }
        }
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                visible[row * kColumns + col] = qBound(0, field[(kVisibleTopRow + row) * kColumns + col], 8);
            }
        }
        return visible;
    }

    QString formatByteSize(qint64 size) const {
        if (size >= 1024 * 1024) {
            return QString::number(size / (1024.0 * 1024.0), 'f', 1) + " MB";
        }
        if (size >= 1024) {
            return QString::number(size / 1024.0, 'f', 1) + " KB";
        }
        return QString::number(size) + " bytes";
    }

    QFileInfoList discoverGeneratedFiles() const {
        QFileInfoList discovered;
        const QStringList roots = {
            QDir(appDataDir()).filePath("run"),
            repoRoot_ + "/output",
            repoRoot_ + "/solution-finder-1.43/output",
            repoRoot_ + "/native-macos/output"
        };
        for (const QString &root : roots) {
            QDir dir(root);
            if (!dir.exists()) {
                continue;
            }
            QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QFileInfo file(it.next());
                const QString name = file.fileName();
                const QString suffix = file.suffix().toLower();
                if (name == "field.txt" || name == "patterns.txt") {
                    continue;
                }
                if (name.startsWith("qt_output") || suffix == "html" || suffix == "csv" || suffix == "txt" || suffix == "log") {
                    discovered.push_back(file);
                }
            }
        }
        std::sort(discovered.begin(), discovered.end(), [](const QFileInfo &lhs, const QFileInfo &rhs) {
            return lhs.lastModified() > rhs.lastModified();
        });
        return discovered;
    }

    void refreshGeneratedFiles() {
        const QString current = outputFileBox_ ? outputFileBox_->currentData().toString() : QString();
        if (outputFileBox_) {
            outputFileBox_->blockSignals(true);
            outputFileBox_->clear();
            outputFileBox_->addItem("Command Output", "__command_output__");
        }
        if (outputFilesList_) {
            outputFilesList_->clear();
        }

        const QFileInfoList discovered = discoverGeneratedFiles();
        QStringList seenPaths;
        for (const QFileInfo &file : discovered) {
            const QString path = file.absoluteFilePath();
            if (seenPaths.contains(path)) {
                continue;
            }
            seenPaths << path;
            if (outputFileBox_) {
                outputFileBox_->addItem(file.fileName(), path);
            }
            if (outputFilesList_) {
                auto *item = new QListWidgetItem(file.fileName() + "\n" + formatByteSize(file.size()), outputFilesList_);
                item->setData(Qt::UserRole, path);
                item->setToolTip(path);
            }
        }
        if (outputFilesList_ && outputFilesList_->count() > 0 && !outputFilesList_->currentItem()) {
            outputFilesList_->setCurrentRow(0);
        }

        if (outputFileBox_) {
            const int index = outputFileBox_->findData(current);
            if (index >= 0) {
                outputFileBox_->setCurrentIndex(index);
            }
            outputFileBox_->blockSignals(false);
            showSelectedGeneratedFile();
        }
    }

    void showSelectedGeneratedFile() {
        if (!centerOutputBrowser_ || !outputFileBox_) {
            return;
        }
        const QString path = outputFileBox_->currentData().toString();
        if (path == "__command_output__" || path.isEmpty()) {
            centerOutputBrowser_->setPlainText(displayedOutputText().isEmpty() ? "Run a command, then click Refresh." : displayedOutputText());
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            centerOutputBrowser_->setPlainText("Could not open " + path);
            return;
        }
        const QString content = QString::fromUtf8(file.readAll());
        if (path.endsWith(".html", Qt::CaseInsensitive) || content.trimmed().startsWith("<")) {
            centerOutputBrowser_->setHtml(themedOutputHtml(content));
        } else {
            centerOutputBrowser_->setPlainText(content);
        }
        extractPreviewCodes(content);
    }

    QString themedOutputHtml(const QString &content) const {
        const QString css = R"(
<style>
html, body {
    background: #1f1f1f !important;
    color: #eeeeee !important;
    font-family: Menlo, "SF Mono", "DejaVu Sans Mono", monospace !important;
    font-size: 13px !important;
    line-height: 1.45 !important;
}
a { color: #8ab4ff !important; }
a:visited { color: #c7a8ff !important; }
table, th, td { border-color: #555555 !important; color: #eeeeee !important; }
th { background: #303030 !important; }
td { background: #242424 !important; }
pre, code {
    color: #f0f0f0 !important;
    background: #171717 !important;
    font-family: Menlo, "SF Mono", "DejaVu Sans Mono", monospace !important;
}
</style>
)";
        QString html = content;
        const int headClose = html.indexOf("</head>", 0, Qt::CaseInsensitive);
        if (headClose >= 0) {
            html.insert(headClose, css);
            return html;
        }
        const int bodyOpen = html.indexOf("<body", 0, Qt::CaseInsensitive);
        if (bodyOpen >= 0) {
            return css + html;
        }
        if (html.trimmed().startsWith("<")) {
            return "<html><head>" + css + "</head><body>" + html + "</body></html>";
        }
        return "<html><head>" + css + "</head><body><pre>" + html.toHtmlEscaped() + "</pre></body></html>";
    }

    QString selectedOutputFilePath() const {
        if (!outputFilesList_ || !outputFilesList_->currentItem()) {
            return QString();
        }
        return outputFilesList_->currentItem()->data(Qt::UserRole).toString();
    }

    void previewSelectedOutputFile() {
        const QString path = selectedOutputFilePath();
        if (path.isEmpty()) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            rawOutputLog_ = "Could not open " + path + "\n";
            refreshDisplayedOutput();
            return;
        }
        showingOutputFileContent_ = true;
        rawOutputLog_ = QString::fromUtf8(file.readAll());
        refreshDisplayedOutput();
    }

    void openSelectedOutputFile() {
        const QString path = selectedOutputFilePath();
        if (path.isEmpty()) {
            return;
        }
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == "html" || suffix == "htm") {
            if (outputFileBox_) {
                int index = outputFileBox_->findData(path);
                if (index < 0) {
                    outputFileBox_->addItem(QFileInfo(path).fileName(), path);
                    index = outputFileBox_->findData(path);
                }
                outputFileBox_->setCurrentIndex(index);
            }
            if (sectionTabs_) {
                sectionTabs_->setCurrentIndex(2);
            }
            showSelectedGeneratedFile();
            return;
        }
        if (suffix == "txt" || suffix == "csv" || suffix == "tsv" || suffix == "log" || suffix.isEmpty()) {
            previewSelectedOutputFile();
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }

    void openOutputFolder() {
        const QString folder = QDir(appDataDir()).filePath("run");
        QDir().mkpath(folder);
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    }

    bool isCompactResultLine(const QString &line) const {
        return line.startsWith("success = ")
            || line.startsWith("tetris = ")
            || line.startsWith("Found solutions = ")
            || line.startsWith("Found path ")
            || line.startsWith("Found path[")
            || line.startsWith("Found pattern ")
            || line == "success:"
            || line.startsWith("OR  = ")
            || line.startsWith("AND = ")
            || line == ">>>"
            || line.startsWith("Failed")
            || QRegularExpression("^\\d+(\\.\\d+)? % \\[\\d+/\\d+\\]:").match(line).hasMatch();
    }

    QString compactOutput(const QString &output) const {
        QStringList compact;
        bool includeSearchSection = false;
        bool includeOutputSection = false;
        const QStringList lines = output.split('\n');
        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();
            if (line.startsWith("Searching pattern size")) {
                compact << line;
                continue;
            }
            if (trimmed == "# Setup Field" || trimmed == "# Initialize / User-defined") {
                if (!compact.isEmpty() && !compact.last().isEmpty()) {
                    compact << "";
                }
                compact << line;
                continue;
            }
            if (isCompactResultLine(trimmed)) {
                compact << line;
                continue;
            }
            if (line == "# Search") {
                if (!compact.isEmpty() && !compact.last().isEmpty()) {
                    compact << "";
                }
                compact << line;
                includeSearchSection = true;
                includeOutputSection = false;
                continue;
            }
            if (line == "# Output") {
                if (!compact.isEmpty() && !compact.last().isEmpty()) {
                    compact << "";
                }
                compact << line;
                includeSearchSection = false;
                includeOutputSection = true;
                continue;
            }
            if (includeSearchSection) {
                if (line.startsWith("  -> Stopwatch")) {
                    compact << line;
                    continue;
                }
                if (line.startsWith("# ")) {
                    includeSearchSection = false;
                }
            }
            if (includeOutputSection) {
                if (line.startsWith("# ") || line.startsWith("Success pattern tree") || line.startsWith("Tetris-ending PC pattern tree") || line == "-------------------") {
                    includeOutputSection = false;
                } else {
                    compact << line;
                    continue;
                }
            }
        }
        return compact.join('\n').trimmed();
    }

    QString displayedOutputText() const {
        if (verboseCheck_ && verboseCheck_->isChecked()) {
            return rawOutputLog_;
        }
        if (showingOutputFileContent_) {
            return rawOutputLog_;
        }
        return compactOutput(rawOutputLog_);
    }

    void refreshDisplayedOutput() {
        const QString text = displayedOutputText();
        if (outputEdit_) {
            outputEdit_->setPlainText(text);
            outputEdit_->moveCursor(QTextCursor::End);
        }
        if (centerOutputBrowser_ && outputFileBox_ && outputFileBox_->currentData().toString() == "__command_output__") {
            centerOutputBrowser_->setPlainText(text.isEmpty() ? "Run a search to see output here." : text);
        }
    }

    void appendRawOutput(const QString &text) {
        showingOutputFileContent_ = false;
        rawOutputLog_ += text;
        refreshDisplayedOutput();
    }

    void extractPreviewCodes(const QString &content) {
        if (!previewCodeBox_) {
            return;
        }
        QRegularExpression regex("v115@[A-Za-z0-9+/\\?]+");
        QRegularExpressionMatchIterator it = regex.globalMatch(content);
        QStringList codes;
        while (it.hasNext()) {
            const QString code = it.next().captured(0);
            if (!codes.contains(code)) {
                codes << code;
            }
        }
        if (codes.isEmpty()) {
            return;
        }
        previewCodeBox_->blockSignals(true);
        previewCodeBox_->clear();
        for (const QString &code : codes) {
            previewCodeBox_->addItem(code);
        }
        previewCodeBox_->blockSignals(false);
        loadPreviewCode(codes.first());
    }

    void refreshPreviewCodes() {
        if (!previewCodeBox_) {
            return;
        }
        const QString code = fumenEdit_ ? fumenEdit_->toPlainText().trimmed() : QString();
        if (!code.isEmpty() && previewCodeBox_->findText(code) < 0) {
            previewCodeBox_->blockSignals(true);
            previewCodeBox_->insertItem(0, code);
            if (currentPreviewCode_.isEmpty()) {
                previewCodeBox_->setCurrentIndex(0);
            }
            previewCodeBox_->blockSignals(false);
        }
        if (!code.isEmpty() && currentPreviewCode_.isEmpty()) {
            loadPreviewCode(code);
        } else {
            updatePreviewBoard();
        }
    }

    QString extractFumenCode(const QString &candidate) const {
        const QString decoded = QString::fromUtf8(QByteArray::fromPercentEncoding(candidate.toUtf8()));
        const QStringList candidates = {candidate, decoded};
        const QRegularExpression regex("v115@[A-Za-z0-9+/\\?]+");
        for (const QString &text : candidates) {
            const QRegularExpressionMatch match = regex.match(text);
            if (match.hasMatch()) {
                return match.captured(0).trimmed();
            }
        }
        return QString();
    }

    QString fumenCodeFromUrl(const QUrl &url) const {
        const QStringList candidates = {
            url.toString(),
            url.toString(QUrl::FullyEncoded),
            QString::fromUtf8(QByteArray::fromPercentEncoding(url.toEncoded())),
            url.query(),
            url.fragment(),
            url.path()
        };
        for (const QString &candidate : candidates) {
            const QString code = extractFumenCode(candidate);
            if (code.startsWith("v115@") && code.size() > 6) {
                return code;
            }
        }
        return QString();
    }

    void openFumenLinkInPreview(const QUrl &url) {
        const QString code = fumenCodeFromUrl(url);
        if (code.isEmpty()) {
            appendRawOutput("\nNo fumen code found in link: " + url.toString() + "\n");
            return;
        }
        if (sectionTabs_) {
            sectionTabs_->setCurrentIndex(3);
        }
        if (previewCodeBox_) {
            previewCodeBox_->blockSignals(true);
            int index = previewCodeBox_->findText(code);
            if (index < 0) {
                previewCodeBox_->insertItem(0, code);
                index = 0;
            }
            previewCodeBox_->setCurrentIndex(index);
            previewCodeBox_->blockSignals(false);
        }
        loadPreviewCode(code);
    }

    void loadPreviewCode(QString code) {
        const int prefix = code.indexOf("v115@");
        if (prefix > 0) {
            code = code.mid(prefix);
        }
        if (!code.startsWith("v115@")) {
            return;
        }
        const auto decoded = decodeFumenV115(code);
        if (!decoded.has_value()) {
            return;
        }
        currentPreviewCode_ = code;
        previewPages_ = decoded->pages;
        previewOperations_ = decoded->operations;
        currentPreviewPage_ = 0;
        updatePreviewBoard();
    }

    void updatePreviewBoard() {
        if (!previewBoard_) {
            return;
        }
        previewBoard_->setCells(visibleCellsFromFumenPage(previewPages_, previewOperations_, currentPreviewPage_));
        if (previewPageLabel_) {
            const int count = qMax(1, static_cast<int>(previewPages_.size()));
            previewPageLabel_->setText(QString("%1/%2").arg(qMin(currentPreviewPage_ + 1, count)).arg(count));
        }
    }

    void goToPreviewPage(int page) {
        if (page < 0 || page >= static_cast<int>(previewPages_.size())) {
            return;
        }
        currentPreviewPage_ = page;
        updatePreviewBoard();
    }

    void sendPreviewToEditor() {
        if (previewPages_.empty()) {
            loadPreviewCode(previewCodeBox_ ? previewCodeBox_->currentText() : QString());
        }
        if (previewPages_.empty()) {
            std::array<int, kFumenBlocks> page{};
            page.fill(0);
            const auto visible = previewBoard_ ? previewBoard_->cells() : std::array<int, kColumns * kRows>{};
            for (int row = 0; row < kRows; ++row) {
                for (int col = 0; col < kColumns; ++col) {
                    page[(kVisibleTopRow + row) * kColumns + col] = visible[row * kColumns + col];
                }
            }
            previewPages_ = {page};
            previewOperations_ = {FumenOperation()};
        }
        replaceFumenPages(previewPages_, previewOperations_);
        if (!currentPreviewCode_.isEmpty() && fumenEdit_) {
            updatingFumenEdit_ = true;
            fumenEdit_->setPlainText(currentPreviewCode_);
            updatingFumenEdit_ = false;
        } else {
            updateFumenCodeFromPages();
        }
        if (outputEdit_) {
            outputEdit_->appendPlainText("Preview sent to editor.");
        }
    }

    int pieceTypeFromChar(QChar ch) const {
        switch (ch.toUpper().unicode()) {
        case 'I': return 1;
        case 'L': return 2;
        case 'O': return 3;
        case 'Z': return 4;
        case 'T': return 5;
        case 'J': return 6;
        case 'S': return 7;
        default: return 0;
        }
    }

    void updatePlayBoard() {
        if (!playBoard_) {
            return;
        }
        ++playRenderSerial_;
        std::array<int, kColumns * kRows> visible{};
        std::array<int, kColumns * kRows> ghosts{};
        visible.fill(0);
        ghosts.fill(0);
        std::array<int, SFT_GAME_WIDTH * SFT_GAME_HEIGHT> rendered{};
        std::array<int, SFT_GAME_WIDTH * SFT_GAME_HEIGHT> renderedGhosts{};
        sft_game_write_render_cells(&playGame_, rendered.data(), renderedGhosts.data());
        for (int row = 0; row < kRows; ++row) {
            const int gameY = kRows - 1 - row;
            for (int col = 0; col < kColumns; ++col) {
                const int index = row * kColumns + col;
                const int renderIndex = gameY * SFT_GAME_WIDTH + col;
                visible[index] = rendered[renderIndex];
                ghosts[index] = renderedGhosts[renderIndex];
            }
        }
        playBoard_->setRenderCells(visible, ghosts);
        if (playHoldPreview_) {
            playHoldPreview_->setPiece(playGame_.hold);
        }
        for (int i = 0; i < static_cast<int>(playNextPreviews_.size()); ++i) {
            if (playNextPreviews_[i]) {
                playNextPreviews_[i]->setPiece(sft_game_queue_piece(&playGame_, i));
            }
        }
        updatePlayStats();
    }

    bool playVisualStateChanged(const SFTGameState &before, const SFTGameState &after) const {
        return before.current != after.current
            || before.hold != after.hold
            || before.rotation != after.rotation
            || before.x != after.x
            || before.y != after.y
            || before.queue_index != after.queue_index
            || before.queue_count != after.queue_count
            || before.custom_queue_enabled != after.custom_queue_enabled
            || before.game_over != after.game_over
            || before.pieces_locked != after.pieces_locked
            || before.lines_cleared != after.lines_cleared
            || before.gravity_level != after.gravity_level
            || before.last_clear_lines != after.last_clear_lines
            || before.last_clear_t_spin != after.last_clear_t_spin
            || before.last_clear_t_spin_mini != after.last_clear_t_spin_mini
            || std::memcmp(before.board, after.board, sizeof(before.board)) != 0
            || std::memcmp(before.queue, after.queue, sizeof(before.queue)) != 0;
    }

    unsigned int randomPlaySeed() const {
        unsigned int seed = QRandomGenerator::global()->generate();
        return seed == 0 ? 1u : seed;
    }

    void initializePlayGame() {
        sft_game_init_seeded(&playGame_, randomPlaySeed());
        applyPlayTuning();
        applyPlayQueueAndHold(false);
        heldPlayInputs_.clear();
        playUndoStack_.clear();
        playHasStarted_ = false;
        playLastExportedPieces_ = playGame_.pieces_locked;
        playStatsClock_.invalidate();
        updatePlayTimerState();
        invalidatePCScout(
            playPCEnabledCheck_ && playPCEnabledCheck_->isChecked()
                ? "Ready to check the active queue"
                : "PC Scout is off");
        updatePlayBoard();
    }

    void resetPlayGame() {
        invalidatePCScout();
        invalidateAuxiliaryScout();
        sft_game_reset_seeded(&playGame_, randomPlaySeed());
        applyPlayTuning();
        applyPlayQueueAndHold(false);
        heldPlayInputs_.clear();
        playUndoStack_.clear();
        playHasStarted_ = false;
        playLastExportedPieces_ = playGame_.pieces_locked;
        playStatsClock_.invalidate();
        updatePlayTimerState();
        if (playStatusLabel_) {
            playStatusLabel_->clear();
        }
        updatePlayBoard();
        schedulePCScout();
        if (playBoard_) {
            playBoard_->setFocus();
        }
    }

    void applyPlayTuning() {
        if (!playGravityLevelBox_ || !playLockSpin_) {
            return;
        }
        sft_game_set_gravity_level(&playGame_, playConfiguredGravityLevel_);
        sft_game_set_level_progression(&playGame_,
                                       playLevelProgressionCheck_ && playLevelProgressionCheck_->isChecked());
        const bool usesProgressedLockDelay = playGame_.level_progression_enabled
            && playGame_.gravity_level >= 20
            && playGame_.gravity_level > playGame_.gravity_base_level;
        sft_game_set_lock_delay(&playGame_, usesProgressedLockDelay
            ? sft_game_lock_delay_for_level(playGame_.gravity_level)
            : playConfiguredLockDelay_);
        const int lockResetMode = playStepResetCheck_ && playStepResetCheck_->isChecked()
            ? SFT_LOCK_RESET_STEP
            : (playMoveResetCheck_ && playMoveResetCheck_->isChecked() ? SFT_LOCK_RESET_MOVE : SFT_LOCK_RESET_NONE);
        sft_game_set_lock_reset(&playGame_, lockResetMode,
                                playMoveResetLimitSpin_ ? playMoveResetLimitSpin_->value() : 15);
        sft_game_set_options(&playGame_,
                             playGravityCheck_ && playGravityCheck_->isChecked(),
                             playInfiniteLockCheck_ && playInfiniteLockCheck_->isChecked(),
                             playInfiniteHoldCheck_ && playInfiniteHoldCheck_->isChecked());
    }

    void loadEditorBoardIntoPlay() {
        invalidatePCScout();
        invalidateAuxiliaryScout();
        ensureFumenState();
        saveCurrentFumenPage();
        sft_game_init_seeded(&playGame_, randomPlaySeed());
        sft_game_load_fumen_cells(&playGame_, fumenPages_[currentFumenPage_].data());
        applyPlayTuning();
        applyPlayQueueAndHold(false);
        heldPlayInputs_.clear();
        playUndoStack_.clear();
        playHasStarted_ = false;
        playLastExportedPieces_ = playGame_.pieces_locked;
        playStatsClock_.invalidate();
        updatePlayTimerState();
        if (playStatusLabel_) {
            playStatusLabel_->setText("Loaded editor board into play.");
        }
        updatePlayBoard();
        playBoard_->setFocus();
    }

    void applyPlayQueueAndHold(bool resetStats = true) {
        if (!playQueueEdit_ || !playHoldBox_) {
            return;
        }
        std::vector<int> pieces;
        for (QChar ch : playQueueEdit_->text()) {
            const int piece = pieceTypeFromChar(ch);
            if (piece > 0) {
                pieces.push_back(piece);
            }
        }
        if (pieces.empty()) {
            sft_game_clear_custom_queue(&playGame_);
        } else {
            sft_game_set_custom_queue(&playGame_, pieces.data(), static_cast<int>(pieces.size()));
        }
        const int holdPiece = playHoldBox_->currentText() == "-"
            ? 0
            : pieceTypeFromChar(playHoldBox_->currentText().front());
        sft_game_set_hold_piece(&playGame_, holdPiece);
        applyPlayTuning();
        heldPlayInputs_.clear();
        playUndoStack_.clear();
        playLastExportedPieces_ = playGame_.pieces_locked;
        resetPlayOpeningDetection(playGame_.pieces_locked);
        if (resetStats) {
            playHasStarted_ = false;
            playStatsClock_.invalidate();
            updatePlayTimerState();
        }
        savePlaySettings();
        updatePlayBoard();
        invalidateAuxiliaryScout();
        schedulePCScout();
        if (playBoard_) {
            playBoard_->setFocus();
        }
    }

    void startPlayIfNeeded() {
        if (playHasStarted_) {
            return;
        }
        playHasStarted_ = true;
        playStatsClock_.start();
        updatePlayTimerState();
        if (playStatusLabel_) {
            playStatusLabel_->clear();
        }
    }

    void updatePlayTimerState() {
        if (!playTimer_) {
            return;
        }
        const bool shouldRun = playHasStarted_
            && sectionTabs_
            && sectionTabs_->currentIndex() == 1
            && !playGame_.game_over;
        if (shouldRun) {
            if (!playTimer_->isActive()) {
                playFrameClock_.restart();
                playTimer_->start();
            }
        } else {
            playTimer_->stop();
            playFrameClock_.invalidate();
            if (playInputTimer_) {
                playInputTimer_->stop();
            }
        }
    }

    void pushPlayUndo(const SFTGameState &state) {
        PlayUndoSnapshot snapshot;
        snapshot.game = state;
        snapshot.openingCycleStartPieces = playOpeningCycleStartPieces_;
        snapshot.openerDetectionDone = playOpenerDetectionDone_;
        snapshot.variantDetectionDone = playVariantDetectionDone_;
        snapshot.earlyVariantDetection = playEarlyVariantDetection_;
        snapshot.detectedOpenerName = playDetectedOpenerName_;
        snapshot.detectedOpenerMirrored = playDetectedOpenerMirrored_;
        snapshot.detectionLabel = playDetectionLabel_ ? playDetectionLabel_->text() : QString();
        playUndoStack_.push_back(snapshot);
        if (playUndoStack_.size() > 50) {
            playUndoStack_.erase(playUndoStack_.begin(), playUndoStack_.begin() + (playUndoStack_.size() - 50));
        }
        if (playUndoButton_) {
            playUndoButton_->setEnabled(true);
        }
    }

    void undoPlayPlacement() {
        if (playUndoStack_.empty()) {
            return;
        }
        invalidatePCScout();
        invalidateAuxiliaryScout();
        const PlayUndoSnapshot snapshot = playUndoStack_.back();
        playUndoStack_.pop_back();
        playGame_ = snapshot.game;
        playOpeningCycleStartPieces_ = snapshot.openingCycleStartPieces;
        playOpenerDetectionDone_ = snapshot.openerDetectionDone;
        playVariantDetectionDone_ = snapshot.variantDetectionDone;
        playEarlyVariantDetection_ = snapshot.earlyVariantDetection;
        playDetectedOpenerName_ = snapshot.detectedOpenerName;
        playDetectedOpenerMirrored_ = snapshot.detectedOpenerMirrored;
        if (playDetectionLabel_) {
            playDetectionLabel_->setText(snapshot.detectionLabel);
        }
        heldPlayInputs_.clear();
        schedulePlayInputTimer();
        playLastExportedPieces_ = playGame_.pieces_locked;
        if (playUndoButton_) {
            playUndoButton_->setEnabled(!playUndoStack_.empty());
        }
        exportPlayBoardToEditor(false, false);
        updatePlayBoard();
        schedulePCScout();
        playBoard_->setFocus();
    }

    bool runPlayCommandInternal(int command, bool allowFollowup = true, bool renderNow = true) {
        if (playGame_.game_over) {
            return false;
        }
        if (command == SFT_CMD_SOFT_DROP && playSoftSpin_ && playSoftSpin_->value() == 0) {
            const bool changed = sft_game_drop_to_surface(&playGame_) != 0;
            if (changed && renderNow) {
                updatePlayBoard();
            }
            return changed;
        }

        const SFTGameState before = playGame_;
        const bool changed = sft_game_command(&playGame_, command) != 0;
        if (!changed) {
            return false;
        }
        if (playGame_.pieces_locked != before.pieces_locked) {
            pushPlayUndo(before);
            applyEntryMovementForHeldInputs();
            exportPlayBoardIfPieceLocked();
        } else if (command == SFT_CMD_HOLD) {
            applyEntryMovementForHeldInputs();
            invalidateAuxiliaryScout();
            schedulePCScout();
        } else if (allowFollowup && (command == SFT_CMD_ROTATE_CW || command == SFT_CMD_ROTATE_CCW || command == SFT_CMD_ROTATE_180)) {
            applyEntryMovementForHeldInputs();
        } else if (allowFollowup && (command == SFT_CMD_LEFT || command == SFT_CMD_RIGHT)) {
            applyHeldSoftDropFollowup();
        }
        if (renderNow) {
            updatePlayBoard();
        }
        return true;
    }

    void runPlayCommand(int command) {
        applyPlayTuning();
        startPlayIfNeeded();
        runPlayCommandInternal(command);
        schedulePlayInputTimer();
        if (playBoard_) {
            playBoard_->setFocus();
        }
    }

    int keyForPlayBinding(const QString &binding) const {
        const QString key = binding.trimmed().toLower();
        if (key == "space") return Qt::Key_Space;
        if (key == "left") return Qt::Key_Left;
        if (key == "right") return Qt::Key_Right;
        if (key == "up") return Qt::Key_Up;
        if (key == "down") return Qt::Key_Down;
        if (key.size() == 1) return key.front().toUpper().unicode();
        return 0;
    }

    int commandForPlayKey(int key) const {
        const std::array<int, 8> commands = {
            SFT_CMD_LEFT, SFT_CMD_RIGHT, SFT_CMD_SOFT_DROP, SFT_CMD_HARD_DROP,
            SFT_CMD_ROTATE_CW, SFT_CMD_ROTATE_CCW, SFT_CMD_ROTATE_180, SFT_CMD_HOLD
        };
        for (int i = 0; i < static_cast<int>(commands.size()); ++i) {
            if (playControlEdits_[i] && keyForPlayBinding(playControlEdits_[i]->text()) == key) {
                return commands[i];
            }
        }
        return 0;
    }

    bool isRepeatablePlayInput(int command) const {
        return isPlayHorizontal(command) || command == SFT_CMD_SOFT_DROP;
    }

    int playInputInitialDelay(int command) const {
        return command == SFT_CMD_SOFT_DROP
            ? (playSoftSpin_ ? playSoftSpin_->value() : 75)
            : (playDasSpin_ ? playDasSpin_->value() : 130);
    }

    int playInputRepeatInterval(int command) const {
        return command == SFT_CMD_SOFT_DROP
            ? (playSoftSpin_ ? playSoftSpin_->value() : 75)
            : (playArrSpin_ ? playArrSpin_->value() : 28);
    }

    qint64 playInputNow() {
        if (!playInputClock_.isValid()) {
            playInputClock_.start();
        }
        return playInputClock_.elapsed();
    }

    void handlePlayKeyPressed(int key, Qt::KeyboardModifiers modifiers) {
        const bool undoShortcut = key == Qt::Key_Z && (modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::MetaModifier));
        if (undoShortcut || (playControlEdits_[8] && keyForPlayBinding(playControlEdits_[8]->text()) == key)) {
            undoPlayPlacement();
            return;
        }
        if (playControlEdits_[9] && keyForPlayBinding(playControlEdits_[9]->text()) == key) {
            resetPlayGame();
            return;
        }
        const int command = commandForPlayKey(key);
        if (command == 0 || heldPlayInputs_.contains(key)) {
            return;
        }
        startPlayIfNeeded();
        const qint64 pressedAt = playInputNow();
        runPlayCommandInternal(command);
        HeldPlayInput input;
        input.command = command;
        input.pressOrder = ++playInputPressCounter_;
        input.pressedAtMs = pressedAt;
        input.nextRepeatAtMs = input.pressedAtMs + playInputInitialDelay(command);
        heldPlayInputs_.insert(key, input);
        schedulePlayInputTimer();
    }

    void handlePlayKeyReleased(int key, Qt::KeyboardModifiers) {
        heldPlayInputs_.remove(key);
        schedulePlayInputTimer();
    }

    bool isPlayHorizontal(int command) const {
        return command == SFT_CMD_LEFT || command == SFT_CMD_RIGHT;
    }

    bool horizontalInputIsBlocked(const HeldPlayInput &input) const {
        if (!isPlayHorizontal(input.command)) {
            return false;
        }
        for (auto it = heldPlayInputs_.cbegin(); it != heldPlayInputs_.cend(); ++it) {
            if (isPlayHorizontal(it->command) && it->command != input.command && it->pressOrder > input.pressOrder) {
                return true;
            }
        }
        return false;
    }

    bool runPlayMovement(int command, int repeats) {
        bool changedAny = false;
        for (int i = 0; i < qMax(1, repeats); ++i) {
            const int piecesBefore = playGame_.pieces_locked;
            if (!runPlayCommandInternal(command, false, false)) {
                break;
            }
            changedAny = true;
            if (playGame_.pieces_locked != piecesBefore) {
                break;
            }
        }
        return changedAny;
    }

    void applyEntryMovementForHeldInputs() {
        const qint64 now = playInputNow();
        int horizontalKey = 0;
        HeldPlayInput horizontal;
        for (auto it = heldPlayInputs_.cbegin(); it != heldPlayInputs_.cend(); ++it) {
            if (isPlayHorizontal(it->command) && it->pressOrder > horizontal.pressOrder) {
                horizontalKey = it.key();
                horizontal = it.value();
            }
        }
        if (horizontalKey != 0 && (horizontal.repeated || now >= horizontal.nextRepeatAtMs)) {
            if (runPlayMovement(horizontal.command, playArrSpin_->value() == 0 ? SFT_GAME_WIDTH : 1)) {
                HeldPlayInput &input = heldPlayInputs_[horizontalKey];
                input.repeated = true;
                const int interval = playInputRepeatInterval(input.command);
                input.nextRepeatAtMs = now + (interval <= 0 ? 16 : interval);
            }
        }
        for (auto it = heldPlayInputs_.begin(); it != heldPlayInputs_.end(); ++it) {
            if (it->command == SFT_CMD_SOFT_DROP
                && (playSoftSpin_->value() == 0 || it->repeated || now >= it->nextRepeatAtMs)) {
                runPlayMovement(SFT_CMD_SOFT_DROP, playSoftSpin_->value() == 0 ? SFT_GAME_HEIGHT : 1);
                it->repeated = true;
                const int interval = playInputRepeatInterval(it->command);
                it->nextRepeatAtMs = now + (interval <= 0 ? 16 : interval);
                break;
            }
        }
        schedulePlayInputTimer();
    }

    void applyHeldSoftDropFollowup() {
        const qint64 now = playInputNow();
        for (auto it = heldPlayInputs_.begin(); it != heldPlayInputs_.end(); ++it) {
            if (it->command != SFT_CMD_SOFT_DROP) {
                continue;
            }
            if (playSoftSpin_->value() == 0 || it->repeated || now >= it->nextRepeatAtMs) {
                runPlayMovement(SFT_CMD_SOFT_DROP, playSoftSpin_->value() == 0 ? SFT_GAME_HEIGHT : 1);
                it->repeated = true;
                const int interval = playInputRepeatInterval(it->command);
                it->nextRepeatAtMs = now + (interval <= 0 ? 16 : interval);
            }
            break;
        }
        schedulePlayInputTimer();
    }

    void schedulePlayInputTimer() {
        if (!playInputTimer_) {
            return;
        }
        playInputTimer_->stop();
        if (!playHasStarted_
            || !sectionTabs_
            || sectionTabs_->currentIndex() != 1
            || playGame_.game_over) {
            return;
        }

        const qint64 now = playInputNow();
        std::optional<qint64> nearestDeadline;
        for (auto it = heldPlayInputs_.cbegin(); it != heldPlayInputs_.cend(); ++it) {
            const HeldPlayInput &input = it.value();
            if (!isRepeatablePlayInput(input.command) || horizontalInputIsBlocked(input)) {
                continue;
            }
            if (!nearestDeadline.has_value() || input.nextRepeatAtMs < nearestDeadline.value()) {
                nearestDeadline = input.nextRepeatAtMs;
            }
        }
        if (!nearestDeadline.has_value()) {
            return;
        }
        const qint64 remaining = qMax<qint64>(0, nearestDeadline.value() - now);
        playInputTimer_->start(static_cast<int>(qMin<qint64>(remaining, 60000)));
    }

    void processPlayInputDeadlines() {
        if (!playHasStarted_
            || !sectionTabs_
            || sectionTabs_->currentIndex() != 1
            || playGame_.game_over) {
            schedulePlayInputTimer();
            return;
        }

        const qint64 now = playInputNow();
        const SFTGameState beforeInput = playGame_;
        QList<int> keys = heldPlayInputs_.keys();
        std::sort(keys.begin(), keys.end(), [this](int left, int right) {
            const HeldPlayInput &a = heldPlayInputs_[left];
            const HeldPlayInput &b = heldPlayInputs_[right];
            const int aPriority = a.command == SFT_CMD_SOFT_DROP ? 0 : (isPlayHorizontal(a.command) ? 1 : 2);
            const int bPriority = b.command == SFT_CMD_SOFT_DROP ? 0 : (isPlayHorizontal(b.command) ? 1 : 2);
            return aPriority == bPriority ? a.pressOrder < b.pressOrder : aPriority < bPriority;
        });

        for (int key : keys) {
            if (!heldPlayInputs_.contains(key)) {
                continue;
            }
            HeldPlayInput input = heldPlayInputs_.value(key);
            if (!isRepeatablePlayInput(input.command)
                || horizontalInputIsBlocked(input)
                || now < input.nextRepeatAtMs) {
                continue;
            }

            input.repeated = true;
            const int interval = playInputRepeatInterval(input.command);
            if (interval <= 0) {
                runPlayMovement(
                    input.command,
                    input.command == SFT_CMD_SOFT_DROP ? SFT_GAME_HEIGHT : SFT_GAME_WIDTH
                );
                input.nextRepeatAtMs = now + 16;
            } else {
                const qint64 dueAt = input.nextRepeatAtMs;
                const int repeatsDue = qMin(
                    64,
                    1 + static_cast<int>(qMax<qint64>(0, now - dueAt) / interval)
                );
                for (int repeat = 0; repeat < repeatsDue; ++repeat) {
                    if (!runPlayMovement(input.command, 1)) {
                        break;
                    }
                }
                input.nextRepeatAtMs = dueAt + static_cast<qint64>(repeatsDue) * interval;
                if (input.nextRepeatAtMs <= now) {
                    input.nextRepeatAtMs = now + interval;
                }
            }
            heldPlayInputs_[key] = input;
        }

        if (playVisualStateChanged(beforeInput, playGame_)) {
            updatePlayBoard();
        }
        schedulePlayInputTimer();
    }

    void advancePlayFrame() {
        if (!playHasStarted_ || !sectionTabs_ || sectionTabs_->currentIndex() != 1) {
            updatePlayTimerState();
            return;
        }
        const qint64 elapsed = playFrameClock_.isValid() ? playFrameClock_.restart() : 16;
        const int elapsedMs = qBound(1, static_cast<int>(elapsed), 100);
        const quint64 renderSerialBefore = playRenderSerial_;
        const SFTGameState beforeTick = playGame_;
        // The UI input repeater owns soft-drop timing. Feeding the held key
        // into the gravity tick would apply an additional level-based drop.
        sft_game_tick(&playGame_, elapsedMs, 0);
        if (playGame_.pieces_locked != beforeTick.pieces_locked) {
            pushPlayUndo(beforeTick);
            applyEntryMovementForHeldInputs();
            exportPlayBoardIfPieceLocked();
        }

        schedulePlayInputTimer();
        playStatsRefreshElapsedMs_ += elapsedMs;
        if (playRenderSerial_ == renderSerialBefore && playVisualStateChanged(beforeTick, playGame_)) {
            updatePlayBoard();
            playStatsRefreshElapsedMs_ = 0;
        } else if (playStatsRefreshElapsedMs_ >= 250) {
            updatePlayStats();
            playStatsRefreshElapsedMs_ = 0;
        }
        if (playGame_.game_over) {
            heldPlayInputs_.clear();
            schedulePlayInputTimer();
            updatePlayTimerState();
        }
    }

    void exportPlayBoardIfPieceLocked() {
        if (playGame_.pieces_locked == playLastExportedPieces_) {
            return;
        }
        playLastExportedPieces_ = playGame_.pieces_locked;
        exportPlayBoardToEditor(false, false);
        invalidateAuxiliaryScout();
        schedulePCScout(false, true);
        if (detectPlayPerfectClear()) {
            return;
        }
        checkPlayOpeningDetection();
    }

    void resetPlayOpeningDetection(int cycleStart) {
        playOpeningCycleStartPieces_ = cycleStart;
        playOpenerDetectionDone_ = false;
        playVariantDetectionDone_ = false;
        playDetectedOpenerName_.clear();
        playDetectedOpenerMirrored_.reset();
        playEarlyVariantDetection_ = false;
        if (playDetectionLabel_) {
            playDetectionLabel_->clear();
        }
    }

    std::array<int, kFumenBlocks> playLockedFumenCells() const {
        std::array<int, kFumenBlocks> cells{};
        cells.fill(0);
        sft_game_write_fumen_cells(&playGame_, 0, cells.data());
        return cells;
    }

    bool detectPlayPerfectClear() {
        if (playGame_.pieces_locked <= playOpeningCycleStartPieces_ || playGame_.last_clear_lines <= 0) {
            return false;
        }
        const bool empty = std::all_of(std::begin(playGame_.board), std::end(playGame_.board), [](int value) {
            return value == 0;
        });
        if (!empty) {
            return false;
        }
        resetPlayOpeningDetection(playGame_.pieces_locked);
        if (playDetectionLabel_) {
            playDetectionLabel_->setText("Perfect clear");
        }
        return true;
    }

    std::vector<OpeningDetectionResult> strictPlayOpeningResults(
        const std::array<int, kFumenBlocks> &cells,
        const QString &openerFilter = QString(),
        const std::optional<bool> &mirrorFilter = std::nullopt,
        bool variantsOnly = false,
        const QString &debugContext = "Play opener scan") const {
        std::vector<OpeningDetectionResult> filtered;
        const auto candidates = detectOpenersForCells(cells);
        logOpeningCandidates(debugContext, candidates);
        for (const OpeningDetectionResult &result : candidates) {
            if (!openerFilter.isEmpty() && result.opener.openerName != openerFilter) {
                continue;
            }
            if (mirrorFilter.has_value() && result.mirrored != mirrorFilter.value()) {
                continue;
            }
            if (variantsOnly && result.opener.variationName.compare("Base", Qt::CaseInsensitive) == 0) {
                continue;
            }
            if (result.occupancyScore < 0.77 || result.overallScore() < 0.77) {
                continue;
            }
            if (result.comparableColorCells >= 6) {
                if (result.colorScore < 0.70) {
                    continue;
                }
            } else if (result.occupancyScore < 0.85) {
                continue;
            }
            filtered.push_back(result);
        }
        return filtered;
    }

    void detectPlayVariant(const std::array<int, kFumenBlocks> &cells) {
        playVariantDetectionDone_ = true;
        const auto results = strictPlayOpeningResults(
            cells,
            playDetectedOpenerName_,
            playDetectedOpenerMirrored_,
            true,
            "Play variant scan");
        if (results.empty()) {
            return;
        }
        const OpeningDetectionResult &best = results.front();
        QString name = best.opener.openerName + " - " + best.opener.variationName;
        if (best.mirrored) {
            name += " mirrored";
        }
        if (playDetectionLabel_) {
            playDetectionLabel_->setText("Variant: " + name);
        }
    }

    void checkPlayOpeningDetection() {
        const int cyclePieces = playGame_.pieces_locked - playOpeningCycleStartPieces_;
        if (cyclePieces < 6) {
            return;
        }
        const auto cells = playLockedFumenCells();
        if (!playOpenerDetectionDone_) {
            playOpenerDetectionDone_ = true;
            const auto results = strictPlayOpeningResults(
                cells, QString(), std::nullopt, false, "Play opener scan");
            if (!results.empty()) {
                const OpeningDetectionResult &best = results.front();
                playDetectedOpenerName_ = best.opener.openerName;
                playDetectedOpenerMirrored_ = best.mirrored;
                playEarlyVariantDetection_ = best.opener.earlyVariantDetection
                    || best.opener.openerName.compare("DPC Patterns", Qt::CaseInsensitive) == 0;
                QString name = best.opener.openerName;
                if (best.mirrored) {
                    name += " mirrored";
                }
                if (playDetectionLabel_) {
                    playDetectionLabel_->setText("Opener: " + name);
                }
            }
        }
        if (playDetectedOpenerName_.isEmpty() || playVariantDetectionDone_) {
            return;
        }
        const int variantPieces = playEarlyVariantDetection_ ? 6 : 12;
        if (cyclePieces >= variantPieces) {
            detectPlayVariant(cells);
        }
    }

    void exportPlayBoardToEditor(bool switchToEditor, bool useActivePieceSetting = true) {
        std::array<int, kFumenBlocks> cells{};
        cells.fill(0);
        const bool includeActive = useActivePieceSetting && playExportActiveCheck_ && playExportActiveCheck_->isChecked();
        sft_game_write_fumen_cells(&playGame_, includeActive ? 1 : 0, cells.data());
        storePlayBoardInFumen(cells);
        if (switchToEditor && sectionTabs_) {
            sectionTabs_->setCurrentIndex(0);
        }
    }

    void storePlayBoardInFumen(const std::array<int, kFumenBlocks> &cells) {
        ensureFumenState();
        fumenPages_[currentFumenPage_] = cells;
        for (int index = 230; index < kFumenBlocks; ++index) {
            fumenPages_[currentFumenPage_][index] = 0;
        }
        currentOperation_ = FumenOperation();
        if (currentFumenPage_ >= static_cast<int>(fumenOperations_.size())) {
            fumenOperations_.resize(fumenPages_.size());
        }
        fumenOperations_[currentFumenPage_] = FumenOperation();
        playFumenEditorDirty_ = true;
        if (playFumenSyncTimer_) {
            playFumenSyncTimer_->start(120);
        } else {
            updateFumenCodeFromPages();
        }
    }

    void synchronizePlayFumenEditor() {
        if (!playFumenEditorDirty_) {
            return;
        }
        if (playFumenSyncTimer_) {
            playFumenSyncTimer_->stop();
        }
        if (placeMinoCheck_) {
            updatingFumenControls_ = true;
            placeMinoCheck_->setChecked(false);
            updatingFumenControls_ = false;
        }
        updateMinoControls();
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
        playFumenEditorDirty_ = false;
    }

    QString playClearText() const {
        const int lines = playGame_.last_clear_lines;
        QString lineName;
        if (lines == 1) lineName = "Single";
        else if (lines == 2) lineName = "Double";
        else if (lines == 3) lineName = "Triple";
        else if (lines == 4) lineName = "Tetris";
        else if (lines > 4) lineName = QString("%1 Lines").arg(lines);
        if (playGame_.last_clear_t_spin) {
            const QString spin = playGame_.last_clear_t_spin_mini ? "Mini T-Spin" : "T-Spin";
            return lineName.isEmpty() ? spin : spin + " " + lineName;
        }
        return lineName;
    }

    void updatePlayStats() {
        if (playGravityLevelBox_ && playLockSpin_) {
            const int currentLevel = qBound(1, playGame_.gravity_level, 30);
            const int currentLockDelay = playGame_.lock_delay_ms;
            if (playGravityLevelBox_->currentData().toInt() != currentLevel
                || playLockSpin_->value() != currentLockDelay) {
                updatingPlayLevelLock_ = true;
                playGravityLevelBox_->setCurrentIndex(currentLevel - 1);
                playLockSpin_->setValue(currentLockDelay);
                updatingPlayLevelLock_ = false;
            }
        }
        if (playPiecesLabel_) playPiecesLabel_->setText(QString("Pieces: %1").arg(playGame_.pieces_locked));
        if (playLinesLabel_) playLinesLabel_->setText(QString("Lines: %1").arg(playGame_.lines_cleared));
        if (playLevelLabel_) {
            const int progressionLines = qMax(0, playGame_.lines_cleared - playGame_.progression_start_lines);
            const QString levelText = playGame_.level_progression_enabled
                ? (playGame_.gravity_level >= 30
                    ? "Level: 30 (maximum)"
                    : QString("Level: %1 (%2/10 lines)").arg(playGame_.gravity_level).arg(progressionLines % 10))
                : QString("Level: %1").arg(playGame_.gravity_level);
            playLevelLabel_->setText(levelText);
        }
        if (playClearLabel_) playClearLabel_->setText(playClearText());
        const double seconds = playHasStarted_ && playStatsClock_.isValid() ? playStatsClock_.elapsed() / 1000.0 : 0.0;
        const double pps = seconds > 0 ? playGame_.pieces_locked / seconds : 0.0;
        if (playPpsLabel_) playPpsLabel_->setText(QString("PPS: %1").arg(pps, 0, 'f', 2));
        if (playStatusLabel_) {
            if (playGame_.game_over) {
                playStatusLabel_->setText("Game over");
            } else if (playGame_.grounded && !playGame_.infinite_lock_delay) {
                playStatusLabel_->setText(QString("Lock: %1/%2 ms").arg(playGame_.lock_elapsed_ms).arg(playGame_.lock_delay_ms));
            } else if (playStatusLabel_->text() != "Loaded editor board into play.") {
                playStatusLabel_->clear();
            }
        }
        if (playUndoButton_) playUndoButton_->setEnabled(!playUndoStack_.empty());
    }

    void updatePiecePreviewSizes() {
        if (!playPreviewSpin_) {
            return;
        }
        if (playHoldPreview_) playHoldPreview_->setCellSize(playPreviewSpin_->value());
        for (auto *preview : playNextPreviews_) {
            if (preview) preview->setCellSize(playPreviewSpin_->value());
        }
    }

    void loadPlaySettings() {
        loadingPlaySettings_ = true;
        QSettings settings;
        const std::array<QString, 10> defaults = {"a", "d", "s", "space", "w", "q", "e", "c", "z", "r"};
        const std::array<QString, 10> names = {"left", "right", "softDrop", "hardDrop", "rotateCW", "rotateCCW", "rotate180", "hold", "undo", "reset"};
        for (int i = 0; i < static_cast<int>(playControlEdits_.size()); ++i) {
            playControlEdits_[i]->setText(settings.value("play/control/" + names[i], defaults[i]).toString());
        }
        playQueueEdit_->setText(settings.value("play/customQueue", "").toString());
        playHoldBox_->setCurrentText(settings.value("play/customHold", "-").toString());
        playDasSpin_->setValue(settings.value("play/tuning/das", 130).toInt());
        playArrSpin_->setValue(settings.value("play/tuning/arr", 28).toInt());
        playSoftSpin_->setValue(settings.value("play/tuning/softDrop", 75).toInt());
        int gravityLevel = qBound(1, settings.value("play/tuning/gravityLevel", 1).toInt(), 30);
        const int storedLockDelay = settings.value("play/tuning/lockDelay", 500).toInt();
        if (gravityLevel > 20 && storedLockDelay != sft_game_lock_delay_for_level(gravityLevel)) {
            gravityLevel = 20;
        }
        playConfiguredGravityLevel_ = gravityLevel;
        playConfiguredLockDelay_ = gravityLevel > 20
            ? sft_game_lock_delay_for_level(gravityLevel)
            : storedLockDelay;
        playGravityLevelBox_->setCurrentIndex(gravityLevel - 1);
        playLockSpin_->setValue(playConfiguredLockDelay_);
        playMoveResetLimitSpin_->setValue(settings.value("play/tuning/moveResetLimit", 15).toInt());
        playPreviewSpin_->setValue(settings.value("play/previewCellSize", 14).toInt());
        playGravityCheck_->setChecked(settings.value("play/rules/gravity", true).toBool());
        playLevelProgressionCheck_->setChecked(settings.value("play/rules/levelProgression", false).toBool());
        const bool stepReset = settings.value("play/rules/stepReset", false).toBool();
        playStepResetCheck_->setChecked(stepReset);
        playMoveResetCheck_->setChecked(!stepReset && settings.value("play/rules/moveReset", true).toBool());
        playMoveResetLimitSpin_->setEnabled(playMoveResetCheck_->isChecked());
        playInfiniteLockCheck_->setChecked(settings.value("play/rules/infiniteLock", false).toBool());
        playInfiniteHoldCheck_->setChecked(settings.value("play/rules/infiniteHold", false).toBool());
        playExportActiveCheck_->setChecked(settings.value("play/rules/exportActive", false).toBool());
        loadingPlaySettings_ = false;
    }

    void savePlaySettings() {
        if (loadingPlaySettings_ || !playQueueEdit_) {
            return;
        }
        QSettings settings;
        const std::array<QString, 10> names = {"left", "right", "softDrop", "hardDrop", "rotateCW", "rotateCCW", "rotate180", "hold", "undo", "reset"};
        for (int i = 0; i < static_cast<int>(playControlEdits_.size()); ++i) {
            settings.setValue("play/control/" + names[i], playControlEdits_[i]->text());
        }
        settings.setValue("play/customQueue", playQueueEdit_->text());
        settings.setValue("play/customHold", playHoldBox_->currentText());
        settings.setValue("play/tuning/das", playDasSpin_->value());
        settings.setValue("play/tuning/arr", playArrSpin_->value());
        settings.setValue("play/tuning/softDrop", playSoftSpin_->value());
        settings.setValue("play/tuning/gravityLevel", playConfiguredGravityLevel_);
        settings.setValue("play/tuning/lockDelay", playConfiguredLockDelay_);
        settings.setValue("play/tuning/moveResetLimit", playMoveResetLimitSpin_->value());
        settings.setValue("play/previewCellSize", playPreviewSpin_->value());
        settings.setValue("play/rules/gravity", playGravityCheck_->isChecked());
        settings.setValue("play/rules/levelProgression", playLevelProgressionCheck_->isChecked());
        settings.setValue("play/rules/moveReset", playMoveResetCheck_->isChecked());
        settings.setValue("play/rules/stepReset", playStepResetCheck_->isChecked());
        settings.setValue("play/rules/infiniteLock", playInfiniteLockCheck_->isChecked());
        settings.setValue("play/rules/infiniteHold", playInfiniteHoldCheck_->isChecked());
        settings.setValue("play/rules/exportActive", playExportActiveCheck_->isChecked());
    }

    void selectPaint(int value) {
        if (board_) {
            board_->setPaintValue(value);
        }
        for (auto *button : paintButtons_) {
            button->setChecked(false);
        }
        for (int i = 0; i < static_cast<int>(paintValues_.size()) && i < static_cast<int>(paintButtons_.size()); ++i) {
            if (paintValues_[i] == value) {
                paintButtons_[i]->setChecked(true);
            }
        }
    }

    void loadOpeners(bool resetEditor = true) {
        openers_.clear();
        const QString databasePath = openerDatabasePath(repoRoot_);
        QFile file(databasePath);
        if (!file.open(QIODevice::ReadOnly)) {
            if (outputEdit_) {
                outputEdit_->appendPlainText("Could not open " + databasePath);
            }
            DiagnosticLog::instance().append("Could not open opener database: " + databasePath);
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            DiagnosticLog::instance().append(
                "Could not parse opener database " + databasePath + ": " + parseError.errorString());
            return;
        }
        const QJsonArray array = doc.object().value("openers").toArray();
        for (const QJsonValue &value : array) {
            const QJsonObject object = value.toObject();
            Opener opener;
            opener.id = object.value("id").toString();
            opener.openerName = object.value("openerName").toString(object.value("name").toString());
            opener.variationName = object.value("variationName").toString("Base");
            opener.code = object.value("code").toString();
            opener.earlyVariantDetection = object.value("earlyVariantDetection").toBool(false);
            if (!opener.openerName.isEmpty()) {
                openers_.push_back(opener);
            }
        }

        QStringList groups;
        for (const Opener &opener : openers_) {
            if (!groups.contains(opener.openerName)) {
                groups << opener.openerName;
            }
        }
        groups.sort(Qt::CaseInsensitive);
        loadingOpeners_ = true;
        openerGroupBox_->blockSignals(true);
        openerVariationBox_->blockSignals(true);
        openerGroupBox_->clear();
        openerGroupBox_->addItem("Choose opener...", "");
        for (const QString &group : groups) {
            openerGroupBox_->addItem(group, group);
        }
        openerVariationBox_->clear();
        openerVariationBox_->addItem("Choose variation...", "");
        openerVariationBox_->blockSignals(false);
        openerGroupBox_->blockSignals(false);
        loadingOpeners_ = false;
        if (resetEditor) {
            selectEmptyBoardPreset();
        }
        DiagnosticLog::instance().append(
            QString("Loaded %1 opener records from %2")
                .arg(static_cast<int>(openers_.size()))
                .arg(databasePath));
    }

    void selectEmptyBoardPreset() {
        const int groupIndex = openerGroupBox_->findData("Empty board");
        if (groupIndex < 0) {
            return;
        }
        openerGroupBox_->blockSignals(true);
        openerGroupBox_->setCurrentIndex(groupIndex);
        openerGroupBox_->blockSignals(false);
        populateVariations();
        const int variationIndex = openerVariationBox_->findData("empty");
        if (variationIndex >= 0) {
            openerVariationBox_->blockSignals(true);
            openerVariationBox_->setCurrentIndex(variationIndex);
            openerVariationBox_->blockSignals(false);
            loadSelectedOpener();
        }
    }

    void populateVariations() {
        if (loadingOpeners_) {
            return;
        }
        const QString group = openerGroupBox_->currentText();
        const QString groupId = openerGroupBox_->currentData().toString();
        openerVariationBox_->blockSignals(true);
        openerVariationBox_->clear();
        openerVariationBox_->addItem(groupId.isEmpty() ? "Choose variation..." : "Choose variation...", "");
        if (groupId.isEmpty()) {
            openerVariationBox_->blockSignals(false);
            return;
        }
        for (const Opener &opener : openers_) {
            if (opener.openerName == group) {
                openerVariationBox_->addItem(opener.variationName, opener.id);
            }
        }
        openerVariationBox_->blockSignals(false);
    }

    void loadSelectedOpener() {
        if (loadingOpeners_) {
            return;
        }
        const QString id = openerVariationBox_->currentData().toString();
        if (id.isEmpty()) {
            return;
        }
        for (const Opener &opener : openers_) {
            if (opener.id == id) {
                updatingFumenEdit_ = true;
                fumenEdit_->setPlainText(opener.code);
                updatingFumenEdit_ = false;
                if (opener.code.isEmpty()) {
                    resetToEmptyFumen();
                    outputEdit_->appendPlainText("Loaded opener: " + opener.openerName + " - " + opener.variationName);
                    return;
                }
                const auto decoded = decodeFumenV115(opener.code);
                if (decoded.has_value()) {
                    replaceFumenPages(decoded->pages, decoded->operations);
                    if (commandBox_ && commandBox_->currentText() == "setup") {
                        convertFumenToSetupGray();
                    }
                    outputEdit_->appendPlainText(QString("Loaded opener: %1 - %2 (%3 page%4)")
                                                     .arg(opener.openerName, opener.variationName)
                                                     .arg(decoded->pageCount)
                                                     .arg(decoded->pageCount == 1 ? "" : "s"));
                } else {
                    outputEdit_->appendPlainText("Could not decode opener fumen: " + opener.openerName + " - " + opener.variationName);
                }
                return;
            }
        }
    }

    void replaceCurrentPageWithFumenCells(const std::array<int, kFumenBlocks> &cells) {
        playFumenEditorDirty_ = false;
        ensureFumenState();
        fumenPages_[currentFumenPage_] = cells;
        for (int index = 230; index < kFumenBlocks; ++index) {
            fumenPages_[currentFumenPage_][index] = 0;
        }
        currentOperation_ = FumenOperation();
        if (placeMinoCheck_) {
            updatingFumenControls_ = true;
            placeMinoCheck_->setChecked(false);
            updatingFumenControls_ = false;
        }
        updateMinoControls();
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
    }

    bool importBoardScreenshot() {
        const auto image = captureBoardScreenshot(this);
        if (!image.has_value()) {
            return false;
        }
        const bool preservingColors = !commandBox_ || commandBox_->currentText() != "setup";
        const auto cells = fumenCellsFromBoardImage(*image, preservingColors);
        if (!cells.has_value()) {
            QMessageBox::warning(this, "Screenshot", "Could not read the screenshot as a Tetris board.");
            DiagnosticLog::instance().append("Board screenshot classification failed.");
            return false;
        }
        const int occupied = static_cast<int>(std::count_if(
            cells->begin(), cells->end(), [](int value) { return value != 0; }));
        replaceCurrentPageWithFumenCells(*cells);
        if (commandBox_ && commandBox_->currentText() == "setup") {
            convertFumenToSetupGray();
        }
        if (outputEdit_) {
            appendRawOutput("\nImported board screenshot.\n");
        }
        DiagnosticLog::instance().append(
            QString("Imported board screenshot: %1 occupied cells, colors %2")
                .arg(occupied)
                .arg(preservingColors ? "preserved" : "forced gray"));
        return true;
    }

    std::vector<std::vector<int>> fumenOccupancyMask(const std::array<int, kFumenBlocks> &cells, bool mirrored = false) const {
        int usedHeight = 0;
        for (int bottomOffset = 0; bottomOffset < kRows; ++bottomOffset) {
            const int row = kVisibleBottomRow - 1 - bottomOffset;
            bool occupied = false;
            for (int col = 0; col < kColumns; ++col) {
                if (cells[row * kColumns + col] != 0) {
                    occupied = true;
                    break;
                }
            }
            if (occupied) {
                usedHeight = bottomOffset + 1;
            }
        }
        std::vector<std::vector<int>> mask;
        for (int bottomOffset = 0; bottomOffset < usedHeight; ++bottomOffset) {
            const int row = kVisibleBottomRow - 1 - bottomOffset;
            std::vector<int> maskRow;
            maskRow.reserve(kColumns);
            for (int col = 0; col < kColumns; ++col) {
                const int sourceColumn = mirrored ? kColumns - 1 - col : col;
                maskRow.push_back(cells[row * kColumns + sourceColumn] != 0 ? 1 : 0);
            }
            mask.push_back(maskRow);
        }
        return mask;
    }

    std::vector<std::vector<int>> fumenColorMask(const std::array<int, kFumenBlocks> &cells, bool mirrored = false) const {
        int usedHeight = 0;
        for (int bottomOffset = 0; bottomOffset < kRows; ++bottomOffset) {
            const int row = kVisibleBottomRow - 1 - bottomOffset;
            bool occupied = false;
            for (int col = 0; col < kColumns; ++col) {
                if (cells[row * kColumns + col] != 0) {
                    occupied = true;
                    break;
                }
            }
            if (occupied) {
                usedHeight = bottomOffset + 1;
            }
        }
        std::vector<std::vector<int>> mask;
        for (int bottomOffset = 0; bottomOffset < usedHeight; ++bottomOffset) {
            const int row = kVisibleBottomRow - 1 - bottomOffset;
            std::vector<int> maskRow;
            maskRow.reserve(kColumns);
            for (int col = 0; col < kColumns; ++col) {
                const int sourceColumn = mirrored ? kColumns - 1 - col : col;
                const int value = cells[row * kColumns + sourceColumn];
                maskRow.push_back(mirrored ? mirrorColor(value) : value);
            }
            mask.push_back(maskRow);
        }
        return mask;
    }

    std::vector<std::vector<int>> shiftedMask(const std::vector<std::vector<int>> &mask, int offset) const {
        if (mask.empty() || offset == 0) {
            return mask;
        }
        std::vector<std::vector<int>> shifted;
        if (offset > 0) {
            shifted.assign(offset, std::vector<int>(kColumns, 0));
            shifted.insert(shifted.end(), mask.begin(), mask.end());
            return shifted;
        }
        const int dropCount = std::min(static_cast<int>(mask.size()), std::abs(offset));
        shifted.insert(shifted.end(), mask.begin() + dropCount, mask.end());
        return shifted;
    }

    double occupancyScore(const std::vector<std::vector<int>> &lhs, const std::vector<std::vector<int>> &rhs) const {
        const int height = std::max(lhs.size(), rhs.size());
        if (height <= 0) {
            return 0.0;
        }
        int intersection = 0;
        int unionCount = 0;
        int leftCount = 0;
        int rightCount = 0;
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                const bool left = row < static_cast<int>(lhs.size()) && lhs[row][col] != 0;
                const bool right = row < static_cast<int>(rhs.size()) && rhs[row][col] != 0;
                if (left) {
                    ++leftCount;
                }
                if (right) {
                    ++rightCount;
                }
                if (left || right) {
                    ++unionCount;
                    if (left && right) {
                        ++intersection;
                    }
                }
            }
        }
        if (unionCount == 0 || leftCount == 0 || rightCount == 0) {
            return 0.0;
        }
        const double jaccard = static_cast<double>(intersection) / unionCount;
        const double coverage = std::min(static_cast<double>(intersection) / leftCount,
                                         static_cast<double>(intersection) / rightCount);
        return std::max(jaccard, coverage * 0.92);
    }

    std::pair<int, int> colorMatch(const std::vector<std::vector<int>> &lhs, const std::vector<std::vector<int>> &rhs) const {
        const int height = std::max(lhs.size(), rhs.size());
        int matching = 0;
        int comparable = 0;
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                const int left = row < static_cast<int>(lhs.size()) ? lhs[row][col] : 0;
                const int right = row < static_cast<int>(rhs.size()) ? rhs[row][col] : 0;
                if (left == 0 || right == 0 || left == 8 || right == 8) {
                    continue;
                }
                ++comparable;
                if (left == right) {
                    ++matching;
                }
            }
        }
        return {matching, comparable};
    }

    bool betterOpeningResult(const OpeningDetectionResult &candidate, const std::optional<OpeningDetectionResult> &current) const {
        if (!current.has_value()) {
            return true;
        }
        const double occupancyDelta = candidate.occupancyScore - current->occupancyScore;
        if (std::abs(occupancyDelta) > 0.04) {
            return occupancyDelta > 0;
        }
        if (candidate.comparableColorCells >= 6 && current->comparableColorCells >= 6) {
            const double combinedDelta = candidate.overallScore() - current->overallScore();
            if (std::abs(combinedDelta) > 0.01) {
                return combinedDelta > 0;
            }
        }
        if (std::abs(occupancyDelta) > 0.015) {
            return occupancyDelta > 0;
        }
        const double colorDelta = candidate.colorScore - current->colorScore;
        if (std::abs(colorDelta) > 0.08) {
            return colorDelta > 0;
        }
        if (candidate.opener.variationName == "Base" && current->opener.variationName != "Base") {
            return true;
        }
        return false;
    }

    std::optional<OpeningDetectionResult> bestOpeningDetectionResult(
        const Opener &opener,
        const std::vector<std::array<int, kFumenBlocks>> &pages,
        const std::vector<std::vector<int>> &targetMask,
        const std::vector<std::vector<int>> &targetColors) const {
        std::optional<OpeningDetectionResult> best;
        const std::vector<int> offsets = {-2, -1, 0, 1, 2};
        for (int pageIndex = 0; pageIndex < static_cast<int>(pages.size()); ++pageIndex) {
            for (bool mirrored : {false, true}) {
                for (int offset : offsets) {
                    const auto sampleMask = shiftedMask(fumenOccupancyMask(pages[pageIndex], mirrored), offset);
                    const auto sampleColors = shiftedMask(fumenColorMask(pages[pageIndex], mirrored), offset);
                    const double shape = occupancyScore(targetMask, sampleMask);
                    const auto color = colorMatch(targetColors, sampleColors);
                    OpeningDetectionResult candidate;
                    candidate.opener = opener;
                    candidate.occupancyScore = shape;
                    candidate.colorScore = color.second == 0 ? 0.0 : static_cast<double>(color.first) / color.second;
                    candidate.comparableColorCells = color.second;
                    candidate.mirrored = mirrored;
                    candidate.pageIndex = pageIndex;
                    candidate.rowOffset = offset;
                    if (betterOpeningResult(candidate, best)) {
                        best = candidate;
                    }
                }
            }
        }
        return best;
    }

    std::vector<OpeningDetectionResult> detectOpenersForCells(const std::array<int, kFumenBlocks> &cells) const {
        const auto targetMask = fumenOccupancyMask(cells);
        const auto targetColors = fumenColorMask(cells);
        std::vector<OpeningDetectionResult> results;
        for (const Opener &opener : openers_) {
            if (opener.id.isEmpty() || opener.code.isEmpty() || opener.id == "empty") {
                continue;
            }
            const auto decoded = decodeFumenV115(opener.code);
            if (!decoded.has_value() || decoded->pages.empty()) {
                continue;
            }
            const auto result = bestOpeningDetectionResult(opener, decoded->pages, targetMask, targetColors);
            if (result.has_value() && (result->occupancyScore >= 0.42 || result->overallScore() >= 0.42)) {
                results.push_back(*result);
            }
        }
        std::sort(results.begin(), results.end(), [this](const OpeningDetectionResult &lhs, const OpeningDetectionResult &rhs) {
            return betterOpeningResult(lhs, std::optional<OpeningDetectionResult>(rhs));
        });
        return results;
    }

    void logOpeningCandidates(
        const QString &context,
        const std::vector<OpeningDetectionResult> &results) const {
        QStringList lines;
        const int count = qMin(8, static_cast<int>(results.size()));
        for (int index = 0; index < count; ++index) {
            const OpeningDetectionResult &result = results[index];
            lines << QString(
                         "%1. %2 | overall %3% | shape %4% | color %5% (%6 cells) | page %7 | row %8")
                         .arg(index + 1)
                         .arg(result.displayName())
                         .arg(result.overallScore() * 100.0, 0, 'f', 1)
                         .arg(result.occupancyScore * 100.0, 0, 'f', 1)
                         .arg(result.colorScore * 100.0, 0, 'f', 1)
                         .arg(result.comparableColorCells)
                         .arg(result.pageIndex + 1)
                         .arg(result.rowOffset);
        }
        if (lines.isEmpty()) {
            lines << "No candidates passed the preliminary shape threshold.";
        }
        DiagnosticLog::instance().appendBlock(context, lines);
    }

    std::array<int, kFumenBlocks> mirroredFumenPage(std::array<int, kFumenBlocks> page) const {
        std::array<int, kFumenBlocks> mirrored{};
        mirrored.fill(0);
        for (int row = 0; row < kFumenRows; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                mirrored[row * kColumns + (kColumns - 1 - col)] = mirrorColor(page[row * kColumns + col]);
            }
        }
        return mirrored;
    }

    void loadDetectedOpening(const OpeningDetectionResult &result) {
        const auto decoded = decodeFumenV115(result.opener.code);
        if (!decoded.has_value()) {
            return;
        }
        std::vector<std::array<int, kFumenBlocks>> pages = decoded->pages;
        if (result.mirrored) {
            for (auto &page : pages) {
                page = mirroredFumenPage(page);
            }
        }
        replaceFumenPages(pages, decoded->operations);
        if (result.pageIndex >= 0 && result.pageIndex < static_cast<int>(fumenPages_.size())) {
            goToFumenPage(result.pageIndex);
        }
        updatingFumenEdit_ = true;
        fumenEdit_->setPlainText(result.opener.code);
        updatingFumenEdit_ = false;
        updateFumenCodeFromPages();
        if (outputEdit_) {
            appendRawOutput("\nDetected " + result.displayName() + ".\n");
        }
    }

    void detectOpeningFromScreenshot() {
        const auto image = captureBoardScreenshot(this);
        if (!image.has_value()) {
            return;
        }
        const auto cells = fumenCellsFromBoardImage(*image, true);
        if (!cells.has_value()) {
            QMessageBox::warning(this, "Opener Detector", "Could not read the screenshot as a Tetris board.");
            return;
        }
        const auto results = detectOpenersForCells(*cells);
        logOpeningCandidates("Screenshot opener scan", results);
        if (results.empty() || results.front().overallScore() < 0.58) {
            QMessageBox::information(this, "Opener Detector", "No opener match found.");
            return;
        }

        const OpeningDetectionResult &best = results.front();
        const QString message =
            best.displayName() + "\n\nAdd this opener to the editor?";
        QMessageBox box(QMessageBox::Question, "Opener Match Found", message, QMessageBox::NoButton, this);
        auto *addButton = box.addButton("Add to Editor", QMessageBox::AcceptRole);
        box.addButton("Close", QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == addButton) {
            loadDetectedOpening(best);
        }
    }

    QString generatedFieldText() const {
        const auto &cells = board_->cells();
        int lowestRow = kRows - 1;
        int highestOccupied = kRows;
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                if (cells[row * kColumns + col] != 0) {
                    highestOccupied = qMin(highestOccupied, row);
                }
            }
        }
        const int height = highestOccupied == kRows ? 1 : (lowestRow - highestOccupied + 1);
        QString text = QString::number(height) + "\n";
        for (int row = kRows - height; row < kRows; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                text += cells[row * kColumns + col] == 0 ? "_" : "X";
            }
            text += "\n";
        }
        return text;
    }

    QString setupFieldText(bool *ok, QString *error) {
        if (ok) {
            *ok = false;
        }
        if (error) {
            error->clear();
        }
        syncCurrentPageFromBoard();
        const auto &cells = board_->cells();
        const int height = autoCommandHeight();
        if (linesSpin_) {
            linesSpin_->setValue(qMin(12, height));
        }
        if (height > 12) {
            if (error) {
                *error = "setup field input supports heights up to 12";
            }
            return QString();
        }

        QStringList rows;
        bool hasFilledBlock = false;
        for (int y = height - 1; y >= 0; --y) {
            const int row = kRows - 1 - y;
            QString rowText;
            for (int col = 0; col < kColumns; ++col) {
                const int value = cells[row * kColumns + col];
                if (value == 1) {
                    rowText += "*";
                    hasFilledBlock = true;
                } else if (value == 3) {
                    rowText += ".";
                } else if (value == 8) {
                    rowText += "X";
                } else {
                    rowText += "_";
                }
            }
            rows << rowText;
        }
        if (!hasFilledBlock) {
            if (error) {
                *error = "setup needs at least one I cell inside the selected height";
            }
            return QString();
        }
        rows.prepend(QString::number(height));
        if (ok) {
            *ok = true;
        }
        return rows.join('\n') + "\n";
    }

    void updateGeneratedField() {
        if (generatedField_) {
            generatedField_->setPlainText(generatedFieldText());
        }
        updateAutoHeightFields();
    }

    QString writeTextFile(const QString &name, const QString &content) {
        QDir dir(appDataDir());
        dir.mkpath("run");
        const QString path = dir.filePath("run/" + name);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << content;
        }
        return path;
    }

    std::pair<QString, QStringList> sfinderLaunchCommand() const {
        QString program;
        QStringList args;
        const QString linuxLauncher = repoRoot_ + "/native-linux/bin/sfinder";
        const QString macLauncher = repoRoot_ + "/native-macos/bin/sfinder";
#ifndef Q_OS_WIN
        if (QFileInfo::exists(linuxLauncher)) {
            program = linuxLauncher;
        } else if (QFileInfo::exists(macLauncher) && QSysInfo::productType() == "macos") {
            program = macLauncher;
        } else {
            program = "java";
            args << "-jar" << repoRoot_ + "/solution-finder-1.43/sfinder.jar";
        }
#else
        program = "java.exe";
        args << "-jar" << repoRoot_ + "/solution-finder-1.43/sfinder.jar";
#endif
        return {program, args};
    }

    QString auxiliaryScoutFieldText(int *heightOut = nullptr) const {
        int height = 1;
        for (int y = 0; y < SFT_GAME_HEIGHT; ++y) {
            for (int x = 0; x < SFT_GAME_WIDTH; ++x) {
                if (playGame_.board[y * SFT_GAME_WIDTH + x] != 0) {
                    height = qMax(height, y + 1);
                }
            }
        }
        if (heightOut) {
            *heightOut = height;
        }

        QString text;
        for (int y = height - 1; y >= 0; --y) {
            for (int x = 0; x < SFT_GAME_WIDTH; ++x) {
                text += playGame_.board[y * SFT_GAME_WIDTH + x] == 0 ? "_" : "X";
            }
            text += "\n";
        }
        return text;
    }

    QString activeScoutPattern(int depth) const {
        std::array<char, SFT_GAME_PATTERN_TEXT_CAPACITY> pattern{};
        const int patternLength = sft_game_write_active_pattern(
            &playGame_,
            depth,
            pattern.data(),
            static_cast<int>(pattern.size()));
        return patternLength > 0
            ? QString::fromLatin1(pattern.data(), patternLength)
            : QString();
    }

    QSet<QString> legalActiveScoutPatterns(int maximumDepth) const {
        QSet<QString> legalPatterns;
        for (int depth = 1; depth <= maximumDepth; ++depth) {
            std::array<char, SFT_GAME_PATTERN_TEXT_CAPACITY> patterns{};
            const int length = sft_game_write_active_patterns(
                &playGame_,
                depth,
                patterns.data(),
                static_cast<int>(patterns.size()));
            if (length <= 0) {
                continue;
            }
            const QString text = QString::fromLatin1(patterns.data(), length);
            for (const QString &pattern : text.split(';', Qt::SkipEmptyParts)) {
                legalPatterns.insert(pattern.trimmed().toUpper());
            }
        }
        return legalPatterns;
    }

    QString scoutPieceSetKey(QString pieces) const {
        pieces = pieces.trimmed().toUpper();
        std::sort(pieces.begin(), pieces.end());
        return pieces;
    }

    void updateAuxiliaryScoutControls() {
        const bool running = auxiliaryScoutProcess_ != nullptr;
        const bool blocked = process_ != nullptr || pcScoutProcess_ != nullptr;
        if (playSpinScoutButton_) {
            playSpinScoutButton_->setEnabled(!running && !blocked);
        }
        if (playRenScoutButton_) {
            playRenScoutButton_->setEnabled(!running && !blocked);
        }
        if (playSpinScoutCancelButton_) {
            playSpinScoutCancelButton_->setEnabled(running && auxiliaryScoutMode_ == "spin");
        }
        if (playRenScoutCancelButton_) {
            playRenScoutCancelButton_->setEnabled(running && auxiliaryScoutMode_ == "ren");
        }
        if (playSpinScoutPreviewButton_) {
            const bool showing = activeAuxiliaryPreviewMode_ == "spin";
            playSpinScoutPreviewButton_->setText(showing ? "Hide Preview" : "Show Preview");
            playSpinScoutPreviewButton_->setEnabled(
                !running && (showing || overlayHasCells(playSpinScoutOverlayCells_)));
        }
        if (playRenScoutPreviewButton_) {
            const bool showing = activeAuxiliaryPreviewMode_ == "ren";
            playRenScoutPreviewButton_->setText(showing ? "Hide Preview" : "Show Preview");
            playRenScoutPreviewButton_->setEnabled(
                !running && (showing || overlayHasCells(playRenScoutOverlayCells_)));
        }
    }

    bool overlayHasCells(const std::array<int, kColumns * kRows> &cells) const {
        return std::any_of(cells.begin(), cells.end(), [](int value) { return value > 0; });
    }

    void refreshPlaySolutionOverlay() {
        if (!playBoard_) {
            return;
        }
        if (activeAuxiliaryPreviewMode_ == "spin"
            && overlayHasCells(playSpinScoutOverlayCells_)) {
            playBoard_->setSolutionCells(playSpinScoutOverlayCells_);
            return;
        }
        if (activeAuxiliaryPreviewMode_ == "ren"
            && overlayHasCells(playRenScoutOverlayCells_)) {
            playBoard_->setSolutionCells(playRenScoutOverlayCells_);
            return;
        }
        if (activeAuxiliaryPreviewMode_.isEmpty()
            && pcScoutSolutionVisible_
            && overlayHasCells(pcScoutSolutionCells_)) {
            playBoard_->setSolutionCells(pcScoutSolutionCells_);
            return;
        }
        playBoard_->clearSolutionCells();
    }

    std::optional<std::array<int, kColumns * kRows>> scoutOverlayForCode(
        const QString &code,
        const SFTGameState &baseGame,
        bool firstPlacementOnly,
        int *addedCellCount = nullptr
    ) const {
        const auto decoded = decodeFumenV115(code);
        if (!decoded.has_value() || decoded->pages.empty()) {
            return std::nullopt;
        }

        std::array<int, kColumns * kRows> best{};
        best.fill(0);
        int bestCount = 0;
        for (int pageIndex = 0; pageIndex < static_cast<int>(decoded->pages.size()); ++pageIndex) {
            std::array<int, kFumenBlocks> field = decoded->pages[pageIndex];
            if (pageIndex < static_cast<int>(decoded->operations.size())) {
                const FumenOperation &operation = decoded->operations[pageIndex];
                if (operation.type > 0) {
                    for (int index : fumenOperationCells(operation)) {
                        if (0 <= index && index < kFumenBlocks) {
                            field[index] = operation.type;
                        }
                    }
                }
            }

            const auto visible = visibleFumenCells(field);
            std::array<int, kColumns * kRows> candidate{};
            candidate.fill(0);
            int candidateCount = 0;
            for (int row = 0; row < kRows; ++row) {
                const int gameY = kRows - 1 - row;
                for (int column = 0; column < kColumns; ++column) {
                    const int visibleIndex = row * kColumns + column;
                    const int gameIndex = gameY * SFT_GAME_WIDTH + column;
                    if (baseGame.board[gameIndex] == 0 && visible[visibleIndex] > 0) {
                        candidate[visibleIndex] = visible[visibleIndex];
                        ++candidateCount;
                    }
                }
            }
            if (candidateCount > 0 && firstPlacementOnly) {
                if (addedCellCount) {
                    *addedCellCount = candidateCount;
                }
                return candidate;
            }
            if (candidateCount > bestCount) {
                best = candidate;
                bestCount = candidateCount;
            }
        }

        if (bestCount <= 0) {
            return std::nullopt;
        }
        if (addedCellCount) {
            *addedCellCount = bestCount;
        }
        return best;
    }

    std::pair<int, int> spinSurfacePenalties(
        const SFTGameState &baseGame,
        const std::array<int, kColumns * kRows> &overlay
    ) const {
        std::array<int, kColumns> heights{};
        heights.fill(0);
        for (int column = 0; column < kColumns; ++column) {
            for (int y = 0; y < kRows; ++y) {
                const int gameIndex = y * SFT_GAME_WIDTH + column;
                const int visibleIndex = (kRows - 1 - y) * kColumns + column;
                if (baseGame.board[gameIndex] != 0 || overlay[visibleIndex] != 0) {
                    heights[column] = y + 1;
                }
            }
        }

        int pillarPenalty = 0;
        int bumpiness = 0;
        for (int column = 0; column < kColumns; ++column) {
            if (column + 1 < kColumns) {
                bumpiness += std::abs(heights[column] - heights[column + 1]);
            }
            const int left = column > 0 ? heights[column - 1] : heights[column];
            const int right = column + 1 < kColumns ? heights[column + 1] : heights[column];
            const int rise = heights[column] - qMax(left, right);
            if (rise > 1) {
                pillarPenalty += rise * rise;
            }
            if (column + 1 < kColumns) {
                const int cliff = std::abs(heights[column] - heights[column + 1]);
                if (cliff > 2) {
                    pillarPenalty += (cliff - 2) * (cliff - 2);
                }
            }
        }
        return {pillarPenalty, bumpiness};
    }

    std::optional<SpinScoutChoice> bestSpinScoutChoice(
        const QString &html,
        const SFTGameState &baseGame,
        const QSet<QString> &legalPatterns,
        int *queueSolutionCount = nullptr
    ) const {
        const QRegularExpression candidatePattern(
            R"SPIN(<div>\[([^\]]+)\]\s*<a href='[^']*?(v115@[A-Za-z0-9+/\?]+)'[^>]*>([^<]*)</a>\s*\[clear=(\d+),\s*hole=(\d+),\s*piece=(\d+)\])SPIN",
            QRegularExpression::CaseInsensitiveOption);
        struct RawChoice {
            QString mark;
            QString code;
            QString route;
            int holes = 0;
            int pieces = 0;
        };
        std::vector<RawChoice> rawChoices;
        QSet<QString> legalPieceSets;
        for (const QString &pattern : legalPatterns) {
            legalPieceSets.insert(scoutPieceSetKey(pattern));
        }
        int minimumValidity = 1;
        int minimumHoles = std::numeric_limits<int>::max();
        int minimumPieces = std::numeric_limits<int>::max();
        QRegularExpressionMatchIterator matches = candidatePattern.globalMatch(html);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            RawChoice raw;
            raw.mark = match.captured(1).trimmed().toUpper();
            raw.code = match.captured(2);
            raw.route = match.captured(3);
            raw.holes = match.captured(5).toInt();
            raw.pieces = match.captured(6).toInt();

            QString routePieces;
            const QRegularExpression routePattern(R"((?:^|\s)([TIJLSZO])-)");
            QRegularExpressionMatchIterator routeMatches =
                routePattern.globalMatch(raw.route.toUpper());
            while (routeMatches.hasNext()) {
                routePieces += routeMatches.next().captured(1);
            }
            if (routePieces.size() != raw.pieces
                || routePieces.isEmpty()
                || !legalPieceSets.contains(scoutPieceSetKey(routePieces))) {
                continue;
            }

            rawChoices.push_back(raw);
            const int validity = raw.mark == "O" ? 0 : 1;
            if (validity < minimumValidity
                || (validity == minimumValidity && raw.holes < minimumHoles)
                || (validity == minimumValidity
                    && raw.holes == minimumHoles
                    && raw.pieces < minimumPieces)) {
                minimumValidity = validity;
                minimumHoles = raw.holes;
                minimumPieces = raw.pieces;
            }
        }
        if (queueSolutionCount) {
            *queueSolutionCount = static_cast<int>(rawChoices.size());
        }

        std::optional<SpinScoutChoice> best;
        QSet<QString> evaluatedCodes;
        for (const RawChoice &raw : rawChoices) {
            const int validity = raw.mark == "O" ? 0 : 1;
            if (validity != minimumValidity
                || raw.holes != minimumHoles
                || raw.pieces > minimumPieces + 2
                || evaluatedCodes.contains(raw.code)
                || evaluatedCodes.size() >= 512) {
                continue;
            }
            evaluatedCodes.insert(raw.code);
            const auto overlay = scoutOverlayForCode(raw.code, baseGame, false);
            if (!overlay.has_value()) {
                continue;
            }
            const auto [pillarPenalty, bumpiness] =
                spinSurfacePenalties(baseGame, overlay.value());

            SpinScoutChoice choice;
            choice.code = raw.code;
            choice.holes = raw.holes;
            choice.pieces = raw.pieces;
            choice.pillarPenalty = pillarPenalty;
            choice.bumpiness = bumpiness;
            const int validityPenalty = validity;
            choice.score =
                static_cast<qint64>(validityPenalty) * 1000000000LL
                + static_cast<qint64>(choice.holes) * 1000000LL
                + static_cast<qint64>(choice.pillarPenalty) * 10000LL
                + static_cast<qint64>(choice.pieces) * 100LL
                + choice.bumpiness;
            if (!best.has_value() || choice.score < best->score) {
                best = choice;
            }
        }
        return best;
    }

    void scheduleAuxiliaryScouts(bool immediate = false) {
        auxiliaryAutoQueue_.clear();
        if (!auxiliaryScoutRefreshTimer_) {
            return;
        }
        auxiliaryScoutRefreshTimer_->stop();
        const bool spinEnabled =
            playSpinScoutAutoCheck_ && playSpinScoutAutoCheck_->isChecked();
        const bool renEnabled =
            playRenScoutAutoCheck_ && playRenScoutAutoCheck_->isChecked();
        if (!spinEnabled && !renEnabled) {
            return;
        }
        auxiliaryScoutRefreshTimer_->start(immediate ? 0 : 250);
    }

    void runScheduledAuxiliaryScout() {
        if (process_ || pcScoutProcess_ || auxiliaryScoutProcess_) {
            if (auxiliaryScoutRefreshTimer_) {
                auxiliaryScoutRefreshTimer_->start(250);
            }
            return;
        }
        if (auxiliaryAutoQueue_.isEmpty()) {
            if (playSpinScoutAutoCheck_ && playSpinScoutAutoCheck_->isChecked()) {
                auxiliaryAutoQueue_ << "spin";
            }
            if (playRenScoutAutoCheck_ && playRenScoutAutoCheck_->isChecked()) {
                auxiliaryAutoQueue_ << "ren";
            }
        }
        while (!auxiliaryAutoQueue_.isEmpty()) {
            const QString mode = auxiliaryAutoQueue_.takeFirst();
            const bool enabled = mode == "spin"
                ? playSpinScoutAutoCheck_ && playSpinScoutAutoCheck_->isChecked()
                : playRenScoutAutoCheck_ && playRenScoutAutoCheck_->isChecked();
            if (enabled) {
                runAuxiliaryScout(mode, true);
                return;
            }
        }
    }

    void completeAuxiliaryScoutRun(bool automaticRun) {
        updateAuxiliaryScoutControls();
        if (automaticRun && !auxiliaryAutoQueue_.isEmpty()) {
            QTimer::singleShot(0, this, [this]() { runScheduledAuxiliaryScout(); });
            return;
        }
        auxiliaryAutoQueue_.clear();
        if (playPCEnabledCheck_ && playPCEnabledCheck_->isChecked()) {
            schedulePCScout(true);
        }
    }

    void invalidateAuxiliaryScout(const QString &status = "Board changed; scout again") {
        if (auxiliaryScoutRefreshTimer_) {
            auxiliaryScoutRefreshTimer_->stop();
        }
        auxiliaryAutoQueue_.clear();
        if (auxiliaryScoutProcess_) {
            QProcess *process = auxiliaryScoutProcess_;
            auxiliaryScoutProcess_ = nullptr;
            process->terminate();
            QTimer::singleShot(250, process, [process]() {
                if (process->state() != QProcess::NotRunning) {
                    process->kill();
                }
            });
        }
        auxiliaryScoutAutomaticRun_ = false;
        playSpinScoutFumen_.clear();
        playRenScoutFumen_.clear();
        playSpinScoutOverlayCells_.fill(0);
        playRenScoutOverlayCells_.fill(0);
        auxiliaryScoutMode_.clear();
        auxiliaryScoutOutput_.clear();
        if (playSpinScoutResultLabel_) {
            playSpinScoutResultLabel_->clear();
        }
        if (playRenScoutResultLabel_) {
            playRenScoutResultLabel_->clear();
        }
        if (playSpinScoutStatusLabel_) {
            playSpinScoutStatusLabel_->setText(status);
        }
        if (playRenScoutStatusLabel_) {
            playRenScoutStatusLabel_->setText(status);
        }
        updateAuxiliaryScoutControls();
        refreshPlaySolutionOverlay();
        scheduleAuxiliaryScouts();
    }

    void runAuxiliaryScout(const QString &mode, bool automaticRun = false) {
        QLabel *statusLabel = mode == "spin" ? playSpinScoutStatusLabel_ : playRenScoutStatusLabel_;
        QLabel *resultLabel = mode == "spin" ? playSpinScoutResultLabel_ : playRenScoutResultLabel_;
        if (process_ || pcScoutProcess_ || auxiliaryScoutProcess_) {
            if (statusLabel) {
                statusLabel->setText("Another sfinder search is running");
            }
            if (automaticRun && auxiliaryScoutRefreshTimer_) {
                auxiliaryScoutRefreshTimer_->start(250);
            }
            return;
        }

        const int depth = mode == "spin"
            ? playSpinScoutPiecesSpin_->value()
            : playRenScoutPiecesSpin_->value();
        const QString pattern = activeScoutPattern(depth);
        if (pattern.isEmpty()) {
            statusLabel->setText("Could not read enough pieces from the active queue");
            completeAuxiliaryScoutRun(automaticRun);
            return;
        }
        const QSet<QString> legalSpinPatterns =
            mode == "spin" ? legalActiveScoutPatterns(depth) : QSet<QString>();

        int fieldHeight = 1;
        const QString fieldText = auxiliaryScoutFieldText(&fieldHeight);
        const QString fieldPath = writeTextFile("play-" + mode + "-scout-field.txt", fieldText);
        QDir runDir(appDataDir());
        runDir.mkpath("run");
        const QString outputBase = runDir.filePath("run/play-" + mode + "-scout");
        QFile::remove(outputBase + ".html");

        auto launch = sfinderLaunchCommand();
        QStringList args = launch.second;
        args << mode
             << "-fp" << fieldPath
             << "-p" << pattern;
        if (mode == "spin") {
            const int requiredLines = playSpinScoutLinesBox_->currentData().toInt();
            const int fillTop = qMin(kRows, qMax(fieldHeight, requiredLines + 2));
            args << "-c" << QString::number(requiredLines)
                 << "-fb" << "0"
                 << "-ft" << QString::number(fillTop)
                 << "-m" << QString::number(qMin(kRows, fillTop + 2))
                 << "-r" << "false"
                 << "-mr" << "0"
                 << "-f" << "strict"
                 << "-fo" << "html"
                 << "-o" << outputBase;
            playSpinScoutFumen_.clear();
        } else {
            args << "-H" << "avoid"
                 << "-d" << playRenScoutDropBox_->currentData().toString()
                 << "-K" << "srs"
                 << "-o" << outputBase;
            playRenScoutFumen_.clear();
        }

        auxiliaryScoutMode_ = mode;
        auxiliaryScoutAutomaticRun_ = automaticRun;
        auxiliaryScoutGame_ = playGame_;
        auxiliaryScoutOutput_.clear();
        resultLabel->clear();
        statusLabel->setText(
            mode == "spin" ? "Searching for bounded T-spin setups..." : "Searching for REN routes...");
        DiagnosticLog::instance().append(
            QString("%1 scout launched: %2 %3")
                .arg(mode.toUpper(), launch.first, args.join(" ")));

        QProcess *process = new QProcess(this);
        auxiliaryScoutProcess_ = process;
        updateAuxiliaryScoutControls();
        process->setWorkingDirectory(repoRoot_);
        process->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
        process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
            constexpr unsigned long kCreateNoWindow = 0x08000000UL;
            arguments->flags |= kCreateNoWindow;
        });
#endif
        connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
            if (auxiliaryScoutProcess_ == process) {
                auxiliaryScoutOutput_ += QString::fromLocal8Bit(process->readAllStandardOutput());
            } else {
                process->readAllStandardOutput();
            }
        });
        connect(process, &QProcess::finished, this, [
                    this,
                    process,
                    mode,
                    depth,
                    pattern,
                    legalSpinPatterns,
                    outputBase,
                    automaticRun
                ](int exitCode, QProcess::ExitStatus exitStatus) {
            const QString output =
                auxiliaryScoutOutput_ + QString::fromLocal8Bit(process->readAllStandardOutput());
            process->deleteLater();
            if (auxiliaryScoutProcess_ != process) {
                return;
            }
            auxiliaryScoutProcess_ = nullptr;
            auxiliaryScoutMode_.clear();
            auxiliaryScoutAutomaticRun_ = false;

            QLabel *finishedStatus =
                mode == "spin" ? playSpinScoutStatusLabel_ : playRenScoutStatusLabel_;
            QLabel *finishedResult =
                mode == "spin" ? playSpinScoutResultLabel_ : playRenScoutResultLabel_;
            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                finishedStatus->setText("Scout failed; details were added to Output Log");
                DiagnosticLog::instance().appendBlock(
                    mode.toUpper() + " scout failed",
                    {QString("exit %1").arg(exitCode), output.trimmed()});
                completeAuxiliaryScoutRun(automaticRun);
                return;
            }

            const QString resultPath = outputBase + ".html";
            QFile resultFile(resultPath);
            if (!resultFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                finishedStatus->setText("sfinder did not create a readable scout result");
                completeAuxiliaryScoutRun(automaticRun);
                return;
            }
            const QString html = QString::fromUtf8(resultFile.readAll());
            const QRegularExpression countPattern(
                R"(<header>\s*(\d+)\s+solutions?\s*</header>)",
                QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch countMatch = countPattern.match(html);
            const int solutionCount = countMatch.hasMatch() ? countMatch.captured(1).toInt() : 0;
            const QRegularExpression codePattern("v115@[A-Za-z0-9+/\\?]+");
            const QRegularExpressionMatch codeMatch = codePattern.match(html);
            QString code = codeMatch.hasMatch() ? codeMatch.captured(0) : QString();

            if (mode == "spin") {
                int queueSolutionCount = 0;
                const auto bestChoice = bestSpinScoutChoice(
                    html,
                    auxiliaryScoutGame_,
                    legalSpinPatterns,
                    &queueSolutionCount);
                if (bestChoice.has_value()) {
                    code = bestChoice->code;
                } else {
                    code.clear();
                }
                playSpinScoutFumen_ = code;
                playSpinScoutOverlayCells_.fill(0);
                if (const auto overlay =
                        scoutOverlayForCode(code, auxiliaryScoutGame_, false);
                    overlay.has_value()) {
                    playSpinScoutOverlayCells_ = overlay.value();
                }
                if (bestChoice.has_value()) {
                    finishedResult->setText(
                        QString("Best of %1: %2 holes · %3 pieces · pillar score %4")
                            .arg(queueSolutionCount)
                            .arg(bestChoice->holes)
                            .arg(bestChoice->pieces)
                            .arg(bestChoice->pillarPenalty));
                    DiagnosticLog::instance().append(
                        QString("Spin scout selected score %1: holes=%2, pieces=%3, pillars=%4, bumpiness=%5")
                            .arg(bestChoice->score)
                            .arg(bestChoice->holes)
                            .arg(bestChoice->pieces)
                            .arg(bestChoice->pillarPenalty)
                            .arg(bestChoice->bumpiness));
                } else {
                    finishedResult->setText(
                        solutionCount > 0
                            ? "No T-spin setup is reachable from the active queue and hold"
                            : "No practical T-spin setup found");
                }
                DiagnosticLog::instance().append(
                    QString("Spin scout queue filter: %1/%2 setups use pieces available from %3 across %4 legal queue/hold windows")
                        .arg(queueSolutionCount)
                        .arg(solutionCount)
                        .arg(pattern)
                        .arg(legalSpinPatterns.size()));
            } else {
                playRenScoutFumen_ = code;
                playRenScoutOverlayCells_.fill(0);
                if (const auto overlay =
                        scoutOverlayForCode(code, auxiliaryScoutGame_, true);
                    overlay.has_value()) {
                    playRenScoutOverlayCells_ = overlay.value();
                }
                int maxRen = 0;
                const QRegularExpression renPattern(
                    R"(<h2>\s*(\d+)\s+Ren\s*</h2>)",
                    QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatchIterator renMatches = renPattern.globalMatch(html);
                while (renMatches.hasNext()) {
                    maxRen = qMax(maxRen, renMatches.next().captured(1).toInt());
                }
                const QString renLength = maxRen >= 7 ? "7+" : QString::number(maxRen);
                finishedResult->setText(
                    solutionCount > 0
                        ? QString("Best route: %1 REN  |  %2 solutions")
                              .arg(renLength)
                              .arg(solutionCount)
                        : "No REN route found");
            }
            refreshPlaySolutionOverlay();
            finishedStatus->setText(QString("Finished with the active %1-piece queue window").arg(depth));
            DiagnosticLog::instance().append(
                QString("%1 scout finished: %2 solutions from %3 pieces")
                    .arg(mode.toUpper())
                    .arg(solutionCount)
                    .arg(depth));
            completeAuxiliaryScoutRun(automaticRun);
        });

        process->start(launch.first, args);
        if (!process->waitForStarted(1000)) {
            auxiliaryScoutProcess_ = nullptr;
            auxiliaryScoutMode_.clear();
            auxiliaryScoutAutomaticRun_ = false;
            process->deleteLater();
            statusLabel->setText("Scout failed to start sfinder");
            DiagnosticLog::instance().append(mode.toUpper() + " scout failed to start.");
            completeAuxiliaryScoutRun(automaticRun);
        }
    }

    void cancelAuxiliaryScout() {
        if (!auxiliaryScoutProcess_) {
            return;
        }
        const QString mode = auxiliaryScoutMode_;
        const bool automaticRun = auxiliaryScoutAutomaticRun_;
        QProcess *process = auxiliaryScoutProcess_;
        auxiliaryScoutProcess_ = nullptr;
        auxiliaryScoutMode_.clear();
        auxiliaryScoutAutomaticRun_ = false;
        process->terminate();
        QTimer::singleShot(250, process, [process]() {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
            }
        });
        QLabel *statusLabel = mode == "spin" ? playSpinScoutStatusLabel_ : playRenScoutStatusLabel_;
        if (statusLabel) {
            statusLabel->setText("Scout canceled");
        }
        DiagnosticLog::instance().append(mode.toUpper() + " scout canceled.");
        completeAuxiliaryScoutRun(automaticRun);
    }

    void previewAuxiliaryScoutResult(const QString &mode) {
        const QString code = mode == "spin" ? playSpinScoutFumen_ : playRenScoutFumen_;
        QLabel *statusLabel = mode == "spin" ? playSpinScoutStatusLabel_ : playRenScoutStatusLabel_;
        if (activeAuxiliaryPreviewMode_ == mode) {
            activeAuxiliaryPreviewMode_.clear();
            refreshPlaySolutionOverlay();
            if (statusLabel) {
                statusLabel->setText("Preview hidden");
            }
            updateAuxiliaryScoutControls();
            return;
        }
        if (code.isEmpty()) {
            return;
        }
        activeAuxiliaryPreviewMode_ = mode;
        refreshPlaySolutionOverlay();
        if (statusLabel) {
            statusLabel->setText(
                mode == "spin"
                    ? "Showing the T-spin setup on the playfield"
                    : "Showing the next placement required to continue the combo");
        }
        updateAuxiliaryScoutControls();
    }

    void updatePCScoutControls() {
        const bool enabled = playPCEnabledCheck_ && playPCEnabledCheck_->isChecked();
        const bool running = pcScoutProcess_ != nullptr;
        const bool blocked = process_ != nullptr || auxiliaryScoutProcess_ != nullptr;
        if (playPCSourceBox_) {
            playPCSourceBox_->setEnabled(enabled && !running && !blocked);
        }
        if (playPCDropBox_) {
            playPCDropBox_->setEnabled(enabled && !running && !blocked);
        }
        if (playPCShowSolutionButton_) {
            playPCShowSolutionButton_->setText(
                pcScoutSolutionRequested_ ? "Hide Solution" : "Show Solution");
            const bool canEnablePreview =
                !running
                && process_ == nullptr
                && auxiliaryScoutProcess_ == nullptr
                && pcScoutSuccessfulTarget_.has_value();
            playPCShowSolutionButton_->setEnabled(
                enabled && (pcScoutSolutionRequested_ || canEnablePreview));
        }
        if (playPCCancelButton_) {
            playPCCancelButton_->setEnabled(running);
        }
        updateAuxiliaryScoutControls();
    }

    void invalidatePCScout(
        const QString &status = "Board changed; checking again",
        bool preserveSolutionOverlay = false,
        bool preserveSolutionRequest = false
    ) {
        if (pcScoutRefreshTimer_) {
            pcScoutRefreshTimer_->stop();
        }
        if (pcScoutProcess_) {
            QProcess *process = pcScoutProcess_;
            pcScoutProcess_ = nullptr;
            process->terminate();
            QTimer::singleShot(250, process, [process]() {
                if (process->state() != QProcess::NotRunning) {
                    process->kill();
                }
            });
        }
        pcScoutTargets_.clear();
        pcScoutResultLines_.clear();
        pcScoutOutput_.clear();
        pcScoutFound_ = false;
        pcScoutBuildingSolution_ = false;
        pcScoutSuccessfulTarget_.reset();
        pcScoutSuccessfulPattern_.clear();
        pcScoutSuccessfulFieldPath_.clear();
        pcScoutSuccessfulDrop_.clear();
        if (!preserveSolutionOverlay) {
            pcScoutSolutionCells_.fill(0);
            pcScoutSolutionVisible_ = false;
        }
        if (!preserveSolutionRequest) {
            pcScoutSolutionRequested_ = false;
            pcScoutSolutionNeedsRefresh_ = false;
        }
        if (playPCResultsLabel_) {
            playPCResultsLabel_->clear();
        }
        if (playPCStatusLabel_) {
            playPCStatusLabel_->setText(status);
        }
        refreshPlaySolutionOverlay();
        updatePCScoutControls();
    }

    void suspendPCScoutSolutionPreview() {
        if (!pcScoutSolutionRequested_) {
            return;
        }
        pcScoutSolutionCells_.fill(0);
        pcScoutSolutionVisible_ = false;
        pcScoutSolutionNeedsRefresh_ = true;
        refreshPlaySolutionOverlay();
        updatePCScoutControls();
    }

    void schedulePCScout(bool immediate = false, bool afterPlacement = false) {
        const bool enabled = playPCEnabledCheck_ && playPCEnabledCheck_->isChecked();
        const bool hadPendingRefresh = pcScoutRefreshTimer_ && pcScoutRefreshTimer_->isActive();
        if (!enabled) {
            const bool needsReset = pcScoutProcess_
                || (pcScoutRefreshTimer_ && pcScoutRefreshTimer_->isActive())
                || !pcScoutTargets_.empty()
                || !pcScoutResultLines_.isEmpty()
                || !pcScoutOutput_.isEmpty()
                || (playPCStatusLabel_ && playPCStatusLabel_->text() != "PC Scout is off");
            if (needsReset) {
                invalidatePCScout("PC Scout is off");
            }
            return;
        }
        const bool clearedLine = afterPlacement && playGame_.last_clear_lines > 0;
        const bool preserveRequest = pcScoutSolutionRequested_;
        const bool preserveOverlay = preserveRequest && !clearedLine;
        if (preserveRequest && (clearedLine || !pcScoutSolutionVisible_)) {
            pcScoutSolutionNeedsRefresh_ = true;
        }
        const bool wasRunning = pcScoutProcess_ != nullptr;
        std::array<SFTPCScoutCandidate, 1> candidate{};
        if (sft_game_pc_scout_candidates(&playGame_, 7, candidate.data(), 1) <= 0) {
            suspendPCScoutSolutionPreview();
            invalidatePCScout(
                "No perfect-clear window exists within 7 pieces",
                false,
                preserveRequest);
            return;
        }
        invalidatePCScout(
            "Waiting to check the current position...",
            preserveOverlay,
            preserveRequest);
        if (!pcScoutRefreshTimer_) {
            return;
        }
        const int delayMs = immediate ? 0 : (afterPlacement ? (wasRunning || hadPendingRefresh ? 75 : 0) : 250);
        pcScoutRefreshTimer_->start(delayMs);
    }

    void runPCScout() {
        if (!playPCEnabledCheck_ || !playPCEnabledCheck_->isChecked()) {
            if (playPCStatusLabel_) {
                playPCStatusLabel_->setText("PC Scout is off");
            }
            return;
        }
        if (process_ || auxiliaryScoutProcess_) {
            playPCStatusLabel_->setText("Another sfinder search is running");
            return;
        }
        if (pcScoutProcess_) {
            return;
        }

        std::array<SFTPCScoutCandidate, 2> candidates{};
        const int candidateCount = sft_game_pc_scout_candidates(
            &playGame_, 7, candidates.data(), static_cast<int>(candidates.size()));
        pcScoutResultLines_.clear();
        pcScoutFound_ = false;
        playPCResultsLabel_->clear();
        if (candidateCount <= 0) {
            suspendPCScoutSolutionPreview();
            playPCStatusLabel_->setText("No perfect-clear window exists within 7 pieces");
            DiagnosticLog::instance().append("PC Scout found no eligible window within 7 pieces.");
            return;
        }

        pcScoutTargets_.clear();
        for (int index = 0; index < std::min(candidateCount, static_cast<int>(candidates.size())); ++index) {
            pcScoutTargets_.push_back({candidates[index].pieces, candidates[index].clear_lines});
        }
        pcScoutGame_ = playGame_;
        pcScoutUsesActiveQueue_ = playPCSourceBox_->currentData().toString() == "active";
        pcScoutTargetIndex_ = 0;
        updatePCScoutControls();
        startNextPCScoutTarget();
    }

    void startNextPCScoutTarget() {
        if (pcScoutTargetIndex_ >= static_cast<int>(pcScoutTargets_.size())) {
            updatePCScoutControls();
            if (!pcScoutFound_) {
                suspendPCScoutSolutionPreview();
            }
            if (pcScoutSolutionRequested_
                && pcScoutSolutionNeedsRefresh_
                && pcScoutSuccessfulTarget_.has_value()) {
                startPCScoutSolution();
                return;
            }
            playPCStatusLabel_->setText(
                pcScoutFound_ ? "PC Scout finished" : "No perfect clear found in the checked windows");
            return;
        }

        const PCScoutTarget target = pcScoutTargets_[pcScoutTargetIndex_];
        playPCStatusLabel_->setText(QString("Checking %1-piece window...").arg(target.pieces));
        std::array<char, SFT_GAME_FIELD_TEXT_CAPACITY> field{};
        const int fieldLength = sft_game_write_sfinder_field(
            &pcScoutGame_,
            target.clearLines,
            field.data(),
            static_cast<int>(field.size()));
        if (fieldLength <= 0) {
            updatePCScoutControls();
            playPCStatusLabel_->setText("Could not prepare the current field");
            return;
        }
        pcScoutFieldPath_ = writeTextFile(
            "pc-scout-field.txt",
            QString::fromUtf8(field.data(), fieldLength));
        const int patternDepth = pcScoutUsesActiveQueue_
            ? target.pieces
            : std::min(target.pieces + 1, 7);
        QString searchPattern = QString("*p%1").arg(patternDepth);
        if (pcScoutUsesActiveQueue_) {
            std::array<char, SFT_GAME_PATTERN_TEXT_CAPACITY> pattern{};
            const int patternLength = sft_game_write_active_patterns(
                &pcScoutGame_,
                patternDepth,
                pattern.data(),
                static_cast<int>(pattern.size()));
            if (patternLength <= 0) {
                updatePCScoutControls();
                playPCStatusLabel_->setText("Could not read the active piece queue");
                return;
            }
            searchPattern = QString::fromLatin1(pattern.data(), patternLength);
        }
        const QString targetFieldPath = pcScoutFieldPath_;
        const QString targetDrop = playPCDropBox_->currentData().toString();
        const bool targetUsesActiveQueue = pcScoutUsesActiveQueue_;
        auto launch = sfinderLaunchCommand();
        QStringList args = launch.second;
        args << "percent"
             << "-fp" << targetFieldPath
             << "-c" << QString::number(target.clearLines)
             << "-p" << searchPattern
             << "-H" << (targetUsesActiveQueue ? "avoid" : "use")
             << "-d" << targetDrop
             << "-K" << "srs"
             << "-th" << "1"
             << "-fc" << "0"
             << "-td" << "0";

        pcScoutOutput_.clear();
        DiagnosticLog::instance().append(
            QString("PC Scout launched: %1 %2").arg(launch.first, args.join(" ")));
        QProcess *process = new QProcess(this);
        pcScoutProcess_ = process;
        updatePCScoutControls();
        process->setWorkingDirectory(repoRoot_);
        process->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
        process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
            constexpr unsigned long kCreateNoWindow = 0x08000000UL;
            arguments->flags |= kCreateNoWindow;
        });
#endif
        connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
            if (pcScoutProcess_ == process) {
                pcScoutOutput_ += QString::fromLocal8Bit(process->readAllStandardOutput());
            } else {
                process->readAllStandardOutput();
            }
        });
        connect(process, &QProcess::finished, this, [
                    this,
                    process,
                    target,
                    searchPattern,
                    targetFieldPath,
                    targetDrop,
                    targetUsesActiveQueue
                ](int exitCode, QProcess::ExitStatus exitStatus) {
            const QString output = pcScoutOutput_ + QString::fromLocal8Bit(process->readAllStandardOutput());
            process->deleteLater();
            if (pcScoutProcess_ != process) {
                return;
            }
            pcScoutProcess_ = nullptr;

            const QRegularExpression resultPattern(
                R"(success\s*=\s*([0-9]+(?:\.[0-9]+)?)%\s*\((\d+)/(\d+)\))");
            const QRegularExpressionMatch match = resultPattern.match(output);
            if (exitStatus != QProcess::NormalExit || exitCode != 0 || !match.hasMatch()) {
                updatePCScoutControls();
                playPCStatusLabel_->setText(
                    exitStatus == QProcess::NormalExit && exitCode == 15
                        ? "PC Scout canceled"
                        : "PC Scout could not read sfinder's result");
                appendRawOutput(
                    QString("\nPC Scout failed for %1 pieces:\n%2\n").arg(target.pieces).arg(output));
                DiagnosticLog::instance().appendBlock(
                    QString("PC Scout failed for %1 pieces").arg(target.pieces),
                    {QString("exit %1").arg(exitCode), output.trimmed()});
                return;
            }

            const QString percent = match.captured(1);
            const QString successes = match.captured(2);
            const QString total = match.captured(3);
            pcScoutFound_ = pcScoutFound_ || successes.toInt() > 0;
            if (successes.toInt() > 0 && !pcScoutSuccessfulTarget_.has_value()) {
                pcScoutSuccessfulTarget_ = target;
                pcScoutSuccessfulPattern_ = searchPattern;
                pcScoutSuccessfulFieldPath_ = targetFieldPath;
                pcScoutSuccessfulDrop_ = targetDrop;
                pcScoutSuccessfulUsesActiveQueue_ = targetUsesActiveQueue;
            }
            const QString result = pcScoutUsesActiveQueue_
                ? (successes.toInt() > 0 ? "Available" : "No PC")
                : percent + "%";
            pcScoutResultLines_ << QString(
                "<b>PC in %1 pieces: %2</b><br><span style=\"color:#b7b7b7\">%3/%4 %5 · %6 lines</span>")
                .arg(target.pieces)
                .arg(result)
                .arg(successes)
                .arg(total)
                .arg(pcScoutUsesActiveQueue_ ? "hold routes" : "bags")
                .arg(target.clearLines);
            playPCResultsLabel_->setText(pcScoutResultLines_.join("<br><br>"));
            DiagnosticLog::instance().append(
                QString("PC Scout %1-piece result: %2/%3 (%4%)")
                    .arg(target.pieces)
                    .arg(successes)
                    .arg(total)
                    .arg(percent));
            ++pcScoutTargetIndex_;
            startNextPCScoutTarget();
        });

        process->start(launch.first, args);
        if (!process->waitForStarted(1000)) {
            pcScoutProcess_ = nullptr;
            process->deleteLater();
            updatePCScoutControls();
            playPCStatusLabel_->setText("PC Scout failed to start sfinder");
            DiagnosticLog::instance().append("PC Scout failed to start sfinder.");
        }
    }

    std::optional<std::array<int, kColumns * kRows>> pcScoutOverlayForCode(
        const QString &code,
        int *addedCellCount = nullptr
    ) const {
        return scoutOverlayForCode(code, pcScoutGame_, false, addedCellCount);
    }

    void togglePCScoutSolution() {
        if (pcScoutSolutionRequested_) {
            pcScoutSolutionRequested_ = false;
            pcScoutSolutionNeedsRefresh_ = false;
            if (pcScoutBuildingSolution_ && pcScoutProcess_) {
                cancelPCScout();
            }
            pcScoutSolutionVisible_ = false;
            pcScoutSolutionCells_.fill(0);
            refreshPlaySolutionOverlay();
            if (playPCStatusLabel_) {
                playPCStatusLabel_->setText("Solution preview hidden");
            }
            updatePCScoutControls();
            return;
        }

        activeAuxiliaryPreviewMode_.clear();
        updateAuxiliaryScoutControls();
        pcScoutSolutionRequested_ = true;
        const bool hasCachedOverlay = std::any_of(
            pcScoutSolutionCells_.begin(),
            pcScoutSolutionCells_.end(),
            [](int value) { return value > 0; });
        if (hasCachedOverlay) {
            pcScoutSolutionVisible_ = true;
            refreshPlaySolutionOverlay();
            playPCStatusLabel_->setText("Showing the perfect-clear solution");
            updatePCScoutControls();
            return;
        }
        if (!pcScoutSuccessfulTarget_.has_value()) {
            playPCStatusLabel_->setText("Waiting for PC Scout to find a solution");
            updatePCScoutControls();
            return;
        }
        startPCScoutSolution();
    }

    void startPCScoutSolution() {
        if (!pcScoutSolutionRequested_
            || !pcScoutSuccessfulTarget_.has_value()
            || pcScoutProcess_
            || process_
            || auxiliaryScoutProcess_) {
            updatePCScoutControls();
            return;
        }

        const PCScoutTarget target = pcScoutSuccessfulTarget_.value();
        QDir runDir(appDataDir());
        runDir.mkpath("run");
        const QString outputBase = runDir.filePath("run/pc-scout-solution");
        QFile::remove(outputBase + "_minimal.html");
        QFile::remove(outputBase + "_unique.html");

        auto launch = sfinderLaunchCommand();
        QStringList args = launch.second;
        args << "path"
             << "-fp" << pcScoutSuccessfulFieldPath_
             << "-c" << QString::number(target.clearLines)
             << "-p" << pcScoutSuccessfulPattern_
             << "-H" << (pcScoutSuccessfulUsesActiveQueue_ ? "avoid" : "use")
             << "-d" << pcScoutSuccessfulDrop_
             << "-K" << "srs"
             << "-th" << "1"
             << "-f" << "html"
             << "-o" << outputBase
             << "-s" << "no"
             << "-so" << "yes";

        pcScoutOutput_.clear();
        pcScoutBuildingSolution_ = true;
        playPCStatusLabel_->setText("Finding a concrete PC route...");
        DiagnosticLog::instance().append(
            QString("PC solution preview launched: %1 %2").arg(launch.first, args.join(" ")));

        QProcess *process = new QProcess(this);
        pcScoutProcess_ = process;
        updatePCScoutControls();
        process->setWorkingDirectory(repoRoot_);
        process->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
        process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
            constexpr unsigned long kCreateNoWindow = 0x08000000UL;
            arguments->flags |= kCreateNoWindow;
        });
#endif
        connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
            if (pcScoutProcess_ == process) {
                pcScoutOutput_ += QString::fromLocal8Bit(process->readAllStandardOutput());
            } else {
                process->readAllStandardOutput();
            }
        });
        connect(process, &QProcess::finished, this, [
                    this,
                    process,
                    outputBase,
                    target
                ](int exitCode, QProcess::ExitStatus exitStatus) {
            const QString output = pcScoutOutput_ + QString::fromLocal8Bit(process->readAllStandardOutput());
            process->deleteLater();
            if (pcScoutProcess_ != process) {
                return;
            }
            pcScoutProcess_ = nullptr;
            pcScoutBuildingSolution_ = false;

            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                playPCStatusLabel_->setText("Could not generate the PC solution");
                DiagnosticLog::instance().appendBlock(
                    "PC solution preview failed",
                    {QString("exit %1").arg(exitCode), output.trimmed()});
                updatePCScoutControls();
                return;
            }

            QString resultPath = outputBase + "_minimal.html";
            if (!QFileInfo::exists(resultPath)) {
                resultPath = outputBase + "_unique.html";
            }
            QFile resultFile(resultPath);
            if (!resultFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                playPCStatusLabel_->setText("sfinder did not create a readable solution");
                DiagnosticLog::instance().append(
                    "PC solution preview failed: no readable path output.");
                updatePCScoutControls();
                return;
            }

            const QString html = QString::fromUtf8(resultFile.readAll());
            const QRegularExpression codePattern("v115@[A-Za-z0-9+/\\?]+");
            QRegularExpressionMatchIterator matches = codePattern.globalMatch(html);
            QStringList codes;
            while (matches.hasNext()) {
                const QString code = matches.next().captured(0);
                if (!codes.contains(code)) {
                    codes.push_back(code);
                }
            }

            std::optional<std::array<int, kColumns * kRows>> overlay;
            int overlayCells = 0;
            for (int index = codes.size() > 1 ? 1 : 0; index < codes.size(); ++index) {
                overlay = pcScoutOverlayForCode(codes[index], &overlayCells);
                if (overlay.has_value()) {
                    break;
                }
            }
            if (!overlay.has_value() && codes.size() > 1) {
                overlay = pcScoutOverlayForCode(codes.front(), &overlayCells);
            }
            if (!overlay.has_value()) {
                playPCStatusLabel_->setText("Could not decode sfinder's solution");
                DiagnosticLog::instance().appendBlock(
                    "PC solution preview decode failed",
                    {QString("Fumen links found: %1").arg(codes.size()), resultPath});
                updatePCScoutControls();
                return;
            }

            pcScoutSolutionCells_ = overlay.value();
            pcScoutSolutionVisible_ = true;
            pcScoutSolutionNeedsRefresh_ = false;
            refreshPlaySolutionOverlay();
            playPCStatusLabel_->setText(
                QString("Showing a %1-piece PC solution").arg(target.pieces));
            DiagnosticLog::instance().append(
                QString("PC solution preview displayed %1 translucent cells from %2.")
                    .arg(overlayCells)
                    .arg(resultPath));
            updatePCScoutControls();
        });

        process->start(launch.first, args);
        if (!process->waitForStarted(1000)) {
            pcScoutProcess_ = nullptr;
            pcScoutBuildingSolution_ = false;
            process->deleteLater();
            playPCStatusLabel_->setText("PC solution search failed to start");
            DiagnosticLog::instance().append("PC solution preview failed to start sfinder.");
            updatePCScoutControls();
        }
    }

    void cancelPCScout() {
        if (!pcScoutProcess_) {
            return;
        }
        const bool wasBuildingSolution = pcScoutBuildingSolution_;
        QProcess *process = pcScoutProcess_;
        pcScoutProcess_ = nullptr;
        pcScoutBuildingSolution_ = false;
        process->terminate();
        QTimer::singleShot(250, process, [process]() {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
            }
        });
        updatePCScoutControls();
        playPCStatusLabel_->setText(
            wasBuildingSolution ? "Solution preview canceled" : "PC Scout canceled");
        DiagnosticLog::instance().append(
            wasBuildingSolution ? "PC solution preview canceled." : "PC Scout canceled.");
    }

    QString outputBaseForCommand(const QString &command) const {
        QDir dir(appDataDir());
        dir.mkpath("run");
        const QString normalized = command;
        if (normalized == "setup") {
            return dir.filePath("run/qt_setup.html");
        }
        if (normalized == "tetris-path") {
            return dir.filePath("run/qt_tetris_path");
        }
        return dir.filePath("run/qt_" + normalized);
    }

    QStringList buildSfinderArguments(const QString &fieldPath, const QString &patternsPath, const QString &outputBase) const {
        const QString command = commandBox_->currentText();
        QString sfCommand = command;
        QStringList args;
        if (command == "tetris-path") {
            sfCommand = "path";
        }
        args << sfCommand;

        const QString fumenCode = fumenEdit_->toPlainText().trimmed();
        if (command == "cover" && !fumenCode.isEmpty()) {
            args << "-t" << fumenCode;
        } else {
            args << "-fp" << fieldPath;
        }

        args << "-pp" << patternsPath;

        if (command != "spin") {
            args << "-H" << holdBox_->currentText();
            args << "-d" << dropBox_->currentText();
            args << "-K" << "srs";
        }

        if (command == "percent" || command == "path" || command == "tetris" || command == "tetris-path") {
            args << "-c" << QString::number(linesSpin_->value());
        }
        if (command == "tetris-path") {
            args << "-sc" << "tetris-end";
        }
        if (command == "tetris") {
            args << "-lp" << outputBase;
        }
        if (command == "path" || command == "tetris-path") {
            args << "-f" << "html";
            args << "-o" << outputBase;
        }
        if (command == "setup") {
            args << "-l" << QString::number(autoCommandHeight());
            args << "-f" << "I";
            args << "-m" << "O";
            args << "-fo" << "html";
            args << "-o" << outputBase;
        }
        if (command == "ren") {
            args << "-o" << outputBase;
        }
        if (command == "cover") {
            args << "-o" << outputBase;
        }
        if (command == "spin") {
            args << "-c" << QString::number(linesSpin_->value());
            args << "-fb" << "0";
            args << "-ft" << QString::number(autoCommandHeight());
            args << "-f" << "none";
            args << "-fo" << "html";
            args << "-o" << outputBase;
        }
        return args;
    }

    void runSearch() {
        if (process_ || pcScoutProcess_ || auxiliaryScoutProcess_) {
            return;
        }

        const QString command = commandBox_->currentText();
        bool setupOk = true;
        QString setupError;
        const QString fieldText = command == "setup" ? setupFieldText(&setupOk, &setupError) : generatedFieldText();
        if (!setupOk) {
            showingOutputFileContent_ = false;
            rawOutputLog_ = "Setup input failed: " + setupError + "\n";
            refreshDisplayedOutput();
            DiagnosticLog::instance().append("Setup input failed: " + setupError);
            return;
        }
        const QString fieldPath = writeTextFile(command == "setup" ? "setup-field.txt" : "field.txt", fieldText);
        const QString patterns = patternsEdit_->text().trimmed().isEmpty() ? "*p7" : patternsEdit_->text().trimmed();
        const QString patternsPath = writeTextFile("patterns.txt", patterns + "\n");
        const QString outputBase = outputBaseForCommand(command);

        auto launch = sfinderLaunchCommand();
        const QString program = launch.first;
        QStringList args = launch.second;
        args << buildSfinderArguments(fieldPath, patternsPath, outputBase);
        DiagnosticLog::instance().append(
            QString("sfinder launched: %1 %2").arg(program, args.join(" ")));

        showingOutputFileContent_ = false;
        rawOutputLog_ = "$ " + program + " " + args.join(" ") + "\n\n";
        refreshDisplayedOutput();
        if (outputFileBox_) {
            const int commandIndex = outputFileBox_->findData("__command_output__");
            if (commandIndex >= 0) {
                outputFileBox_->setCurrentIndex(commandIndex);
            }
        }
        refreshDisplayedOutput();

        process_ = new QProcess(this);
        process_->setWorkingDirectory(repoRoot_);
        process_->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
        process_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
            constexpr unsigned long kCreateNoWindow = 0x08000000UL;
            arguments->flags |= kCreateNoWindow;
        });
#endif

        connect(process_, &QProcess::readyReadStandardOutput, this, [this]() {
            appendRawOutput(QString::fromLocal8Bit(process_->readAllStandardOutput()));
        });
        connect(process_, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
            appendRawOutput(QString("\nProcess finished: exit %1 (%2)\n")
                                .arg(exitCode)
                                .arg(status == QProcess::NormalExit ? "normal" : "crashed"));
            process_->deleteLater();
            process_ = nullptr;
            runButton_->setEnabled(true);
            cancelButton_->setEnabled(false);
            refreshGeneratedFiles();
            updatePCScoutControls();
            updateAuxiliaryScoutControls();
            DiagnosticLog::instance().append(
                QString("sfinder finished: exit %1 (%2)")
                    .arg(exitCode)
                    .arg(status == QProcess::NormalExit ? "normal" : "crashed"));
        });

        runButton_->setEnabled(false);
        cancelButton_->setEnabled(true);
        updatePCScoutControls();
        updateAuxiliaryScoutControls();
        process_->start(program, args);
        if (!process_->waitForStarted(1000)) {
            appendRawOutput("Failed to start sfinder process.\n");
            process_->deleteLater();
            process_ = nullptr;
            runButton_->setEnabled(true);
            cancelButton_->setEnabled(false);
            updatePCScoutControls();
            updateAuxiliaryScoutControls();
            DiagnosticLog::instance().append("sfinder failed to start.");
        }
    }

    void cancelSearch() {
        if (!process_) {
            return;
        }
        process_->terminate();
        if (!process_->waitForFinished(1500)) {
            process_->kill();
        }
        DiagnosticLog::instance().append("sfinder search canceled.");
    }

    QString repoRoot_;
    std::vector<Opener> openers_;
    BoardWidget *board_ = nullptr;
    QFormLayout *searchForm_ = nullptr;
    QComboBox *commandBox_ = nullptr;
    QComboBox *holdBox_ = nullptr;
    QComboBox *dropBox_ = nullptr;
    QSpinBox *linesSpin_ = nullptr;
    QLineEdit *spinHeightEdit_ = nullptr;
    QLineEdit *patternsEdit_ = nullptr;
    QCheckBox *verboseCheck_ = nullptr;
    QComboBox *openerGroupBox_ = nullptr;
    QComboBox *openerVariationBox_ = nullptr;
    QCheckBox *placeMinoCheck_ = nullptr;
    QGroupBox *paintGroup_ = nullptr;
    QGroupBox *minoGroup_ = nullptr;
    QGroupBox *pagesGroup_ = nullptr;
    QComboBox *minoPieceBox_ = nullptr;
    QLabel *pageLabel_ = nullptr;
    QPushButton *prevPageButton_ = nullptr;
    QPushButton *nextPageButton_ = nullptr;
    QPushButton *addPageButton_ = nullptr;
    QPushButton *trimBeforePagesButton_ = nullptr;
    QPushButton *trimPagesButton_ = nullptr;
    QPlainTextEdit *fumenEdit_ = nullptr;
    QPlainTextEdit *generatedField_ = nullptr;
    QPlainTextEdit *outputEdit_ = nullptr;
    QListWidget *outputFilesList_ = nullptr;
    QTabBar *sectionTabs_ = nullptr;
    QStackedWidget *centerStack_ = nullptr;
    QWidget *searchSettingsPanel_ = nullptr;
    QWidget *playSettingsPanel_ = nullptr;
    BoardWidget *playBoard_ = nullptr;
    QLabel *playStatusLabel_ = nullptr;
    QLabel *playPiecesLabel_ = nullptr;
    QLabel *playLinesLabel_ = nullptr;
    QLabel *playLevelLabel_ = nullptr;
    QLabel *playPpsLabel_ = nullptr;
    QLabel *playClearLabel_ = nullptr;
    QLabel *playDetectionLabel_ = nullptr;
    QComboBox *playPCDropBox_ = nullptr;
    QCheckBox *playPCEnabledCheck_ = nullptr;
    QComboBox *playPCSourceBox_ = nullptr;
    QPushButton *playPCShowSolutionButton_ = nullptr;
    QPushButton *playPCCancelButton_ = nullptr;
    QLabel *playPCResultsLabel_ = nullptr;
    QLabel *playPCStatusLabel_ = nullptr;
    QTabWidget *playScoutTabs_ = nullptr;
    QComboBox *playSpinScoutLinesBox_ = nullptr;
    QSpinBox *playSpinScoutPiecesSpin_ = nullptr;
    QCheckBox *playSpinScoutAutoCheck_ = nullptr;
    QPushButton *playSpinScoutButton_ = nullptr;
    QPushButton *playSpinScoutCancelButton_ = nullptr;
    QPushButton *playSpinScoutPreviewButton_ = nullptr;
    QLabel *playSpinScoutResultLabel_ = nullptr;
    QLabel *playSpinScoutStatusLabel_ = nullptr;
    QSpinBox *playRenScoutPiecesSpin_ = nullptr;
    QComboBox *playRenScoutDropBox_ = nullptr;
    QCheckBox *playRenScoutAutoCheck_ = nullptr;
    QPushButton *playRenScoutButton_ = nullptr;
    QPushButton *playRenScoutCancelButton_ = nullptr;
    QPushButton *playRenScoutPreviewButton_ = nullptr;
    QLabel *playRenScoutResultLabel_ = nullptr;
    QLabel *playRenScoutStatusLabel_ = nullptr;
    QLineEdit *playQueueEdit_ = nullptr;
    QComboBox *playHoldBox_ = nullptr;
    std::array<QLineEdit *, 10> playControlEdits_{};
    QSpinBox *playDasSpin_ = nullptr;
    QSpinBox *playArrSpin_ = nullptr;
    QSpinBox *playSoftSpin_ = nullptr;
    QComboBox *playGravityLevelBox_ = nullptr;
    QSpinBox *playLockSpin_ = nullptr;
    QSpinBox *playMoveResetLimitSpin_ = nullptr;
    QSpinBox *playPreviewSpin_ = nullptr;
    QCheckBox *playGravityCheck_ = nullptr;
    QCheckBox *playLevelProgressionCheck_ = nullptr;
    QCheckBox *playMoveResetCheck_ = nullptr;
    QCheckBox *playStepResetCheck_ = nullptr;
    QCheckBox *playInfiniteLockCheck_ = nullptr;
    QCheckBox *playInfiniteHoldCheck_ = nullptr;
    QCheckBox *playExportActiveCheck_ = nullptr;
    PiecePreviewWidget *playHoldPreview_ = nullptr;
    std::array<PiecePreviewWidget *, 5> playNextPreviews_{};
    QPushButton *playUndoButton_ = nullptr;
    QTimer *playTimer_ = nullptr;
    QTimer *playInputTimer_ = nullptr;
    QTimer *playFumenSyncTimer_ = nullptr;
    QTextBrowser *centerOutputBrowser_ = nullptr;
    QComboBox *outputFileBox_ = nullptr;
    BoardWidget *previewBoard_ = nullptr;
    QComboBox *previewCodeBox_ = nullptr;
    QLabel *previewPageLabel_ = nullptr;
    QPushButton *screenshotButton_ = nullptr;
    QPushButton *runButton_ = nullptr;
    QPushButton *cancelButton_ = nullptr;
    std::vector<int> paintValues_;
    std::vector<QPushButton *> paintButtons_;
    std::vector<std::array<int, kFumenBlocks>> fumenPages_;
    std::vector<FumenOperation> fumenOperations_;
    SFTGameState playGame_{};
    SFTGameState pcScoutGame_{};
    SFTGameState auxiliaryScoutGame_{};
    std::vector<PlayUndoSnapshot> playUndoStack_;
    QHash<int, HeldPlayInput> heldPlayInputs_;
    bool updatingPlayLevelLock_ = false;
    int playConfiguredGravityLevel_ = 1;
    int playConfiguredLockDelay_ = 500;
    QElapsedTimer playFrameClock_;
    QElapsedTimer playInputClock_;
    quint64 playRenderSerial_ = 0;
    int playStatsRefreshElapsedMs_ = 0;
    QElapsedTimer playStatsClock_;
    int playInputPressCounter_ = 0;
    int playLastExportedPieces_ = 0;
    int playOpeningCycleStartPieces_ = 0;
    bool playHasStarted_ = false;
    bool playFumenEditorDirty_ = false;
    bool loadingPlaySettings_ = false;
    bool playOpenerDetectionDone_ = false;
    bool playVariantDetectionDone_ = false;
    bool playEarlyVariantDetection_ = false;
    QString playDetectedOpenerName_;
    std::optional<bool> playDetectedOpenerMirrored_;
    std::vector<std::array<int, kFumenBlocks>> previewPages_;
    std::vector<FumenOperation> previewOperations_;
    QString currentPreviewCode_;
    QString rawOutputLog_;
    int currentPreviewPage_ = 0;
    FumenOperation currentOperation_;
    int currentFumenPage_ = 0;
    bool updatingFumenEdit_ = false;
    bool updatingFumenControls_ = false;
    bool loadingOpeners_ = false;
    bool showingOutputFileContent_ = false;
    bool setupModeActive_ = false;
    QProcess *process_ = nullptr;
    QProcess *pcScoutProcess_ = nullptr;
    QProcess *auxiliaryScoutProcess_ = nullptr;
    QTimer *auxiliaryScoutRefreshTimer_ = nullptr;
    QStringList auxiliaryAutoQueue_;
    bool auxiliaryScoutAutomaticRun_ = false;
    QString auxiliaryScoutMode_;
    QString auxiliaryScoutOutput_;
    QString playSpinScoutFumen_;
    QString playRenScoutFumen_;
    std::array<int, kColumns * kRows> playSpinScoutOverlayCells_{};
    std::array<int, kColumns * kRows> playRenScoutOverlayCells_{};
    QString activeAuxiliaryPreviewMode_;
    QTimer *pcScoutRefreshTimer_ = nullptr;
    std::vector<PCScoutTarget> pcScoutTargets_;
    QStringList pcScoutResultLines_;
    QString pcScoutOutput_;
    QString pcScoutFieldPath_;
    std::optional<PCScoutTarget> pcScoutSuccessfulTarget_;
    QString pcScoutSuccessfulPattern_;
    QString pcScoutSuccessfulFieldPath_;
    QString pcScoutSuccessfulDrop_;
    std::array<int, kColumns * kRows> pcScoutSolutionCells_{};
    int pcScoutTargetIndex_ = 0;
    bool pcScoutFound_ = false;
    bool pcScoutUsesActiveQueue_ = true;
    bool pcScoutSuccessfulUsesActiveQueue_ = true;
    bool pcScoutBuildingSolution_ = false;
    bool pcScoutSolutionRequested_ = false;
    bool pcScoutSolutionNeedsRefresh_ = false;
    bool pcScoutSolutionVisible_ = false;
    QPointer<OpenerImporterDialog> openerImporterDialog_;
    QPointer<OutputLogDialog> outputLogDialog_;
};

void applyAppTheme(QApplication &app) {
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#303030"));
    palette.setColor(QPalette::WindowText, QColor("#f0f0f0"));
    palette.setColor(QPalette::Base, QColor("#1f1f1f"));
    palette.setColor(QPalette::AlternateBase, QColor("#282828"));
    palette.setColor(QPalette::ToolTipBase, QColor("#f0f0f0"));
    palette.setColor(QPalette::ToolTipText, QColor("#202020"));
    palette.setColor(QPalette::Text, QColor("#f0f0f0"));
    palette.setColor(QPalette::Button, QColor("#666666"));
    palette.setColor(QPalette::ButtonText, QColor("#f4f4f4"));
    palette.setColor(QPalette::BrightText, QColor("#ffffff"));
    palette.setColor(QPalette::Highlight, QColor("#0a84ff"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    app.setPalette(palette);

    app.setStyleSheet(R"(
        QWidget {
            background-color: #303030;
            color: #eeeeee;
            font-size: 13px;
        }
        QLabel#paneTitle {
            font-size: 18px;
            font-weight: 750;
            color: #f2f2f2;
        }
        QLabel#paneSubtitle {
            color: #b7b7b7;
            font-size: 12px;
        }
        QGroupBox {
            border: 1px solid #555555;
            border-radius: 6px;
            margin-top: 12px;
            padding: 10px 8px 8px 8px;
            font-weight: 650;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
            color: #dddddd;
        }
        QPushButton {
            background-color: #666666;
            color: #f4f4f4;
            border: 1px solid #707070;
            border-radius: 6px;
            padding: 5px 12px;
            font-weight: 650;
        }
        QPushButton:hover {
            background-color: #747474;
        }
        QPushButton:pressed {
            background-color: #555555;
        }
        QPushButton:disabled {
            color: #9a9a9a;
            background-color: #484848;
            border-color: #555555;
        }
        QPushButton#primaryButton {
            background-color: #0a6edb;
            border-color: #1884ef;
        }
        QPushButton#primaryButton:hover {
            background-color: #117ce8;
        }
        QComboBox, QSpinBox, QLineEdit, QPlainTextEdit {
            background-color: #232323;
            color: #f1f1f1;
            border: 1px solid #555555;
            border-radius: 5px;
            padding: 4px 6px;
            selection-background-color: #0a84ff;
        }
        QPlainTextEdit {
            font-family: "Menlo", "SF Mono", "DejaVu Sans Mono", monospace;
        }
        QTabWidget::pane {
            border: 1px solid #555555;
            border-radius: 5px;
            top: -1px;
        }
        QTabBar::tab {
            background: #4f4f4f;
            color: #eeeeee;
            border: 1px solid #5f5f5f;
            padding: 5px 18px;
            min-width: 66px;
        }
        QTabBar::tab:selected {
            background: #777777;
            color: #ffffff;
        }
        QTabBar::tab:first {
            border-top-left-radius: 5px;
            border-bottom-left-radius: 5px;
        }
        QTabBar::tab:last {
            border-top-right-radius: 5px;
            border-bottom-right-radius: 5px;
        }
        QCheckBox {
            spacing: 6px;
        }
        QSplitter#mainSplitter::handle {
            background-color: #555555;
        }
        QScrollArea, QScrollArea > QWidget > QWidget {
            background-color: #303030;
        }
    )");
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Solution Finder Enhanced");
    QGuiApplication::setApplicationDisplayName("Solution Finder Enhanced");
    QApplication::setOrganizationName("rustednuts69");
    QApplication::setWindowIcon(QIcon(":/icons/app-icon.png"));
    applyAppTheme(app);

    MainWindow window;
    window.show();
    return app.exec();
}
