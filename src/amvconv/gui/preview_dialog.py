"""转换结果预览对话框：复用侧边栏预览页的共享播放组件。"""

from __future__ import annotations

from pathlib import Path

from PySide6.QtWidgets import QDialog, QVBoxLayout

from .media_player import MediaPlayerWidget


class PreviewDialog(QDialog):
    def __init__(self, path: Path, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"预览 - {Path(path).name}")
        lay = QVBoxLayout(self)
        self.player = MediaPlayerWidget(Path(path))
        lay.addWidget(self.player)

    def closeEvent(self, e):
        self.player.close_media()
        super().closeEvent(e)
