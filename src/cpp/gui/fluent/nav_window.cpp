#include "nav_window.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "widgets.h"

FluentWindow::FluentWindow(QWidget* parent) : QWidget(parent) {
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* panel = new QWidget(this);
    panel->setObjectName("navPanel");
    panel->setFixedWidth(180);  // setExpandWidth(180) 定案：固定展开宽度
    auto* pv = new QVBoxLayout(panel);
    pv->setContentsMargins(8, 14, 8, 12);
    pv->setSpacing(8);

    auto* title = new StrongBodyLabel("AMV Converter", panel);
    title->setContentsMargins(8, 0, 0, 4);
    pv->addWidget(title);

    topNav_ = new QListWidget(panel);
    topNav_->setObjectName("nav");
    topNav_->setFrameShape(QFrame::NoFrame);
    topNav_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pv->addWidget(topNav_, 1);

    bottomNav_ = new QListWidget(panel);
    bottomNav_->setObjectName("nav");
    bottomNav_->setFrameShape(QFrame::NoFrame);
    bottomNav_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pv->addWidget(bottomNav_);

    stack_ = new QStackedWidget(this);
    outer->addWidget(panel);
    outer->addWidget(stack_, 1);

    connect(topNav_, &QListWidget::currentItemChanged, this, &FluentWindow::onNavChanged);
    connect(bottomNav_, &QListWidget::currentItemChanged, this, &FluentWindow::onNavChanged);
}

void FluentWindow::addSubInterface(QWidget* iface, const QIcon& icon, const QString& text, bool bottom) {
    int idx = stack_->addWidget(iface);
    auto* nav = bottom ? bottomNav_ : topNav_;
    auto* item = new QListWidgetItem(icon, text, nav);
    item->setData(Qt::UserRole, idx);
    item->setSizeHint(QSize(0, 36));
    if (stack_->count() == 1) nav->setCurrentItem(item);  // 首个接口默认选中
}

void FluentWindow::onNavChanged(QListWidgetItem* item) {
    if (switching_ || !item) return;
    switching_ = true;
    (item->listWidget() == topNav_ ? bottomNav_ : topNav_)->clearSelection();
    stack_->setCurrentIndex(item->data(Qt::UserRole).toInt());
    switching_ = false;
}
