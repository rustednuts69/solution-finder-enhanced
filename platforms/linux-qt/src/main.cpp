#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QSysInfo>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVBoxLayout>
#include <QStyleFactory>

#include <array>
#include <functional>
#include <optional>
#include <vector>

namespace {

constexpr int kColumns = 10;
constexpr int kRows = 20;

struct Opener {
    QString id;
    QString openerName;
    QString variationName;
    QString code;
};

struct DecodedFumen {
    std::array<int, kColumns * kRows> cells{};
    int pages = 0;
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

QString findRepoRoot(QString start) {
    QDir dir(start);
    while (true) {
        if (QFileInfo::exists(dir.filePath("shared/openers.json")) &&
            QFileInfo::exists(dir.filePath("solution-finder-1.43/sfinder.jar"))) {
            return dir.absolutePath();
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

std::optional<DecodedFumen> decodeFumenV115(QString code) {
    code = code.trimmed();
    const int prefix = code.indexOf("v115@");
    if (prefix >= 0) {
        code = code.mid(prefix + 5);
    }
    const int query = code.indexOf('?');
    if (query >= 0) {
        code = code.left(query);
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

    constexpr int kFumenRows = 24;
    constexpr int kFumenBlocks = kColumns * kFumenRows;
    const int pieceOffsets[] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,1,1,1,2,1,3,1,1,0,1,1,1,2,1,3,0,1,1,1,2,1,3,1,1,0,1,1,1,2,1,3,
        0,1,1,1,2,1,0,2,1,0,1,1,1,2,2,2,2,0,0,1,1,1,2,1,0,0,1,0,1,1,1,2,
        1,1,2,1,1,2,2,2,1,1,2,1,1,2,2,2,1,1,2,1,1,2,2,2,1,1,2,1,1,2,2,2,
        0,1,1,1,1,2,2,2,2,0,1,1,2,1,1,2,0,1,1,1,1,2,2,2,2,0,1,1,2,1,1,2,
        0,1,1,1,2,1,1,2,1,0,1,1,2,1,1,2,1,0,0,1,1,1,2,1,1,0,0,1,1,1,1,2,
        0,1,1,1,2,1,2,2,1,0,2,0,1,1,1,2,0,0,0,1,1,1,2,1,1,0,1,1,0,2,1,2,
        1,1,2,1,0,2,1,2,0,0,0,1,1,1,1,2,1,1,2,1,0,2,1,2,0,0,0,1,1,1,1,2
    };

    std::array<int, kFumenBlocks> field{};
    field.fill(0);
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

        if (!noLock) {
            if (piece > 0) {
                for (int block = 0; block < 4; ++block) {
                    const int base = piece * 32 + rotation * 8 + block * 2;
                    const int x = pieceOffsets[base];
                    const int y = pieceOffsets[base + 1];
                    const int index = position + y * kColumns + x - 11;
                    if (0 <= index && index < kFumenBlocks) {
                        field[index] = piece;
                    }
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

    DecodedFumen decoded;
    decoded.cells.fill(0);
    decoded.pages = pages;
    const int topRow = 3;
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kColumns; ++col) {
            int value = field[(topRow + row) * kColumns + col];
            decoded.cells[row * kColumns + col] = qBound(0, value, 8);
        }
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
        if (onChanged) {
            onChanged();
        }
    }

    void setPaintValue(int value) {
        paintValue_ = value;
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

    int mirrorColor(int value) const {
        if (value == 2) return 6;
        if (value == 6) return 2;
        if (value == 4) return 7;
        if (value == 7) return 4;
        return value;
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
        loadOpeners();
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
        auto *form = new QFormLayout(commandGroup);
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
        verboseCheck_ = new QCheckBox("Verbose output", commandGroup);
        form->addRow("Command", commandBox_);
        form->addRow("Hold", holdBox_);
        form->addRow("Drop", dropBox_);
        form->addRow("Lines", linesSpin_);
        form->addRow("Patterns", patternsEdit_);
        form->addRow("", verboseCheck_);
        layout->addWidget(commandGroup);

        auto *paintGroup = new QGroupBox("Paint", panel);
        auto *paintLayout = new QGridLayout(paintGroup);
        const std::vector<int> palette = {0, 8, 1, 2, 3, 4, 5, 6, 7};
        int column = 0;
        for (int value : palette) {
            auto *button = new QPushButton(cellName(value), paintGroup);
            button->setCheckable(true);
            button->setMinimumHeight(34);
            button->setStyleSheet(QString(
                "QPushButton { background: %1; color: %2; border: 1px solid #242424; border-radius: 5px; font-weight: 650; }"
                "QPushButton:checked { border: 3px solid #0a84ff; }")
                                      .arg(cellColor(value).name(), value == 0 ? "#d8d8d8" : "#ffffff"));
            paintButtons_.push_back(button);
            paintLayout->addWidget(button, column / 3, column % 3);
            connect(button, &QPushButton::clicked, this, [this, value]() {
                selectPaint(value);
            });
            ++column;
        }
        layout->addWidget(paintGroup);

        auto *openerGroup = new QGroupBox("Openers", panel);
        auto *openerLayout = new QFormLayout(openerGroup);
        openerGroupBox_ = new QComboBox(openerGroup);
        openerVariationBox_ = new QComboBox(openerGroup);
        openerLayout->addRow("Base", openerGroupBox_);
        openerLayout->addRow("Variation", openerVariationBox_);
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
        connect(runButton_, &QPushButton::clicked, this, [this]() {
            runSearch();
        });
        connect(cancelButton_, &QPushButton::clicked, this, [this]() {
            cancelSearch();
        });

        selectPaint(8);
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

        auto *sectionTabs = new QTabBar(panel);
        sectionTabs->setObjectName("sectionTabs");
        sectionTabs->addTab("Editor");
        sectionTabs->addTab("Play");
        sectionTabs->addTab("Output");
        sectionTabs->addTab("Preview");
        sectionTabs->setExpanding(false);
        titleRow->addWidget(sectionTabs);
        layout->addLayout(titleRow);

        auto *stack = new QStackedWidget(panel);
        auto *editorPage = new QWidget(stack);
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

        board_ = new BoardWidget(panel);
        board_->onChanged = [this]() {
            updateGeneratedField();
        };
        editorLayout->addWidget(board_, 1);

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

        stack->addWidget(editorPage);
        stack->addWidget(makePlaceholderPage("Play mode", "Playable mode will be ported after the editor and solver panes are stable.", stack));
        stack->addWidget(makePlaceholderPage("Output preview", "Generated HTML output will be shown here once the Qt preview pane is ported.", stack));
        stack->addWidget(makePlaceholderPage("Fumen preview", "Clickable solution fumen previews will be restored in this pane.", stack));
        layout->addWidget(stack, 1);

        connect(sectionTabs, &QTabBar::currentChanged, stack, &QStackedWidget::setCurrentIndex);

        connect(clearButton, &QPushButton::clicked, board_, &BoardWidget::clearBoard);
        connect(mirrorButton, &QPushButton::clicked, board_, &BoardWidget::mirror);
        return panel;
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
        auto *tabs = new QTabWidget(this);
        tabs->setMinimumWidth(320);
        outputEdit_ = new QPlainTextEdit(tabs);
        outputEdit_->setReadOnly(true);
        outputEdit_->setStyleSheet("font-family: 'Menlo', 'SF Mono', 'DejaVu Sans Mono', monospace; font-size: 13px;");
        tabs->addTab(outputEdit_, "Command Output");

        auto *tips = new QPlainTextEdit(tabs);
        tips->setReadOnly(true);
        tips->setPlainText(
            "Qt port status\n"
            "\n"
            "- This shell is native Qt Widgets, not a browser UI.\n"
            "- Board edits produce an sfinder field file.\n"
            "- Opener selection currently loads fumen codes into the text box.\n"
            "- Fumen decoding, output previews, screenshots, and play mode are the next layers to port.\n"
            "\n"
            "Pattern examples\n"
            "\n"
            "t,*p5       T first, then any 5 pieces\n"
            "*p7,*p7     two 7-bag chunks\n"
            "I,O,T       explicit queue\n");
        tabs->addTab(tips, "Notes");
        return tabs;
    }

    void selectPaint(int value) {
        if (board_) {
            board_->setPaintValue(value);
        }
        for (auto *button : paintButtons_) {
            button->setChecked(false);
        }
        const std::vector<int> palette = {0, 8, 1, 2, 3, 4, 5, 6, 7};
        for (int i = 0; i < static_cast<int>(palette.size()) && i < static_cast<int>(paintButtons_.size()); ++i) {
            if (palette[i] == value) {
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
        openerGroupBox_->clear();
        openerGroupBox_->addItems(groups);
        populateVariations();
    }

    void populateVariations() {
        const QString group = openerGroupBox_->currentText();
        openerVariationBox_->clear();
        for (const Opener &opener : openers_) {
            if (opener.openerName == group) {
                openerVariationBox_->addItem(opener.variationName, opener.id);
            }
        }
    }

    void loadSelectedOpener() {
        const QString id = openerVariationBox_->currentData().toString();
        for (const Opener &opener : openers_) {
            if (opener.id == id) {
                fumenEdit_->setPlainText(opener.code);
                if (opener.code.isEmpty()) {
                    board_->clearBoard();
                    outputEdit_->appendPlainText("Loaded opener: " + opener.openerName + " - " + opener.variationName);
                    return;
                }
                const auto decoded = decodeFumenV115(opener.code);
                if (decoded.has_value()) {
                    board_->setCells(decoded->cells);
                    outputEdit_->appendPlainText(QString("Loaded opener: %1 - %2 (%3 page%4)")
                                                     .arg(opener.openerName, opener.variationName)
                                                     .arg(decoded->pages)
                                                     .arg(decoded->pages == 1 ? "" : "s"));
                } else {
                    outputEdit_->appendPlainText("Could not decode opener fumen: " + opener.openerName + " - " + opener.variationName);
                }
                return;
            }
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

    QStringList buildSfinderArguments(const QString &fieldPath, const QString &patternsPath, const QString &outputBase) const {
        const QString command = commandBox_->currentText();
        QString sfCommand = command;
        QStringList args;
        if (command == "tetris-path") {
            sfCommand = "path";
        }
        args << sfCommand;

        const QString fumenCode = fumenEdit_->toPlainText().trimmed();
        if ((command == "cover" || command == "setup") && !fumenCode.isEmpty()) {
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
            args << "--success-condition" << "tetris-end";
        }
        if (command == "path" || command == "tetris-path") {
            args << "-f" << "html";
            args << "-o" << outputBase;
        }
        if (command == "setup") {
            args << "-l" << QString::number(linesSpin_->value());
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
            args << "-ft" << QString::number(linesSpin_->value());
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

        const QString fieldPath = writeTextFile("field.txt", generatedFieldText());
        const QString patterns = patternsEdit_->text().trimmed().isEmpty() ? "*p7" : patternsEdit_->text().trimmed();
        const QString patternsPath = writeTextFile("patterns.txt", patterns + "\n");
        const QString outputBase = QDir(appDataDir()).filePath("run/qt_output");

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

        outputEdit_->clear();
        outputEdit_->appendPlainText("$ " + program + " " + args.join(" "));

        process_ = new QProcess(this);
        process_->setWorkingDirectory(repoRoot_);
        process_->setProcessChannelMode(QProcess::MergedChannels);

        connect(process_, &QProcess::readyReadStandardOutput, this, [this]() {
            outputEdit_->appendPlainText(QString::fromLocal8Bit(process_->readAllStandardOutput()));
        });
        connect(process_, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
            outputEdit_->appendPlainText(QString("\nProcess finished: exit %1 (%2)")
                                             .arg(exitCode)
                                             .arg(status == QProcess::NormalExit ? "normal" : "crashed"));
            process_->deleteLater();
            process_ = nullptr;
            runButton_->setEnabled(true);
            cancelButton_->setEnabled(false);
        });

        runButton_->setEnabled(false);
        cancelButton_->setEnabled(true);
        process_->start(program, args);
        if (!process_->waitForStarted(1000)) {
            outputEdit_->appendPlainText("Failed to start sfinder process.");
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
    QComboBox *commandBox_ = nullptr;
    QComboBox *holdBox_ = nullptr;
    QComboBox *dropBox_ = nullptr;
    QSpinBox *linesSpin_ = nullptr;
    QLineEdit *patternsEdit_ = nullptr;
    QCheckBox *verboseCheck_ = nullptr;
    QComboBox *openerGroupBox_ = nullptr;
    QComboBox *openerVariationBox_ = nullptr;
    QPlainTextEdit *fumenEdit_ = nullptr;
    QPlainTextEdit *generatedField_ = nullptr;
    QPlainTextEdit *outputEdit_ = nullptr;
    QPushButton *runButton_ = nullptr;
    QPushButton *cancelButton_ = nullptr;
    std::vector<QPushButton *> paintButtons_;
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
