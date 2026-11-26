// AMV Converter C++ GUI 壳（PySide6-Fluent-Widgets 版 1:1 翻译，组件=Qt-Fluent-Widgets）
#include <QApplication>
#include <qtfluentwidgets.h>

#include "main_window.h"

int main(int argc, char** argv) {
    // qtfluentwidgets 是静态库：qrc（图标 SVG/主题表）不会自动注册，必须显式初始化，
    // 否则标题栏按钮与导航图标缺失（MinGW 实测定案）
    Q_INIT_RESOURCE(resource);
    QApplication app(argc, argv);
    app.setApplicationName("AMV Converter");
    app.setOrganizationName("CreatorCSIE");
    qfw::setTheme(qfw::Theme::Light);
    MainWindow w;
    w.show();
    return app.exec();
}
