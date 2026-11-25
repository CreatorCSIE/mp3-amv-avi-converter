#pragma once
#include <QIcon>

// FluentIcon 的手搓替身：16px QPainter 单色字形（无第三方图标资源）
enum class FluentIcon {
    FOLDER_ADD, ADD, PLAY, PAUSE, DELETE_, CLOSE, FOLDER,
    VIDEO, ZOOM, INFO, DOWN, UP, COPY, GITHUB, LINK,
};

QIcon fluentIcon(FluentIcon id);
