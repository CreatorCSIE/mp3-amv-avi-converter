#pragma once
#include "fluent/nav_window.h"

class ConvertInterface;
class PreviewInterface;

class MainWindow : public FluentWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    ConvertInterface* convertIface_;
    PreviewInterface* previewIface_;
};
