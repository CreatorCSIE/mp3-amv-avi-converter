#include "main_window.h"

#include "about_interface.h"
#include "convert_interface.h"
#include "fluent/icons.h"
#include "preview_interface.h"

MainWindow::MainWindow(QWidget* parent) : FluentWindow(parent) {
    setWindowTitle("AMV Converter");
    resize(980, 680);
    convertIface_ = new ConvertInterface(this);
    addSubInterface(convertIface_, fluentIcon(FluentIcon::VIDEO), "转换");
    previewIface_ = new PreviewInterface(this);
    addSubInterface(previewIface_, fluentIcon(FluentIcon::ZOOM), "预览");
    addSubInterface(new AboutInterface(this), fluentIcon(FluentIcon::INFO), "关于", true);
}
