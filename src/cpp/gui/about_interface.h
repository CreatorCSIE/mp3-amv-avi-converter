#pragma once
#include <QFrame>
#include <QString>
#include <QWidget>

class QVBoxLayout;
class TransparentPushButton;

// 折叠分组（ClassIsland“更多设置”语义；qfluentwidgets 无 ExpanderGroup，原版即手搓）
class FoldGroup : public QFrame {
    Q_OBJECT
public:
    explicit FoldGroup(const QString& title, QWidget* parent = nullptr);
    void addWidget(QWidget* w, int stretch = 0, Qt::Alignment align = Qt::AlignLeft);

private:
    void toggle();
    TransparentPushButton* head_;
    QWidget* body_;
    QVBoxLayout* bodyLay_;
    bool open_ = false;
};

class AboutInterface : public QWidget {
    Q_OBJECT
public:
    explicit AboutInterface(QWidget* parent = nullptr);
};
