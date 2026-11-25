#pragma once
#include <QIcon>
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QStackedWidget;

// FluentWindow 替身：左侧导航栏（图标+文字）+ QStackedWidget 内容区。
// bottom=true 的接口钉在导航底部（对应 NavigationItemPosition.BOTTOM）。
class FluentWindow : public QWidget {
    Q_OBJECT
public:
    explicit FluentWindow(QWidget* parent = nullptr);
    void addSubInterface(QWidget* iface, const QIcon& icon, const QString& text, bool bottom = false);

private slots:
    void onNavChanged(QListWidgetItem* item);

private:
    QListWidget* topNav_;
    QListWidget* bottomNav_;
    QStackedWidget* stack_;
    bool switching_ = false;
};
