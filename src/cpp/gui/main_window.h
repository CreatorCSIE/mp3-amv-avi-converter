#pragma once
#include <qtfluentwidgets.h>

class ConvertInterface;
class PreviewInterface;

class MainWindow : public qfw::FluentWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    ConvertInterface* convertIface_;
    PreviewInterface* previewIface_;
};
