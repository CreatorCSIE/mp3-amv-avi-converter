"""侧边栏预览页：独立打开/拖入任意媒体文件，逐帧预览（复用共享播放组件）。"""

from __future__ import annotations

from pathlib import Path

from PySide6.QtWidgets import QFileDialog, QHBoxLayout, QVBoxLayout, QWidget
from qfluentwidgets import CaptionLabel, FluentIcon, LineEdit, PushButton

from .main_window import VIDEO_EXT
from .media_player import MediaPlayerWidget

FILE_FILTER = "媒体文件 (" + " ".join("*" + e for e in sorted(VIDEO_EXT)) + ");;所有文件 (*)"


class PreviewInterface(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("previewInterface")
        lay = QVBoxLayout(self)
        lay.setContentsMargins(36, 20, 36, 20)
        lay.setSpacing(12)

        row = QHBoxLayout()
        open_btn = PushButton(FluentIcon.FOLDER, "打开文件")
        open_btn.clicked.connect(self._open)
        self.path_edit = LineEdit()
        self.path_edit.setReadOnly(True)
        self.path_edit.setPlaceholderText("选择或拖入视频文件（AMV / AVI / MP4 …）")
        row.addWidget(open_btn)
        row.addWidget(self.path_edit, 1)
        lay.addLayout(row)

        self.player = MediaPlayerWidget()
        self.player.mediaClosed.connect(self.path_edit.clear)
        lay.addWidget(self.player, 1)
        lay.addWidget(CaptionLabel(
            "内嵌含声预览（音量滑条实时生效）；画面 = 设备显示取向，所见即所得。"))
        self.setAcceptDrops(True)

    def _open(self):
        f, _ = QFileDialog.getOpenFileName(self, "选择视频文件", "", FILE_FILTER)
        if f:
            self.load(Path(f))

    def load(self, path: Path):
        if self.player.set_path(path):
            self.path_edit.setText(str(path))

    def dragEnterEvent(self, e):
        if e.mimeData().hasUrls():
            e.acceptProposedAction()

    def dropEvent(self, e):
        for u in e.mimeData().urls():
            p = Path(u.toLocalFile())
            if p.is_file() and p.suffix.lower() in VIDEO_EXT:
                self.load(p)
                break
