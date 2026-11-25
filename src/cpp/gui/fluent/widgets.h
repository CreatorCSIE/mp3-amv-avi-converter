#pragma once
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QString>

#include "icons.h"

// qfluentwidgets 常用组件的 Qt Widgets 替身（objectName 驱动 QSS 外观）

class CaptionLabel : public QLabel {
public:
    explicit CaptionLabel(const QString& text, QWidget* parent = nullptr) : QLabel(text, parent) {
        setObjectName("caption");
        setWordWrap(true);
    }
};

class BodyLabel : public QLabel {
public:
    explicit BodyLabel(const QString& text, QWidget* parent = nullptr) : QLabel(text, parent) {
        setTextFormat(Qt::RichText);
    }
};

class StrongBodyLabel : public QLabel {
public:
    explicit StrongBodyLabel(const QString& text, QWidget* parent = nullptr) : QLabel(text, parent) {
        setObjectName("strong");
    }
};

class SubtitleLabel : public QLabel {
public:
    explicit SubtitleLabel(const QString& text, QWidget* parent = nullptr) : QLabel(text, parent) {
        setObjectName("subtitle");
    }
};

class PushButton : public QPushButton {
public:
    explicit PushButton(const QString& text = "", QWidget* parent = nullptr) : QPushButton(text, parent) {}
    PushButton(FluentIcon icon, const QString& text, QWidget* parent = nullptr)
        : QPushButton(fluentIcon(icon), text, parent) {}
};

class PrimaryPushButton : public PushButton {
public:
    explicit PrimaryPushButton(const QString& text = "", QWidget* parent = nullptr) : PushButton(text, parent) {
        setObjectName("primary");
    }
    PrimaryPushButton(FluentIcon icon, const QString& text, QWidget* parent = nullptr) : PushButton(icon, text, parent) {
        setObjectName("primary");
    }
};

class TransparentPushButton : public PushButton {
public:
    explicit TransparentPushButton(const QString& text = "", QWidget* parent = nullptr) : PushButton(text, parent) {
        setObjectName("transparent");
    }
    TransparentPushButton(FluentIcon icon, const QString& text, QWidget* parent = nullptr) : PushButton(icon, text, parent) {
        setObjectName("transparent");
    }
};

class CardWidget : public QFrame {
public:
    explicit CardWidget(QWidget* parent = nullptr) : QFrame(parent) { setObjectName("card"); }
};

// InfoBar.error 替身：父窗口顶部居中红色横幅，5 秒自动消失
class InfoBar {
public:
    static void error(const QString& title, const QString& msg, QWidget* parent);
};
