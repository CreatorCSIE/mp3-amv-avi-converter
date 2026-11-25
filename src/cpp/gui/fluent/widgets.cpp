#include "widgets.h"

#include <QHBoxLayout>
#include <QTimer>

void InfoBar::error(const QString& title, const QString& msg, QWidget* parent) {
    auto* bar = new QWidget(parent);
    bar->setObjectName("infobar");
    auto* lay = new QHBoxLayout(bar);
    lay->setContentsMargins(14, 10, 14, 10);
    lay->setSpacing(10);
    auto* t = new QLabel(title, bar);
    t->setObjectName("infobarTitle");
    auto* m = new QLabel(msg, bar);
    m->setObjectName("infobarMsg");
    m->setWordWrap(true);
    m->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(t);
    lay->addWidget(m, 1);
    bar->adjustSize();
    int w = qMin(bar->sizeHint().width(), parent->width() - 48);
    bar->setFixedWidth(w > 240 ? w : 240);
    bar->move((parent->width() - bar->width()) / 2, 16);
    bar->show();
    bar->raise();
    QTimer::singleShot(5000, bar, &QWidget::deleteLater);
}
