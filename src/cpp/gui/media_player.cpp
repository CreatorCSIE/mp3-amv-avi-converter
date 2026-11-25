#include "media_player.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QImage>
#include <QIODevice>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "fluent/icons.h"
#include "fluent/widgets.h"
#include "proc.h"

namespace {
constexpr int CHUNK = 2048;  // 46ms @22050/16bit/mono
}

MediaPlayerWidget::MediaPlayerWidget(QWidget* parent) : QWidget(parent) { build(); }

MediaPlayerWidget::MediaPlayerWidget(const QString& path, QWidget* parent) : QWidget(parent) {
    build();
    setPath(path);
}

MediaPlayerWidget::~MediaPlayerWidget() {
    stopAudio();
    if (aproc_) delete aproc_;
    if (sink_) delete sink_;
}

void MediaPlayerWidget::build() {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    img_ = new QLabel("未选择文件", this);
    img_->setMinimumSize(maxW_, maxH_);
    img_->setAlignment(Qt::AlignCenter);
    lay->addWidget(img_, 1);

    auto* row = new QHBoxLayout();
    playBtn_ = new PushButton(FluentIcon::PLAY, "播放", this);
    connect(playBtn_, &QPushButton::clicked, this, &MediaPlayerWidget::toggle);
    stopBtn_ = new PushButton(FluentIcon::PAUSE, "停止", this);
    connect(stopBtn_, &QPushButton::clicked, this, &MediaPlayerWidget::stop);
    slider_ = new QSlider(Qt::Horizontal, this);
    slider_->setRange(0, 1);
    connect(slider_, &QSlider::sliderPressed, this, &MediaPlayerWidget::beginDrag);
    connect(slider_, &QSlider::sliderMoved, this, &MediaPlayerWidget::previewSeek);
    connect(slider_, &QSlider::sliderReleased, this, &MediaPlayerWidget::endDrag);
    connect(slider_, &QSlider::valueChanged, this, &MediaPlayerWidget::onSliderValue);
    timeLbl_ = new CaptionLabel("0:00 / 0:00", this);
    row->addWidget(playBtn_);
    row->addWidget(stopBtn_);
    row->addWidget(slider_, 1);
    row->addWidget(timeLbl_);
    row->addWidget(new CaptionLabel("音量", this));
    volSlider_ = new QSlider(Qt::Horizontal, this);
    volSlider_->setRange(0, 100);
    volSlider_->setValue(100);
    volSlider_->setFixedWidth(110);
    connect(volSlider_, &QSlider::valueChanged, this, [this](int v) {
        if (sink_) sink_->setVolume(v / 100.0);
    });
    row->addWidget(volSlider_);
    closeBtn_ = new PushButton(FluentIcon::CLOSE, "关闭", this);
    connect(closeBtn_, &QPushButton::clicked, this, &MediaPlayerWidget::closeClicked);
    row->addWidget(closeBtn_);
    lay->addLayout(row);

    vtimer_.setInterval(15);  // 高频追帧
    connect(&vtimer_, &QTimer::timeout, this, &MediaPlayerWidget::tick);
    atimer_.setInterval(20);  // PCM 泵
    connect(&atimer_, &QTimer::timeout, this, &MediaPlayerWidget::pumpAudio);
    seekTimer_.setSingleShot(true);
    seekTimer_.setInterval(150);
    connect(&seekTimer_, &QTimer::timeout, this, [this] { applySeek(clickResume_); });

    setEnabled_(false);
}

// ---------- 对外 ----------

bool MediaPlayerWidget::setPath(const QString& path) {
    closeMedia();
    std::string err;
    if (!pv_.open(path.toUtf8().constData(), err)) {
        img_->setText(QString("无法打开：%1").arg(QString::fromUtf8(err.c_str())));
        return false;
    }
    hasPv_ = true;
    slider_->setRange(0, std::max(1, (int)(pv_.duration * 1000)));
    syncSlider(0);
    timeLbl_->setText("0:00 / " + fmt(pv_.duration));
    makeSink();
    setEnabled_(true);
    showFrame(pv_.read());
    pv_.seek(0.0);
    return true;
}

void MediaPlayerWidget::closeMedia() {
    vtimer_.stop();
    playBtn_->setText("播放");
    stopAudio();
    if (sink_) { delete sink_; sink_ = nullptr; aio_ = nullptr; }  // 先于解码进程析构释放 WASAPI
    if (hasPv_) { pv_.close(); hasPv_ = false; }
    img_->setText("未选择文件");
    setEnabled_(false);
}

void MediaPlayerWidget::setEnabled_(bool on) {
    for (QWidget* w : {static_cast<QWidget*>(playBtn_), static_cast<QWidget*>(stopBtn_),
                       static_cast<QWidget*>(slider_), static_cast<QWidget*>(volSlider_),
                       static_cast<QWidget*>(closeBtn_)})
        w->setEnabled(on);
}

// ---------- 音频 ----------

void MediaPlayerWidget::makeSink() {
    sink_ = nullptr;
    aio_ = nullptr;
    if (!hasPv_ || !pv_.audio_rate) return;  // 无音频流/无声卡退回实时钟纯画面
    QAudioFormat fmt;
    fmt.setSampleRate(pv_.audio_rate);
    fmt.setChannelCount(std::max(1, pv_.audio_ch));
    fmt.setSampleFormat(QAudioFormat::Int16);
    auto* s = new QAudioSink(fmt, this);
    if (s->error() != QAudio::NoError) { delete s; return; }
    sink_ = s;
    byterate_ = pv_.audio_rate * std::max(1, pv_.audio_ch) * 2;
}

void MediaPlayerWidget::startAudio(double seconds) {
    stopAudio();
    astart_ = seconds;
    if (sink_ == nullptr || !hasPv_) {
        t0_ = seconds;
        elapsed_.start();
        return;
    }
    astop_ = false;
    aeof_ = false;
    arest_.clear();
    awritten_ = 0;
    {
        std::lock_guard lk(amx_);
        aq_.clear();
    }
    if (!aproc_) aproc_ = new Process();
    char ss[32];
    snprintf(ss, sizeof ss, "%.3f", seconds);
    aprocAlive_ = aproc_->spawn({"ffmpeg", "-hide_banner", "-v", "error", "-ss", ss,
                                 "-i", pv_.path, "-vn", "-f", "s16le", "-acodec",
                                 "pcm_s16le", "pipe:1"}, true, false);
    if (!aprocAlive_) {  // 起流失败：退实时钟纯画面
        t0_ = seconds;
        elapsed_.start();
        return;
    }
    areader_ = std::thread([this] {
        std::vector<uint8_t> buf(CHUNK);
        while (!astop_) {
            size_t n = aproc_->read_out(buf.data(), CHUNK);
            if (n == 0) break;
            std::unique_lock lk(amx_);
            while (!astop_ && aq_.size() >= 2) acv_.wait(lk);  // 有界队列背压
            if (astop_) break;
            aq_.emplace_back(buf.begin(), buf.begin() + n);
        }
        {
            std::lock_guard lk(amx_);
            aeof_ = true;
        }
        acv_.notify_all();
    });
    sink_->setVolume(volSlider_->value() / 100.0);
    aio_ = sink_->start();
    atimer_.start(20);
}

void MediaPlayerWidget::pumpAudio() {
    if (aio_ == nullptr || sink_ == nullptr) return;
    int free_ = sink_->bytesFree();
    while (free_ >= 512) {
        if (arest_.empty()) {
            std::lock_guard lk(amx_);
            if (aq_.empty()) {
                if (aeof_) atimer_.stop();  // 流尾：停泵（视频追帧处统一重启）
                return;
            }
            arest_ = std::move(aq_.front());
            aq_.pop_front();
        }
        size_t n = std::min<size_t>(free_, arest_.size() - arest_.size() % 2);
        if (n == 0) break;
        aio_->write((const char*)arest_.data(), (qint64)n);
        awritten_ += (qint64)n;
        arest_.erase(arest_.begin(), arest_.begin() + n);
        free_ -= (int)n;
    }
}

void MediaPlayerWidget::stopAudio() {
    atimer_.stop();
    if (sink_) sink_->stop();
    aio_ = nullptr;
    astop_ = true;
    acv_.notify_all();
    if (areader_.joinable()) areader_.join();
    if (aprocAlive_ && aproc_) {
        if (aproc_->running()) aproc_->terminate();
        aproc_->wait(1000);
        aproc_->close();
        aprocAlive_ = false;
    }
}

double MediaPlayerWidget::clock() {
    // 主时钟：音频已消费字节折算（扣除声卡缓冲占用）；无音频走实时钟
    if (!aprocAlive_) {
        if (sink_) return astart_;  // 暂停：冻结在本段起点
        if (!elapsed_.isValid()) return t0_;
        return t0_ + elapsed_.elapsed() / 1000.0;
    }
    int buffered = sink_->bufferSize() - sink_->bytesFree();
    qint64 consumed = awritten_ - std::max(0, buffered);
    return astart_ + (double)std::max<qint64>(0, consumed) / byterate_;
}

// ---------- 视频 ----------

void MediaPlayerWidget::hideEvent(QHideEvent* e) {
    vtimer_.stop();
    stopAudio();
    dragging_ = false;
    playBtn_->setText("播放");
    QWidget::hideEvent(e);
}

QString MediaPlayerWidget::fmt(double s) {
    int s2 = std::max(0, (int)s);
    return QString("%1:%2").arg(s2 / 60).arg(s2 % 60, 2, 10, QLatin1Char('0'));
}

void MediaPlayerWidget::toggle() {
    if (vtimer_.isActive()) {
        vtimer_.stop();
        stopAudio();
        playBtn_->setText("播放");
    } else {
        playBtn_->setText("暂停");
        startAudio(pv_.position());
        vtimer_.start(15);
    }
}

void MediaPlayerWidget::tick() {
    double clk = clock();
    for (int i = 0; i < 4; i++) {  // 落后追帧，单 tick 最多补 4 帧
        if (pv_.position() > clk + 0.02) break;
        auto frame = pv_.read();
        if (frame.empty()) {  // 流尾：整点重来
            pv_.seek(0.0);
            startAudio(0.0);
            return;
        }
        showFrame(frame);
    }
    if (!seekTimer_.isActive()) syncSlider((int)(pv_.position() * 1000));
    timeLbl_->setText(fmt(pv_.position()) + " / " + fmt(pv_.duration));
}

// ---------- 拖动/跳转 ----------

void MediaPlayerWidget::beginDrag() {
    dragging_ = true;
    wasPlaying_ = vtimer_.isActive();
    vtimer_.stop();
    stopAudio();
    playBtn_->setText("播放");
}

void MediaPlayerWidget::previewSeek(int ms) {
    if (hasPv_) timeLbl_->setText(fmt(ms / 1000.0) + " / " + fmt(pv_.duration));
}

void MediaPlayerWidget::endDrag() {
    dragging_ = false;
    applySeek(wasPlaying_);
}

void MediaPlayerWidget::syncSlider(int ms) {
    syncing_ = true;
    slider_->setValue(ms);
    syncing_ = false;
}

void MediaPlayerWidget::onSliderValue(int ms) {
    Q_UNUSED(ms);
    if (syncing_ || dragging_ || !hasPv_) return;
    clickResume_ = vtimer_.isActive();
    seekTimer_.start(150);  // 点槽/键盘跳转：防抖后 seek
}

void MediaPlayerWidget::applySeek(bool resume) {
    if (!hasPv_) return;
    seekTimer_.stop();
    vtimer_.stop();
    stopAudio();
    pv_.seek(slider_->value() / 1000.0);
    showFrame(pv_.read());
    syncSlider((int)(pv_.position() * 1000));
    timeLbl_->setText(fmt(pv_.position()) + " / " + fmt(pv_.duration));
    if (resume) {
        playBtn_->setText("暂停");
        startAudio(pv_.position());
        vtimer_.start(15);
    }
}

void MediaPlayerWidget::stop() {
    if (!hasPv_) return;
    syncSlider(0);
    applySeek(false);
}

void MediaPlayerWidget::closeClicked() {
    closeMedia();
    emit mediaClosed();
}

void MediaPlayerWidget::showFrame(const std::vector<uint8_t>& f) {
    if (f.empty()) return;
    QImage qimg(f.data(), pv_.width, pv_.height, 3 * pv_.width,
                QImage::Format_RGB888);
    QPixmap pix = QPixmap::fromImage(qimg.copy());
    if (pix.width() > maxW_ || pix.height() > maxH_)
        pix = pix.scaled(maxW_, maxH_, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    img_->setPixmap(pix);
}
