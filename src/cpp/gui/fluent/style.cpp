#include "style.h"

#include <QApplication>

void applyFluentTheme(QApplication* app) {
    static const char* qss = R"(
QWidget { background-color: #f3f3f3; color: #1b1b1b;
          font-family: "Segoe UI","Microsoft YaHei UI"; font-size: 14px; }
QLabel#caption { color: #616161; font-size: 12px; }
QLabel#subtitle { font-size: 20px; font-weight: 700; }
QLabel#strong { font-weight: 600; }
QPushButton { background-color: #fbfbfb; border: 1px solid #d7d7d7;
              border-radius: 5px; padding: 5px 14px; }
QPushButton:hover { background-color: #f0f0f0; border-color: #c8c8c8; }
QPushButton:pressed { background-color: #e5e5e5; }
QPushButton:disabled { color: #a0a0a0; background-color: #f5f5f5; }
QPushButton#primary { background-color: #0078d4; color: white; border: none; }
QPushButton#primary:hover { background-color: #106ebe; }
QPushButton#primary:pressed { background-color: #1a7aaf; }
QPushButton#primary:disabled { background-color: #b6d6ef; }
QPushButton#transparent { background: transparent; border: none;
                         text-align: left; padding: 4px 6px; }
QPushButton#transparent:hover { background: #eaeaea; }
QLineEdit, QComboBox { background: #fbfbfb; border: 1px solid #d7d7d7;
    border-bottom: 2px solid #8a8a8a; border-radius: 5px; padding: 5px 10px;
    selection-background-color: #0078d4; selection-color: white; }
QLineEdit:focus, QComboBox:focus { border-bottom: 2px solid #0078d4; background: #fdfdfd; }
QLineEdit:disabled, QComboBox:disabled { color: #a0a0a0; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView { background: #fbfbfb; border: 1px solid #d0d0d0;
    selection-background-color: #e0e0e0; selection-color: #1b1b1b; }
QFrame#card { background: #fbfbfb; border: 1px solid #ececec; border-radius: 8px; }
QTableWidget#fluentTable { background: #fbfbfb; border: 1px solid #e0e0e0;
    border-radius: 8px; gridline-color: #f0f0f0; outline: none; }
QHeaderView::section { background: #f7f7f7; border: none;
    border-bottom: 1px solid #e0e0e0; padding: 6px; font-weight: 600; }
QProgressBar { background: #e6e6e6; border: none; border-radius: 4px;
               min-height: 6px; max-height: 6px; }
QProgressBar::chunk { background: #0078d4; border-radius: 4px; }
QSlider::groove:horizontal { height: 4px; background: #d5d5d5; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #0078d4; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0;
    border-radius: 7px; background: white; border: 1px solid #0078d4; }
QSlider::handle:horizontal:hover { background: #dcecf8; }
QWidget#navPanel { background: #ebebeb; border-right: 1px solid #e0e0e0; }
QListWidget#nav { background: transparent; border: none; outline: none; }
QListWidget#nav::item { border-radius: 5px; padding: 8px 10px; margin: 2px 6px; }
QListWidget#nav::item:hover { background: #e8e8e8; }
QListWidget#nav::item:selected { background: #dedede; color: #1b1b1b; }
QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }
QWidget#infobar { background: #fde7e9; border: 1px solid #f1b0b7; border-radius: 8px; }
QLabel#infobarTitle { font-weight: 600; color: #a1260d; }
QLabel#infobarMsg { color: #442525; }
)";
    app->setStyleSheet(qss);
}
