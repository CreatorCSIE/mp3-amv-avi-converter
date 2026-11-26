#include "preview_interface.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMimeData>
#include <QUrl>
#include <QVBoxLayout>
#include <qtfluentwidgets.h>

#include <algorithm>

#include "common.h"
#include "media_player.h"

namespace {
QString fileFilter() {
    QStringList stars;
    QStringList exts = videoExts();
    std::sort(exts.begin(), exts.end());
    for (const QString& e : exts) stars << "*" + e;
    return "媒体文件 (" + stars.join(" ") + ");;所有文件 (*)";
}
}  // namespace

PreviewInterface::PreviewInterface(QWidget* parent) : QWidget(parent) {
    setObjectName("previewInterface");
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(36, 20, 36, 20);
    lay->setSpacing(12);

    auto* row = new QHBoxLayout();
    auto* openBtn = new qfw::FluentPushButton(qfw::FluentIcon(qfw::FluentIconEnum::Folder),
                                              "打开文件", this);
    connect(openBtn, &qfw::FluentPushButton::clicked, this, &PreviewInterface::openFile);
    pathEdit_ = new qfw::LineEdit(this);
    pathEdit_->setReadOnly(true);
    pathEdit_->setPlaceholderText("选择或拖入视频文件（AMV / AVI / MP4 …）");
    row->addWidget(openBtn);
    row->addWidget(pathEdit_, 1);
    lay->addLayout(row);

    player_ = new MediaPlayerWidget(this);
    connect(player_, &MediaPlayerWidget::mediaClosed, pathEdit_, &qfw::LineEdit::clear);
    lay->addWidget(player_, 1);
    lay->addWidget(new qfw::CaptionLabel(
        "内嵌含声预览（音量滑条实时生效）；画面 = 设备显示取向，所见即所得。", this));
    setAcceptDrops(true);
}

void PreviewInterface::openFile() {
    QString f = QFileDialog::getOpenFileName(this, "选择视频文件", "", fileFilter());
    if (!f.isEmpty()) load(f);
}

void PreviewInterface::load(const QString& path) {
    if (player_->setPath(path)) pathEdit_->setText(path);
}

void PreviewInterface::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void PreviewInterface::dropEvent(QDropEvent* e) {
    for (const QUrl& u : e->mimeData()->urls()) {
        QString p = u.toLocalFile();
        if (isMediaFile(p)) {
            load(p);
            break;
        }
    }
}
