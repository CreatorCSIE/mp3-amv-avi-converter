#include "main_window.h"

#include "about_interface.h"
#include "convert_interface.h"
#include "preview_interface.h"

MainWindow::MainWindow(QWidget* parent) : qfw::FluentWindow(parent) {
    setWindowTitle("AMV Converter");
    resize(980, 680);
    convertIface_ = new ConvertInterface(this);
    addSubInterface(convertIface_, qfw::FluentIconEnum::Video, "转换");
    previewIface_ = new PreviewInterface(this);
    addSubInterface(previewIface_, qfw::FluentIconEnum::Zoom, "预览");
    addSubInterface(new AboutInterface(this), qfw::FluentIconEnum::Info, "关于",
                    qfw::NavigationItemPosition::Bottom);
    navigationInterface_->setExpandWidth(180);
}
