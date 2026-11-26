#pragma once
#include <QWidget>

namespace qfw {
class LineEdit;
}  // namespace qfw
class QDragEnterEvent;
class QDropEvent;
class MediaPlayerWidget;

// 侧边栏预览页：独立打开/拖入任意媒体文件，逐帧预览（复用共享播放组件）
class PreviewInterface : public QWidget {
    Q_OBJECT
public:
    explicit PreviewInterface(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void openFile();
    void load(const QString& path);

    qfw::LineEdit* pathEdit_ = nullptr;
    MediaPlayerWidget* player_ = nullptr;
};
