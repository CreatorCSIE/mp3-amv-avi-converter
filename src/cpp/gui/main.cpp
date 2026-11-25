// AMV Converter C++ GUI 壳（PySide6-Fluent-Widgets 版 1:1 翻译，Fluent 组件手搓子集）
#include <QApplication>

#include "fluent/style.h"
#include "main_window.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("AMV Converter");
    app.setOrganizationName("CreatorCSIE");
    applyFluentTheme(&app);
    MainWindow w;
    w.show();
    return app.exec();
}
