#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QRubberBand>
#include <QScreen>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QSysInfo>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QStyleFactory>

#include <array>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <random>
#include <vector>

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

struct PlayPiece {
    int type = 0;
    int rotation = 0;
    int x = 4;
    int y = 1;
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

class ScreenCaptureOverlay : public QWidget {
public:
    explicit ScreenCaptureOverlay(QWidget *parent = nullptr)
        : QWidget(parent), rubberBand_(new QRubberBand(QRubberBand::Rectangle, this)) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
        setCursor(Qt::CrossCursor);
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            screen_ = screen;
            setGeometry(screen->geometry());
            background_ = screen->grabWindow(0);
        }
    }

    static std::optional<QImage> capture(QWidget *parent = nullptr) {
        ScreenCaptureOverlay overlay(parent);
        if (!overlay.screen_) {
            return std::nullopt;
        }
        overlay.showFullScreen();
        overlay.raise();
        overlay.activateWindow();
        overlay.loop_.exec();
        return overlay.result_;
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        if (!background_.isNull()) {
            painter.drawPixmap(rect(), background_);
        }
        painter.fillRect(rect(), QColor(0, 0, 0, 96));
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape) {
            loop_.quit();
            close();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override {
        origin_ = event->pos();
        rubberBand_->setGeometry(QRect(origin_, QSize()));
        rubberBand_->show();
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        rubberBand_->setGeometry(QRect(origin_, event->pos()).normalized());
    }

    void mouseReleaseEvent(QMouseEvent *) override {
        const QRect localRect = rubberBand_->geometry().normalized();
        rubberBand_->hide();
        if (localRect.width() >= 10 && localRect.height() >= 10 && screen_) {
            const QRect globalRect = localRect.translated(geometry().topLeft());
            result_ = screen_->grabWindow(0, globalRect.x(), globalRect.y(), globalRect.width(), globalRect.height()).toImage();
        }
        loop_.quit();
        close();
    }

private:
    QEventLoop loop_;
    QRubberBand *rubberBand_ = nullptr;
    QScreen *screen_ = nullptr;
    QPixmap background_;
    QPoint origin_;
    std::optional<QImage> result_;
};

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
    class BoardCellWidget : public QFrame {
    public:
        explicit BoardCellWidget(QWidget *parent = nullptr)
            : QFrame(parent) {
            setAttribute(Qt::WA_OpaquePaintEvent, true);
            setAttribute(Qt::WA_TransparentForMouseEvents, true);
        }

        void setValue(int value) {
            if (value_ == value) {
                return;
            }
            value_ = value;
            update();
        }

    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, false);
            const QRect r = rect();
            if (value_ == 0) {
                painter.fillRect(r, QColor("#080808"));
                painter.setPen(QPen(QColor("#2a2a2a"), 1));
                painter.drawRect(r.adjusted(0, 0, -1, -1));
                return;
            }

            const QColor fill = cellColor(value_);
            painter.fillRect(r, QColor("#050505"));
            painter.fillRect(r.adjusted(1, 1, -1, -1), fill);
            painter.setPen(QPen(fill.lighter(135), 2));
            painter.drawLine(r.left() + 2, r.top() + 2, r.right() - 2, r.top() + 2);
            painter.drawLine(r.left() + 2, r.top() + 2, r.left() + 2, r.bottom() - 2);
            painter.setPen(QPen(QColor("#050505"), 1));
            painter.drawRect(r.adjusted(0, 0, -1, -1));
        }

    private:
        int value_ = 0;
    };

public:
    explicit BoardWidget(QWidget *parent = nullptr)
        : QWidget(parent) {
        cells_.fill(0);
        setMinimumSize(260, 520);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setObjectName("boardWidget");
        setStyleSheet("QWidget#boardWidget { background-color: #000000; border: 3px solid #050505; border-radius: 7px; }");
        for (int i = 0; i < kColumns * kRows; ++i) {
            auto *cell = new BoardCellWidget(this);
            cellWidgets_[i] = cell;
        }
        refreshAllCells();
    }

    QSize sizeHint() const override {
        return QSize(320, 640);
    }

    const std::array<int, kColumns * kRows> &cells() const {
        return cells_;
    }

    void clearBoard() {
        cells_.fill(0);
        refreshAllCells();
        if (onChanged) {
            onChanged();
        }
    }

    void setCells(const std::array<int, kColumns * kRows> &cells) {
        cells_ = cells;
        refreshAllCells();
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
        refreshAllCells();
        if (onChanged) {
            onChanged();
        }
    }

    std::function<void()> onChanged;
    std::function<bool(int)> onCellPressed;
    std::function<void(int)> onKeyPressed;

protected:
    void resizeEvent(QResizeEvent *) override {
        layoutCells();
    }

    void mousePressEvent(QMouseEvent *event) override {
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
            onKeyPressed(event->key());
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
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

    void layoutCells() {
        const QRect board = boardRect();
        const int cell = qMax(1, board.width() / kColumns);
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                const int index = row * kColumns + col;
                cellWidgets_[index]->setGeometry(board.left() + col * cell,
                                                 board.top() + row * cell,
                                                 cell,
                                                 cell);
            }
        }
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
        refreshCell(index);
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

    void refreshAllCells() {
        for (int i = 0; i < kColumns * kRows; ++i) {
            refreshCell(i);
        }
        layoutCells();
    }

    void refreshCell(int index) {
        cellWidgets_[index]->setValue(cells_[index]);
    }

    std::array<int, kColumns * kRows> cells_{};
    std::array<BoardCellWidget *, kColumns * kRows> cellWidgets_{};
    int paintValue_ = 8;
    int lastPainted_ = -1;
    bool painting_ = false;
    bool eraseStroke_ = false;
};

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr)
        : QMainWindow(parent),
          repoRoot_(findRepoRoot(QCoreApplication::applicationDirPath())) {
        setWindowTitle("Solution Finder Enhanced - Qt");
        resize(1180, 760);
        buildUi();
        ensureFumenState();
        loadOpeners();
        updateBoardFromFumenState();
        updateFumenCodeFromPages();
        updateGeneratedField();
    }

private:
    void buildUi() {
        auto *root = new QWidget(this);
        auto *mainLayout = new QHBoxLayout(root);
        mainLayout->setContentsMargins(14, 14, 14, 14);
        mainLayout->setSpacing(14);

        mainLayout->addWidget(buildSettingsPanel(), 0);
        mainLayout->addWidget(buildBoardPanel(), 2);
        mainLayout->addWidget(buildOutputPanel(), 1);

        setCentralWidget(root);
    }

    QWidget *buildSettingsPanel() {
        auto *panel = new QWidget(this);
        panel->setMinimumWidth(300);
        panel->setMaximumWidth(360);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
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
        patternsEdit_ = new QLineEdit("t,*p5", commandGroup);
        searchForm_->addRow("Command", commandBox_);
        searchForm_->addRow("Hold", holdBox_);
        searchForm_->addRow("Drop", dropBox_);
        searchForm_->addRow("Lines", linesSpin_);
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
        auto *screenshotButton = new QPushButton("Screenshot", openerGroup);
        auto *detectorButton = new QPushButton("Opener Detector", openerGroup);
        openerActions->addWidget(screenshotButton);
        openerActions->addWidget(detectorButton);
        openerLayout->addRow("", openerActions);
        layout->addWidget(openerGroup);

        auto *actions = new QHBoxLayout();
        runButton_ = new QPushButton("Run Search", panel);
        cancelButton_ = new QPushButton("Cancel", panel);
        cancelButton_->setEnabled(false);
        actions->addWidget(runButton_);
        actions->addWidget(cancelButton_);
        layout->addLayout(actions);

        layout->addStretch(1);

        connect(openerGroupBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            populateVariations();
        });
        connect(openerVariationBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            loadSelectedOpener();
        });
        connect(screenshotButton, &QPushButton::clicked, this, [this]() {
            importBoardScreenshot();
        });
        connect(detectorButton, &QPushButton::clicked, this, [this]() {
            detectOpeningFromScreenshot();
        });
        connect(commandBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            updateCommandUi();
        });
        connect(runButton_, &QPushButton::clicked, this, [this]() {
            runSearch();
        });
        connect(cancelButton_, &QPushButton::clicked, this, [this]() {
            cancelSearch();
        });

        selectPaint(8);
        updateCommandUi();
        return panel;
    }

    QWidget *buildBoardPanel() {
        auto *panel = new QWidget(this);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        auto *titleRow = new QHBoxLayout();
        auto *titleBlock = new QVBoxLayout();
        auto *title = new QLabel("Fumen Editor", panel);
        title->setObjectName("paneTitle");
        auto *subtitle = new QLabel("Native Qt board editor with sfinder command output.", panel);
        subtitle->setObjectName("paneSubtitle");
        titleBlock->addWidget(title);
        titleBlock->addWidget(subtitle);
        titleRow->addLayout(titleBlock);
        titleRow->addStretch(1);

        sectionTabs_ = new QTabBar(panel);
        sectionTabs_->setObjectName("sectionTabs");
        sectionTabs_->addTab("Editor");
        sectionTabs_->addTab("Play");
        sectionTabs_->addTab("Output");
        sectionTabs_->addTab("Preview");
        sectionTabs_->setExpanding(false);
        titleRow->addWidget(sectionTabs_);
        layout->addLayout(titleRow);

        centerStack_ = new QStackedWidget(panel);
        auto *editorPage = new QWidget(centerStack_);
        auto *editorLayout = new QVBoxLayout(editorPage);
        editorLayout->setContentsMargins(0, 0, 0, 0);
        editorLayout->setSpacing(8);

        auto *toolbar = new QHBoxLayout();
        auto *clearButton = new QPushButton("Clear Board", panel);
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
            if (commandBox_ && commandBox_->currentText() == "setup" && linesSpin_) {
                linesSpin_->setValue(qMin(12, autoCommandHeight()));
            }
            updateGeneratedField();
            updateFumenCodeFromPages();
        };
        board_->onCellPressed = [this](int visibleIndex) {
            return handleBoardCellPressed(visibleIndex);
        };
        editorContent->addWidget(board_, 1);
        editorContent->addWidget(buildFumenControlsPanel(panel), 0);
        editorLayout->addLayout(editorContent, 1);

        auto *fumenLabel = new QLabel("Fumen Code", panel);
        fumenEdit_ = new QPlainTextEdit(panel);
        fumenEdit_->setPlaceholderText("Paste or select a fumen code. Presets decode directly onto the board.");
        fumenEdit_->setMaximumHeight(76);
        editorLayout->addWidget(fumenLabel);
        editorLayout->addWidget(fumenEdit_);

        generatedField_ = new QPlainTextEdit(panel);
        generatedField_->setReadOnly(true);
        generatedField_->setMaximumHeight(120);
        editorLayout->addWidget(new QLabel("Generated sfinder Field", panel));
        editorLayout->addWidget(generatedField_);

        centerStack_->addWidget(editorPage);
        centerStack_->addWidget(buildPlayPage(centerStack_));
        centerStack_->addWidget(buildCenterOutputPage(centerStack_));
        centerStack_->addWidget(buildPreviewPage(centerStack_));
        layout->addWidget(centerStack_, 1);

        connect(sectionTabs_, &QTabBar::currentChanged, this, [this](int index) {
            centerStack_->setCurrentIndex(index);
            if (index == 1) {
                playBoard_->setFocus();
            } else if (index == 2) {
                refreshGeneratedFiles();
            } else if (index == 3) {
                refreshPreviewCodes();
            }
        });

        connect(clearButton, &QPushButton::clicked, this, [this]() {
            clearCurrentPage();
        });
        connect(mirrorButton, &QPushButton::clicked, this, [this]() {
            mirrorCurrentPage();
        });
        connect(fumenEdit_, &QPlainTextEdit::textChanged, this, [this]() {
            loadFumenCodeFromText();
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
        auto *page = new QWidget(parent);
        auto *layout = new QHBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);

        playBoard_ = new BoardWidget(page);
        playBoard_->onCellPressed = [](int) { return true; };
        playBoard_->onKeyPressed = [this](int key) { handlePlayKey(key); };
        layout->addWidget(playBoard_, 1);

        auto *side = new QWidget(page);
        side->setMinimumWidth(240);
        side->setMaximumWidth(300);
        auto *sideLayout = new QVBoxLayout(side);
        sideLayout->setContentsMargins(0, 0, 0, 0);
        sideLayout->setSpacing(10);

        auto *queueGroup = new QGroupBox("Play", side);
        auto *queueLayout = new QVBoxLayout(queueGroup);
        playStatusLabel_ = new QLabel("Load the editor board or start a new game.", queueGroup);
        playStatusLabel_->setWordWrap(true);
        playQueueLabel_ = new QLabel("Queue: -", queueGroup);
        playQueueLabel_->setWordWrap(true);
        playQueueEdit_ = new QLineEdit(queueGroup);
        playQueueEdit_->setPlaceholderText("Queue, e.g. TILJSZO");
        queueLayout->addWidget(playStatusLabel_);
        queueLayout->addWidget(playQueueLabel_);
        queueLayout->addWidget(playQueueEdit_);
        auto *queueButtons = new QHBoxLayout();
        auto *applyQueueButton = new QPushButton("Apply", queueGroup);
        auto *randomQueueButton = new QPushButton("Random", queueGroup);
        queueButtons->addWidget(applyQueueButton);
        queueButtons->addWidget(randomQueueButton);
        queueLayout->addLayout(queueButtons);
        sideLayout->addWidget(queueGroup);

        auto *moveGroup = new QGroupBox("Moves", side);
        auto *moveGrid = new QGridLayout(moveGroup);
        auto *leftButton = new QPushButton("Left", moveGroup);
        auto *rightButton = new QPushButton("Right", moveGroup);
        auto *downButton = new QPushButton("Soft", moveGroup);
        auto *cwButton = new QPushButton("CW", moveGroup);
        auto *ccwButton = new QPushButton("CCW", moveGroup);
        auto *hardButton = new QPushButton("Hard Drop", moveGroup);
        auto *loadButton = new QPushButton("Load Editor", moveGroup);
        auto *newButton = new QPushButton("New", moveGroup);
        moveGrid->addWidget(leftButton, 0, 0);
        moveGrid->addWidget(rightButton, 0, 1);
        moveGrid->addWidget(downButton, 0, 2);
        moveGrid->addWidget(cwButton, 1, 0);
        moveGrid->addWidget(ccwButton, 1, 1);
        moveGrid->addWidget(hardButton, 1, 2);
        moveGrid->addWidget(loadButton, 2, 0, 1, 2);
        moveGrid->addWidget(newButton, 2, 2);
        sideLayout->addWidget(moveGroup);
        sideLayout->addStretch(1);
        layout->addWidget(side, 0);

        connect(applyQueueButton, &QPushButton::clicked, this, [this]() {
            setPlayQueueFromText(playQueueEdit_->text());
        });
        connect(randomQueueButton, &QPushButton::clicked, this, [this]() {
            randomizePlayQueue();
        });
        connect(leftButton, &QPushButton::clicked, this, [this]() { movePlayPiece(-1, 0); });
        connect(rightButton, &QPushButton::clicked, this, [this]() { movePlayPiece(1, 0); });
        connect(downButton, &QPushButton::clicked, this, [this]() { movePlayPiece(0, 1); });
        connect(cwButton, &QPushButton::clicked, this, [this]() { rotatePlayPiece(1); });
        connect(ccwButton, &QPushButton::clicked, this, [this]() { rotatePlayPiece(-1); });
        connect(hardButton, &QPushButton::clicked, this, [this]() { hardDropPlayPiece(); });
        connect(loadButton, &QPushButton::clicked, this, [this]() { loadEditorBoardIntoPlay(); });
        connect(newButton, &QPushButton::clicked, this, [this]() { newPlayGame(); });

        newPlayGame();
        return page;
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
        panel->setMinimumWidth(320);
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
            linesSpin_->setValue(qMin(12, autoCommandHeight()));
            if (board_ && board_->paintValue() != 1 && board_->paintValue() != 3 && board_->paintValue() != 8) {
                selectPaint(8);
            }
        }

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
        QString output = "v115@";
        const QString table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::array<int, kFumenBlocks> previous{};
        previous.fill(0);

        std::vector<std::array<int, kFumenBlocks>> pages = fumenPages_;
        if (currentFumenPage_ >= 0 && currentFumenPage_ < static_cast<int>(pages.size()) &&
            placeMinoCheck_ && placeMinoCheck_->isChecked() && currentOperation_.type > 0) {
            for (int index : fumenOperationCells(currentOperation_)) {
                if (0 <= index && index < kFumenBlocks) {
                    pages[currentFumenPage_][index] = currentOperation_.type;
                }
            }
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
            if (linesSpin_) {
                linesSpin_->setValue(qMin(12, autoCommandHeight()));
            }
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

    QString pieceName(int type) const {
        return cellName(type);
    }

    std::vector<int> playPieceCells(const PlayPiece &piece) const {
        std::vector<int> cells;
        if (piece.type <= 0 || piece.type >= 8) {
            return cells;
        }
        const auto &offsets = fumenPieceOffsets()[piece.type][piece.rotation % 4];
        for (const QPoint &offset : offsets) {
            const int x = piece.x + offset.x() - 1;
            const int y = piece.y + offset.y() - 1;
            if (0 <= x && x < kColumns && 0 <= y && y < kRows) {
                cells.push_back(y * kColumns + x);
            }
        }
        return cells;
    }

    bool playPieceCollides(const PlayPiece &piece) const {
        if (piece.type <= 0) {
            return false;
        }
        const auto &offsets = fumenPieceOffsets()[piece.type][piece.rotation % 4];
        for (const QPoint &offset : offsets) {
            const int x = piece.x + offset.x() - 1;
            const int y = piece.y + offset.y() - 1;
            if (x < 0 || x >= kColumns || y >= kRows) {
                return true;
            }
            if (y >= 0 && playCells_[y * kColumns + x] != 0) {
                return true;
            }
        }
        return false;
    }

    void updatePlayBoard() {
        if (!playBoard_) {
            return;
        }
        std::array<int, kColumns * kRows> visible = playCells_;
        if (currentPlayPiece_.type > 0) {
            for (int index : playPieceCells(currentPlayPiece_)) {
                visible[index] = currentPlayPiece_.type;
            }
        }
        playBoard_->setCells(visible);
        if (playQueueLabel_) {
            QString queueText;
            for (int piece : playQueue_) {
                queueText += pieceName(piece);
            }
            playQueueLabel_->setText(QString("Current: %1\nQueue: %2")
                                         .arg(currentPlayPiece_.type > 0 ? pieceName(currentPlayPiece_.type) : "-",
                                              queueText.isEmpty() ? "-" : queueText));
        }
    }

    void setPlayQueueFromText(const QString &text) {
        playQueue_.clear();
        for (QChar ch : text) {
            const int type = pieceTypeFromChar(ch);
            if (type > 0) {
                playQueue_.push_back(type);
            }
        }
        if (currentPlayPiece_.type == 0) {
            spawnNextPlayPiece();
        }
        updatePlayBoard();
    }

    void randomizePlayQueue() {
        refillPlayQueue();
        currentPlayPiece_ = PlayPiece();
        spawnNextPlayPiece();
        updatePlayBoard();
    }

    void refillPlayQueue() {
        playQueue_ = {1, 2, 3, 4, 5, 6, 7};
        std::shuffle(playQueue_.begin(), playQueue_.end(), rng_);
    }

    void newPlayGame() {
        playCells_.fill(0);
        randomizePlayQueue();
        if (playStatusLabel_) {
            playStatusLabel_->setText("Keyboard: arrows move, Z/X rotate, Space hard drops.");
        }
        updatePlayBoard();
    }

    void loadEditorBoardIntoPlay() {
        playCells_ = board_ ? board_->cells() : std::array<int, kColumns * kRows>{};
        currentPlayPiece_ = PlayPiece();
        spawnNextPlayPiece();
        if (playStatusLabel_) {
            playStatusLabel_->setText("Loaded editor board into play.");
        }
        updatePlayBoard();
    }

    void spawnNextPlayPiece() {
        if (playQueue_.empty()) {
            refillPlayQueue();
        }
        if (playQueue_.empty()) {
            currentPlayPiece_ = PlayPiece();
            return;
        }
        currentPlayPiece_ = PlayPiece{playQueue_.front(), 0, 4, 1};
        playQueue_.erase(playQueue_.begin());
        if (playPieceCollides(currentPlayPiece_)) {
            currentPlayPiece_ = PlayPiece();
            if (playStatusLabel_) {
                playStatusLabel_->setText("Game over. Click New to restart.");
            }
        }
    }

    void movePlayPiece(int dx, int dy) {
        PlayPiece next = currentPlayPiece_;
        next.x += dx;
        next.y += dy;
        if (!playPieceCollides(next)) {
            currentPlayPiece_ = next;
            updatePlayBoard();
        }
    }

    void rotatePlayPiece(int delta) {
        PlayPiece next = currentPlayPiece_;
        next.rotation = (next.rotation + delta + 4) % 4;
        if (!playPieceCollides(next)) {
            currentPlayPiece_ = next;
            updatePlayBoard();
        }
    }

    void lockPlayPiece() {
        for (int index : playPieceCells(currentPlayPiece_)) {
            playCells_[index] = currentPlayPiece_.type;
        }
        std::array<int, kColumns * kRows> cleared{};
        cleared.fill(0);
        int writeRow = kRows - 1;
        for (int row = kRows - 1; row >= 0; --row) {
            bool full = true;
            for (int col = 0; col < kColumns; ++col) {
                if (playCells_[row * kColumns + col] == 0) {
                    full = false;
                    break;
                }
            }
            if (!full) {
                for (int col = 0; col < kColumns; ++col) {
                    cleared[writeRow * kColumns + col] = playCells_[row * kColumns + col];
                }
                --writeRow;
            }
        }
        playCells_ = cleared;
        spawnNextPlayPiece();
        updatePlayBoard();
    }

    void hardDropPlayPiece() {
        if (currentPlayPiece_.type <= 0) {
            return;
        }
        PlayPiece next = currentPlayPiece_;
        while (true) {
            PlayPiece lower = next;
            lower.y += 1;
            if (playPieceCollides(lower)) {
                break;
            }
            next = lower;
        }
        currentPlayPiece_ = next;
        lockPlayPiece();
    }

    void handlePlayKey(int key) {
        if (key == Qt::Key_Left) {
            movePlayPiece(-1, 0);
        } else if (key == Qt::Key_Right) {
            movePlayPiece(1, 0);
        } else if (key == Qt::Key_Down) {
            movePlayPiece(0, 1);
        } else if (key == Qt::Key_Space) {
            hardDropPlayPiece();
        } else if (key == Qt::Key_Z) {
            rotatePlayPiece(-1);
        } else if (key == Qt::Key_X || key == Qt::Key_Up) {
            rotatePlayPiece(1);
        }
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

    void loadOpeners() {
        openers_.clear();
        QFile file(repoRoot_ + "/shared/openers.json");
        if (!file.open(QIODevice::ReadOnly)) {
            outputEdit_->appendPlainText("Could not open shared/openers.json");
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonArray array = doc.object().value("openers").toArray();
        for (const QJsonValue &value : array) {
            const QJsonObject object = value.toObject();
            Opener opener;
            opener.id = object.value("id").toString();
            opener.openerName = object.value("openerName").toString(object.value("name").toString());
            opener.variationName = object.value("variationName").toString("Base");
            opener.code = object.value("code").toString();
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
        selectEmptyBoardPreset();
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
                        if (linesSpin_) {
                            linesSpin_->setValue(qMin(12, autoCommandHeight()));
                        }
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

    void importBoardScreenshot() {
        const auto image = ScreenCaptureOverlay::capture(this);
        if (!image.has_value()) {
            return;
        }
        const bool preservingColors = !commandBox_ || commandBox_->currentText() != "setup";
        const auto cells = fumenCellsFromBoardImage(*image, preservingColors);
        if (!cells.has_value()) {
            QMessageBox::warning(this, "Screenshot", "Could not read the screenshot as a Tetris board.");
            return;
        }
        replaceCurrentPageWithFumenCells(*cells);
        if (commandBox_ && commandBox_->currentText() == "setup") {
            convertFumenToSetupGray();
        }
        if (outputEdit_) {
            appendRawOutput("\nImported board screenshot.\n");
        }
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
        const auto image = ScreenCaptureOverlay::capture(this);
        if (!image.has_value()) {
            return;
        }
        const auto cells = fumenCellsFromBoardImage(*image, true);
        if (!cells.has_value()) {
            QMessageBox::warning(this, "Opener Detector", "Could not read the screenshot as a Tetris board.");
            return;
        }
        const auto results = detectOpenersForCells(*cells);
        if (results.empty() || results.front().overallScore() < 0.58) {
            QMessageBox::information(this, "Opener Detector", "No opener match found.");
            if (!results.empty()) {
                appendRawOutput(QString("\nClosest opener: %1 (overall %2%, shape %3%, color %4%).\n")
                                    .arg(results.front().displayName())
                                    .arg(results.front().overallScore() * 100.0, 0, 'f', 1)
                                    .arg(results.front().occupancyScore * 100.0, 0, 'f', 1)
                                    .arg(results.front().colorScore * 100.0, 0, 'f', 1));
            }
            return;
        }

        const OpeningDetectionResult &best = results.front();
        const QString message = QString("%1\n\nOverall %2% | Shape %3% | Color %4%\n\nAdd this opener to the editor?")
                                    .arg(best.displayName())
                                    .arg(best.overallScore() * 100.0, 0, 'f', 1)
                                    .arg(best.occupancyScore * 100.0, 0, 'f', 1)
                                    .arg(best.colorScore * 100.0, 0, 'f', 1);
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
        if (process_) {
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
            return;
        }
        const QString fieldPath = writeTextFile(command == "setup" ? "setup-field.txt" : "field.txt", fieldText);
        const QString patterns = patternsEdit_->text().trimmed().isEmpty() ? "*p7" : patternsEdit_->text().trimmed();
        const QString patternsPath = writeTextFile("patterns.txt", patterns + "\n");
        const QString outputBase = outputBaseForCommand(command);

        QString program;
        QStringList args;
        const QString linuxLauncher = repoRoot_ + "/native-linux/bin/sfinder";
        const QString macLauncher = repoRoot_ + "/native-macos/bin/sfinder";
        if (QFileInfo::exists(linuxLauncher)) {
            program = linuxLauncher;
        } else if (QFileInfo::exists(macLauncher) && QSysInfo::productType() == "macos") {
            program = macLauncher;
        } else {
            program = "java";
            args << "-jar" << repoRoot_ + "/solution-finder-1.43/sfinder.jar";
        }
        args << buildSfinderArguments(fieldPath, patternsPath, outputBase);

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
        });

        runButton_->setEnabled(false);
        cancelButton_->setEnabled(true);
        process_->start(program, args);
        if (!process_->waitForStarted(1000)) {
            appendRawOutput("Failed to start sfinder process.\n");
            process_->deleteLater();
            process_ = nullptr;
            runButton_->setEnabled(true);
            cancelButton_->setEnabled(false);
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
    }

    QString repoRoot_;
    std::vector<Opener> openers_;
    BoardWidget *board_ = nullptr;
    QFormLayout *searchForm_ = nullptr;
    QComboBox *commandBox_ = nullptr;
    QComboBox *holdBox_ = nullptr;
    QComboBox *dropBox_ = nullptr;
    QSpinBox *linesSpin_ = nullptr;
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
    BoardWidget *playBoard_ = nullptr;
    QLabel *playStatusLabel_ = nullptr;
    QLabel *playQueueLabel_ = nullptr;
    QLineEdit *playQueueEdit_ = nullptr;
    QTextBrowser *centerOutputBrowser_ = nullptr;
    QComboBox *outputFileBox_ = nullptr;
    BoardWidget *previewBoard_ = nullptr;
    QComboBox *previewCodeBox_ = nullptr;
    QLabel *previewPageLabel_ = nullptr;
    QPushButton *runButton_ = nullptr;
    QPushButton *cancelButton_ = nullptr;
    std::vector<int> paintValues_;
    std::vector<QPushButton *> paintButtons_;
    std::vector<std::array<int, kFumenBlocks>> fumenPages_;
    std::vector<FumenOperation> fumenOperations_;
    std::array<int, kColumns * kRows> playCells_{};
    PlayPiece currentPlayPiece_;
    std::vector<int> playQueue_;
    std::mt19937 rng_{std::random_device{}()};
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
    )");
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Solution Finder Enhanced");
    QApplication::setOrganizationName("rustednuts69");
    applyAppTheme(app);

    MainWindow window;
    window.show();
    return app.exec();
}
