#include "icons.h"

#include <QPainter>
#include <QPolygonF>

QIcon fluentIcon(FluentIcon id) {
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QColor c("#3b3b3b");
    QPen pen(c, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (id) {
    case FluentIcon::PLAY: {
        p.setBrush(c);
        QPolygonF tri; tri << QPointF(4.5, 3) << QPointF(4.5, 13) << QPointF(13.5, 8);
        p.drawPolygon(tri);
        break;
    }
    case FluentIcon::PAUSE:
        p.setBrush(c);
        p.drawRect(QRectF(4, 3, 2.6, 10));
        p.drawRect(QRectF(9.4, 3, 2.6, 10));
        break;
    case FluentIcon::CLOSE:
        p.drawLine(4, 4, 12, 12);
        p.drawLine(12, 4, 4, 12);
        break;
    case FluentIcon::ADD:
        p.drawLine(8, 3, 8, 13);
        p.drawLine(3, 8, 13, 8);
        break;
    case FluentIcon::FOLDER:
        p.drawRoundedRect(QRectF(2, 4.5, 12, 8.5), 1.5, 1.5);
        p.drawLine(2, 4.5, 5.5, 4.5);
        p.drawLine(5.5, 4.5, 7, 6.5);
        break;
    case FluentIcon::FOLDER_ADD:
        p.drawRoundedRect(QRectF(2, 5.5, 9.5, 7.5), 1.5, 1.5);
        p.drawLine(2, 5.5, 4.8, 5.5);
        p.drawLine(4.8, 5.5, 6.1, 7);
        p.drawLine(12.5, 2.5, 12.5, 8);
        p.drawLine(9.75, 5.25, 15.25, 5.25);
        break;
    case FluentIcon::DELETE_:
        p.drawLine(3, 4, 13, 4);
        p.drawLine(6, 2.2, 10, 2.2);
        p.drawLine(6, 2.2, 6, 4);
        p.drawLine(10, 2.2, 10, 4);
        p.drawPolyline(QPolygonF() << QPointF(4.2, 4) << QPointF(5.2, 13.8)
                                   << QPointF(10.8, 13.8) << QPointF(11.8, 4));
        p.drawLine(6.8, 6.5, 7.1, 11.5);
        p.drawLine(9.2, 6.5, 8.9, 11.5);
        break;
    case FluentIcon::VIDEO: {
        p.drawRoundedRect(QRectF(2, 4, 12, 9), 1.5, 1.5);
        p.setBrush(c);
        QPolygonF tri2; tri2 << QPointF(6.5, 6.2) << QPointF(6.5, 10.8) << QPointF(10.8, 8.5);
        p.drawPolygon(tri2);
        break;
    }
    case FluentIcon::ZOOM:
        p.drawEllipse(QRectF(3, 3, 8, 8));
        p.drawLine(10.5, 10.5, 14, 14);
        break;
    case FluentIcon::INFO:
        p.drawEllipse(QRectF(2, 2, 12, 12));
        p.drawPoint(8, 5.2);
        p.drawLine(8, 7.5, 8, 11);
        break;
    case FluentIcon::DOWN:
        p.drawPolyline(QPolygonF() << QPointF(4, 6.2) << QPointF(8, 10.2) << QPointF(12, 6.2));
        break;
    case FluentIcon::UP:
        p.drawPolyline(QPolygonF() << QPointF(4, 9.8) << QPointF(8, 5.8) << QPointF(12, 9.8));
        break;
    case FluentIcon::COPY:
        p.drawRoundedRect(QRectF(3, 5, 7.5, 8.5), 1.2, 1.2);
        p.drawPolyline(QPolygonF() << QPointF(6, 4.2) << QPointF(6, 2.5) << QPointF(13, 2.5)
                                   << QPointF(13, 10) << QPointF(11.8, 10));
        break;
    case FluentIcon::GITHUB:
        p.drawEllipse(QRectF(2, 2, 12, 12));
        p.drawLine(2.5, 8, 13.5, 8);
        p.drawEllipse(QRectF(5.2, 2, 5.6, 12));
        break;
    case FluentIcon::LINK:
        p.drawEllipse(QRectF(2.5, 2.5, 5, 5));
        p.drawEllipse(QRectF(8.5, 8.5, 5, 5));
        p.drawLine(6.6, 6.6, 9.4, 9.4);
        break;
    }
    p.end();
    return QIcon(pm);
}
