#pragma once
#include <QDialog>
#include <QString>

class MediaPlayerWidget;

// 转换结果预览对话框：复用侧边栏预览页的共享播放组件
class PreviewDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreviewDialog(const QString& path, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    MediaPlayerWidget* player_;
};
