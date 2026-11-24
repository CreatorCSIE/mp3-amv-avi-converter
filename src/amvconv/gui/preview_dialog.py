"""转换结果预览对话框：逐帧播放（无音频）+ 拖动定位。

画面即 ffmpeg 解码输出 = 设备显示取向（AMV 解码自带 vflip 补偿），所见即所得。
"""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QImage, QPixmap
from PySide6.QtWidgets import QDialog, QHBoxLayout, QLabel, QVBoxLayout
from qfluentwidgets import CaptionLabel, PushButton, Slider, FluentIcon

from ..core.preview import Preview

MAX_W, MAX_H = 640, 480


class PreviewDialog(QDialog):
    def __init__(self, path: Path, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"预览 - {Path(path).name}")
        self.pv = Preview(Path(path))
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)

        lay = QVBoxLayout(self)
        self.img = QLabel("加载中…")
        self.img.setMinimumSize(MAX_W, MAX_H)
        self.img.setAlignment(Qt.AlignCenter)
        lay.addWidget(self.img, 1)

        row = QHBoxLayout()
        self.play_btn = PushButton(FluentIcon.PLAY, "播放")
        self.play_btn.clicked.connect(self._toggle)
        self.slider = Slider(Qt.Horizontal)
        self.slider.setRange(0, max(1, int(self.pv.duration * 1000)))
        self.slider.sliderMoved.connect(self._seek)
        self.time_lbl = CaptionLabel("0:00 / " + self._fmt(self.pv.duration))
        row.addWidget(self.play_btn)
        row.addWidget(self.slider, 1)
        row.addWidget(self.time_lbl)
        lay.addLayout(row)

        self._show(self.pv.read())

    def _fmt(self, s: float) -> str:
        s = max(0, int(s))
        return f"{s // 60}:{s % 60:02d}"

    def _toggle(self):
        if self._timer.isActive():
            self._timer.stop()
            self.play_btn.setText("播放")
        else:
            self.play_btn.setText("暂停")
            self._timer.start(int(1000 / max(1, self.pv.fps)))

    def _tick(self):
        frame = self.pv.read()
        if frame is None:  # 流尾：重头
            self.pv.seek(0.0)
            frame = self.pv.read()
        self._show(frame)
        self.slider.blockSignals(True)
        self.slider.setValue(int(self.pv.position * 1000))
        self.slider.blockSignals(False)
        self.time_lbl.setText(self._fmt(self.pv.position) + " / " + self._fmt(self.pv.duration))

    def _seek(self, ms: int):
        self._timer.stop()
        self.play_btn.setText("播放")
        self.pv.seek(ms / 1000.0)
        self._show(self.pv.read())

    def _show(self, frame):
        if frame is None:
            return
        h, w, _ = frame.shape
        qimg = QImage(frame.data, w, h, 3 * w, QImage.Format.Format_RGB888).copy()
        pix = QPixmap.fromImage(qimg)
        if pix.width() > MAX_W or pix.height() > MAX_H:
            pix = pix.scaled(MAX_W, MAX_H, Qt.KeepAspectRatio, Qt.SmoothTransformation)
        self.img.setPixmap(pix)

    def closeEvent(self, e):
        self._timer.stop()
        self.pv.close()
        super().closeEvent(e)
