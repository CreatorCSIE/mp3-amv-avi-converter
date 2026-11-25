#include "convert_interface.h"

#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>

#include "common.h"
#include "ffproc.h"
#include "fluent/icons.h"
#include "fluent/widgets.h"
#include "preview_dialog.h"
#include "profiles.h"
#include "probe.h"

namespace {

struct ProgCtx {
    QueueWorker* w;
    int row;
};

}  // namespace

// ---- QueueWorker ----

void QueueWorker::progCb(int done, int total, void* user) {
    auto* c = static_cast<ProgCtx*>(user);
    if (c && c->w) emit c->w->progress(c->row, done, total);
}

void QueueWorker::enqueue(const Job& j) {
    std::lock_guard<std::mutex> lk(mx_);
    jobs_.push_back(j);
}

void QueueWorker::run() {
    while (true) {
        Job job;
        {
            std::lock_guard<std::mutex> lk(mx_);
            if (jobs_.empty()) return;
            job = jobs_.front();
            jobs_.pop_front();
        }
        ProgCtx ctx{this, job.row};
        Profile p{job.w, job.h, job.fps};
        ConvertOptions opt;
        opt.mode = job.mode;
        opt.flip = 0;
        opt.q = 4;
        opt.progress = &QueueWorker::progCb;
        opt.user = &ctx;
        opt.cancel = job.cancel.get();

        std::string err;
        int rc = -1;
        if (job.mode == "avi") {
            ProbeInfo info;
            std::string perr;
            if (probe_media(job.src, info, perr)) opt.duration = info.duration;
            rc = convert(job.src, job.dst, p, opt, err);
        } else {
            rc = convert(job.src, job.dst, p, opt, err);
        }
        bool ok = (rc == 0);
        QString msg = ok ? QString::fromUtf8(job.dst.c_str())
                         : QString::fromUtf8(err.c_str());
        emit finishedRow(job.row, ok, msg);
    }
}

// ---- ConvertInterface ----

ConvertInterface::ConvertInterface(QWidget* parent) : QWidget(parent) {
    setObjectName("convertInterface");
    build();
    setAcceptDrops(true);
}

void ConvertInterface::build() {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(36, 20, 36, 20);
    lay->setSpacing(12);

    // 行 1：模式 / 分辨率 / 帧率
    auto* row = new QHBoxLayout();
    modeCombo_ = new QComboBox(this);
    modeCombo_->addItems({"AMV（设备用）", "AVI（播放器友好）"});
    sizeCombo_ = new QComboBox(this);
    for (const auto& e : profiles::ui_profiles())
        sizeCombo_->addItem(QString("%1x%2").arg(e.w).arg(e.h));
    sizeCombo_->setCurrentText("160x120");
    fpsCombo_ = new QComboBox(this);
    refreshFps();
    connect(sizeCombo_, &QComboBox::currentTextChanged, this, [this](const QString&) { refreshFps(); });
    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConvertInterface::onMode);
    row->addWidget(new CaptionLabel("模式", this));
    row->addWidget(modeCombo_);
    row->addWidget(new CaptionLabel("分辨率", this));
    row->addWidget(sizeCombo_);
    row->addWidget(new CaptionLabel("帧率", this));
    row->addWidget(fpsCombo_);
    row->addStretch(1);
    lay->addLayout(row);

    // 行 2：输出目录 + 添加文件
    auto* row2 = new QHBoxLayout();
    outEdit_ = new QLineEdit(this);
    outEdit_->setPlaceholderText("输出目录（留空 = 源文件同目录）");
    auto* pick = new PushButton(FluentIcon::FOLDER_ADD, "…", this);
    connect(pick, &QPushButton::clicked, this, &ConvertInterface::pickOut);
    auto* addBtn = new PrimaryPushButton(FluentIcon::ADD, "添加文件", this);
    connect(addBtn, &QPushButton::clicked, this, &ConvertInterface::addFiles);
    row2->addWidget(outEdit_, 1);
    row2->addWidget(pick);
    row2->addWidget(addBtn);
    lay->addLayout(row2);

    // 队列表
    table_ = new QTableWidget(this);
    table_->setObjectName("fluentTable");
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({"源文件", "输出", "状态", "进度"});
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->setColumnWidth(1, 220);
    table_->setColumnWidth(2, 120);
    table_->setColumnWidth(3, 200);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(table_, &QTableWidget::cellClicked, this, &ConvertInterface::onRowClicked);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &ConvertInterface::onPreview);
    lay->addWidget(table_, 1);

    // 行 3：开始 / 清除 / 提示
    auto* row3 = new QHBoxLayout();
    startBtn_ = new PrimaryPushButton(FluentIcon::PLAY, "开始全部", this);
    connect(startBtn_, &QPushButton::clicked, this, &ConvertInterface::startAll);
    clearBtn_ = new PushButton(FluentIcon::DELETE_, "清除已完成", this);
    connect(clearBtn_, &QPushButton::clicked, this, &ConvertInterface::clearDone);
    hint_ = new CaptionLabel("", this);
    row3->addWidget(startBtn_);
    row3->addWidget(clearBtn_);
    row3->addWidget(hint_, 1);
    lay->addLayout(row3);

    worker_ = new QueueWorker(this);
    connect(worker_, &QueueWorker::progress, this, &ConvertInterface::onProgress);
    connect(worker_, &QueueWorker::finishedRow, this, &ConvertInterface::onDone);
}

void ConvertInterface::refreshFps() {
    QStringList wh = sizeCombo_->currentText().split("x");
    int w = wh[0].toInt(), h = wh[1].toInt();
    fpsCombo_->clear();
    std::vector<int> fpsVals = {25};
    for (const auto& e : profiles::ui_profiles())
        if (e.w == w && e.h == h) { fpsVals.assign(std::begin(e.fps), std::end(e.fps)); break; }
    for (int f : fpsVals) {
        QString mark = profiles::ffmpeg_safe(f) ? "" : "（自动改15）";
        fpsCombo_->addItem(QString("%1%2").arg(f).arg(mark));
    }
    int idx = fpsCombo_->findText("25");
    fpsCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
}

void ConvertInterface::onMode(int idx) {
    fpsCombo_->setEnabled(idx == 0);
    hint_->setText(idx == 0 ? "" : "AVI 模式帧率恒 ~21.68（音频驱动），质量 q=4");
}

void ConvertInterface::pickOut() {
    QString d = QFileDialog::getExistingDirectory(this, "选择输出目录");
    if (!d.isEmpty()) outEdit_->setText(d);
}

void ConvertInterface::addFiles() {
    QStringList files = QFileDialog::getOpenFileNames(
        this, "选择视频/音频", "",
        "媒体文件 (*.mp4 *.avi *.mkv *.mp3 *.mov *.wmv *.flv *.webm *.ts *.wav)");
    append(files);
}

void ConvertInterface::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void ConvertInterface::dropEvent(QDropEvent* e) {
    QStringList files;
    for (const QUrl& u : e->mimeData()->urls()) {
        QString p = u.toLocalFile();
        if (isMediaFile(p)) files << p;
    }
    append(files);
}

void ConvertInterface::append(const QStringList& files) {
    QStringList kept;
    for (const QString& f : files) if (isMediaFile(f)) kept << f;
    if (kept.isEmpty()) return;
    for (const QString& f : kept) {
        QFileInfo p(f);
        int r = table_->rowCount();
        table_->insertRow(r);
        auto* srcIt = new QTableWidgetItem(p.fileName());
        srcIt->setData(Qt::UserRole, f);
        table_->setItem(r, 0, srcIt);
        table_->setItem(r, 2, new QTableWidgetItem("等待中"));
        QString dst = target(f);
        auto* it = new QTableWidgetItem(QFileInfo(dst).fileName());
        it->setData(Qt::UserRole, dst);
        table_->setItem(r, 1, it);
        auto* pb = new QProgressBar(this);
        pb->setRange(0, 100);
        pb->setValue(0);
        table_->setCellWidget(r, 3, pb);
    }
}

QString ConvertInterface::target(const QString& src) const {
    QString outDir = outEdit_->text().trimmed();
    QString ext = modeCombo_->currentIndex() == 0 ? ".amv" : ".avi";
    QFileInfo p(src);
    QString dir = outDir.isEmpty() ? p.absolutePath() : outDir;
    return dir + "/" + p.completeBaseName() + ext;
}

void ConvertInterface::startAll() {
    bool avi = modeCombo_->currentIndex() == 1;
    QStringList wh = sizeCombo_->currentText().split("x");
    int w = wh[0].toInt(), h = wh[1].toInt();
    int fps = fpsCombo_->currentText().split("（")[0].toInt();
    int started = 0;
    for (int r = 0; r < table_->rowCount(); r++) {
        if (table_->item(r, 2)->text() != "等待中") continue;
        QString src = table_->item(r, 0)->data(Qt::UserRole).toString();
        if (src.isEmpty() || !QFileInfo(src).isFile()) continue;
        QString dst = target(src);
        Job job;
        job.row = r;
        job.src = src.toUtf8().constData();
        job.dst = dst.toUtf8().constData();
        job.w = w;
        job.h = h;
        if (avi) {
            job.mode = "avi";
            job.fps = 0;
        } else {
            job.mode = "amv";
            job.fps = profiles::ffmpeg_safe(fps) ? fps : profiles::pick_closest_fps(fps);
        }
        auto cancel = std::make_shared<std::atomic<bool>>(false);
        cancels_[r] = cancel;
        job.cancel = cancel;
        worker_->enqueue(job);
        table_->item(r, 2)->setText("转换中（点击取消）");
        started++;
    }
    if (started && !worker_->isRunning()) worker_->start();
}

void ConvertInterface::onRowClicked(int r, int) {
    auto* item = table_->item(r, 2);
    if (item && item->text().startsWith("转换中")) {
        auto it = cancels_.find(r);
        if (it != cancels_.end()) it->second->store(true);
        item->setText("取消中");
    }
}

void ConvertInterface::onPreview(int r, int) {
    if (table_->item(r, 2)->text() != "完成") return;
    QString dst = table_->item(r, 1)->data(Qt::UserRole).toString();
    if (!QFileInfo(dst).isFile()) return;
    PreviewDialog(dst, this).exec();
}

void ConvertInterface::onProgress(int r, int done, int total) {
    auto* pb = qobject_cast<QProgressBar*>(table_->cellWidget(r, 3));
    if (pb) pb->setValue(total > 0 ? (int)(done * 100.0 / total) : 0);
}

void ConvertInterface::onDone(int r, bool ok, const QString& msg) {
    auto* it = table_->item(r, 2);
    if (it) it->setText(ok ? "完成" : "失败");
    auto* pb = qobject_cast<QProgressBar*>(table_->cellWidget(r, 3));
    if (ok && pb) pb->setValue(100);
    if (!ok) InfoBar::error("转换失败", msg.right(300), this);
    cancels_.erase(r);
}

void ConvertInterface::clearDone() {
    for (int r = table_->rowCount() - 1; r >= 0; r--) {
        QString st = table_->item(r, 2)->text();
        if (st == "完成" || st == "失败") {
            cancels_.erase(r);
            table_->removeRow(r);
        }
    }
}
