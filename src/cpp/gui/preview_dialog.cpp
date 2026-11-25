#include "preview_dialog.h"

#include <QCloseEvent>
#include <QFileInfo>
#include <QVBoxLayout>

#include "media_player.h"

PreviewDialog::PreviewDialog(const QString& path, QWidget* parent) : QDialog(parent) {
    setWindowTitle("预览 - " + QFileInfo(path).fileName());
    auto* lay = new QVBoxLayout(this);
    player_ = new MediaPlayerWidget(path, this);
    lay->addWidget(player_);
}

void PreviewDialog::closeEvent(QCloseEvent* e) {
    player_->closeMedia();
    QDialog::closeEvent(e);
}
