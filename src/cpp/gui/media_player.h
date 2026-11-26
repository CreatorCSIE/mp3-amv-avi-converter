#pragma once
#include <QElapsedTimer>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "preview.h"

class QLabel;
namespace qfw {
class FluentPushButton;
class Slider;
class CaptionLabel;
}  // namespace qfw
class QAudioSink;
class QIODevice;
class QHideEvent;
class Process;

// 共享播放组件（media_player.py 1:1 移植）：音频主时钟（QAudioSink 已消费字节折算）
// + 视频 15ms 追帧 + ffmpeg PCM 管道读线程 + 拖动定位 + 音量。
class MediaPlayerWidget : public QWidget {
    Q_OBJECT
public:
    explicit MediaPlayerWidget(QWidget* parent = nullptr);
    explicit MediaPlayerWidget(const QString& path, QWidget* parent = nullptr);
    ~MediaPlayerWidget() override;

    // 装载文件；失败时在画面区提示原因
    bool setPath(const QString& path);
    void closeMedia();

signals:
    void mediaClosed();  // 用户主动关闭（页面据此清路径显示）

protected:
    void hideEvent(QHideEvent* e) override;

private:
    void build();
    void setEnabled_(bool on);
    void makeSink();
    void startAudio(double seconds);
    void stopAudio();
    double clock();
    void tick();
    void pumpAudio();
    void toggle();
    void stop();
    void beginDrag();
    void previewSeek(int ms);
    void endDrag();
    void onSliderValue(int ms);
    void applySeek(bool resume);
    void closeClicked();
    void showFrame(const std::vector<uint8_t>& f);
    void syncSlider(int ms);
    static QString fmt(double s);

    Preview pv_;
    bool hasPv_ = false;

    // 音频子进程 + 有界队列（2×2KB，背压天然限速）
    Process* aproc_ = nullptr;
    bool aprocAlive_ = false;
    std::thread areader_;
    std::mutex amx_;
    std::condition_variable acv_;
    std::deque<std::vector<uint8_t>> aq_;
    bool astop_ = false, aeof_ = false;
    std::vector<uint8_t> arest_;
    qint64 awritten_ = 0;
    double astart_ = 0.0;

    QAudioSink* sink_ = nullptr;
    QIODevice* aio_ = nullptr;
    int byterate_ = 44100;

    QElapsedTimer elapsed_;
    double t0_ = 0.0;

    QTimer vtimer_, atimer_, seekTimer_;
    bool dragging_ = false, wasPlaying_ = false, clickResume_ = false, syncing_ = false;

    QLabel* img_ = nullptr;
    qfw::FluentPushButton* playBtn_ = nullptr;
    qfw::FluentPushButton* stopBtn_ = nullptr;
    qfw::FluentPushButton* closeBtn_ = nullptr;
    qfw::Slider* slider_ = nullptr;
    qfw::Slider* volSlider_ = nullptr;
    qfw::CaptionLabel* timeLbl_ = nullptr;

    int maxW_ = 640, maxH_ = 480;
};
