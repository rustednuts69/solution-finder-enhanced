#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
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
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVBoxLayout>

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

QColor cellColor(int value) {
    switch (value) {
    case 1: return QColor("#00d5dd");
    case 2: return QColor("#f6b100");
    case 3: return QColor("#f2e900");
    case 4: return QColor("#ed1c24");
    case 5: return QColor("#c20fd8");
    case 6: return QColor("#1739df");
    case 7: return QColor("#18c92f");
    case 8: return QColor("#9c9ca1");
    default: return QColor("#050505");
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

class BoardWidget : public QWidget {
public:
    explicit BoardWidget(QWidget *parent = nullptr)
        : QWidget(parent) {
        cells_.fill(0);
        setMinimumSize(260, 520);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
    }

    QSize sizeHint() const override {
        return QSize(320, 640);
    }

    const std::array<int, kColumns * kRows> &cells() const {
        return cells_;
    }

    void clearBoard() {
        cells_.fill(0);
        update();
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
        update();
        if (onChanged) {
            onChanged();
        }
    }

    std::function<void()> onChanged;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor("#111111"));

        const QRect board = boardRect();
        painter.setPen(QPen(QColor("#333333"), 1));
        painter.setBrush(QColor("#020202"));
        painter.drawRoundedRect(board.adjusted(0, 0, -1, -1), 8, 8);

        const double cell = static_cast<double>(board.width()) / kColumns;
        for (int row = 0; row < kRows; ++row) {
            for (int col = 0; col < kColumns; ++col) {
                QRectF r(board.left() + col * cell + 1.0,
                         board.top() + row * cell + 1.0,
                         cell - 2.0,
                         cell - 2.0);
                const int value = cells_[row * kColumns + col];
                painter.fillRect(r, cellColor(value));
                painter.setPen(value == 0 ? QColor("#151515") : QColor("#050505"));
                painter.drawRect(r);
            }
        }
    }

    void mousePressEvent(QMouseEvent *event) override {
        painting_ = true;
        paintCellAt(event->position().toPoint());
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (painting_) {
            paintCellAt(event->position().toPoint());
        }
    }

    void mouseReleaseEvent(QMouseEvent *) override {
        painting_ = false;
        lastPainted_ = -1;
    }

private:
    QRect boardRect() const {
        int side = qMin(width(), height() / 2);
        side = qMax(side, 240);
        int boardWidth = qMin(width() - 8, side);
        int boardHeight = boardWidth * 2;
        if (boardHeight > height() - 8) {
            boardHeight = height() - 8;
            boardWidth = boardHeight / 2;
        }
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
        cells_[index] = cells_[index] == paintValue_ ? 0 : paintValue_;
        update();
        if (onChanged) {
            onChanged();
        }
    }

    int mirrorColor(int value) const {
        if (value == 2) return 6;
        if (value == 6) return 2;
        if (value == 4) return 7;
        if (value == 7) return 4;
        return value;
    }

    std::array<int, kColumns * kRows> cells_{};
    int paintValue_ = 8;
    int lastPainted_ = -1;
    bool painting_ = false;
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
        mainLayout->setContentsMargins(12, 12, 12, 12);
        mainLayout->setSpacing(12);

        mainLayout->addWidget(buildSettingsPanel(), 0);
        mainLayout->addWidget(buildBoardPanel(), 1);
        mainLayout->addWidget(buildOutputPanel(), 1);

        setCentralWidget(root);
    }

    QWidget *buildSettingsPanel() {
        auto *panel = new QWidget(this);
        panel->setMinimumWidth(300);
        panel->setMaximumWidth(380);
        auto *layout = new QVBoxLayout(panel);

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
            button->setStyleSheet(QString("QPushButton { background: %1; color: %2; }")
                                      .arg(cellColor(value).name(), value == 0 ? "#dddddd" : "#ffffff"));
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
        auto *restoreButton = new QPushButton("Load Selected Opener", openerGroup);
        openerLayout->addRow("", restoreButton);
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
        connect(restoreButton, &QPushButton::clicked, this, [this]() {
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
        auto *toolbar = new QHBoxLayout();
        auto *clearButton = new QPushButton("Clear Board", panel);
        auto *mirrorButton = new QPushButton("Mirror", panel);
        toolbar->addWidget(clearButton);
        toolbar->addWidget(mirrorButton);
        toolbar->addStretch(1);
        layout->addLayout(toolbar);

        board_ = new BoardWidget(panel);
        board_->onChanged = [this]() {
            updateGeneratedField();
        };
        layout->addWidget(board_, 1);

        auto *fumenLabel = new QLabel("Fumen Code", panel);
        fumenEdit_ = new QPlainTextEdit(panel);
        fumenEdit_->setPlaceholderText("Paste or select a fumen code. Decoding will be ported next.");
        fumenEdit_->setMaximumHeight(82);
        layout->addWidget(fumenLabel);
        layout->addWidget(fumenEdit_);

        generatedField_ = new QPlainTextEdit(panel);
        generatedField_->setReadOnly(true);
        generatedField_->setMaximumHeight(120);
        layout->addWidget(new QLabel("Generated sfinder Field", panel));
        layout->addWidget(generatedField_);

        connect(clearButton, &QPushButton::clicked, board_, &BoardWidget::clearBoard);
        connect(mirrorButton, &QPushButton::clicked, board_, &BoardWidget::mirror);
        return panel;
    }

    QWidget *buildOutputPanel() {
        auto *tabs = new QTabWidget(this);
        outputEdit_ = new QPlainTextEdit(tabs);
        outputEdit_->setReadOnly(true);
        outputEdit_->setStyleSheet("font-family: 'JetBrains Mono', 'SF Mono', monospace; font-size: 13px;");
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
                outputEdit_->appendPlainText("Loaded opener: " + opener.openerName + " - " + opener.variationName);
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

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Solution Finder Enhanced");
    QApplication::setOrganizationName("rustednuts69");

    MainWindow window;
    window.show();
    return app.exec();
}
