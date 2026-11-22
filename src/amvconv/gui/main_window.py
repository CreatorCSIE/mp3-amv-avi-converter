"""最小 PySide6 壳 —— 仅演示 core 复用方式，功能随逆向结论逐步补齐。

运行：python -m amvconv.gui.main_window
"""

from __future__ import annotations

import sys
from pathlib import Path

from PySide6.QtCore import QThread, Signal
from PySide6.QtWidgets import (
    QApplication, QComboBox, QFileDialog, QHBoxLayout, QLabel, QLineEdit,
    QMainWindow, QPushButton, QTextEdit, QVBoxLayout, QWidget,
)

from ..core import profiles
from ..core.models import ConversionJob
from ..core.profiles import Profile


class _Worker(QThread):
    done = Signal(str)

    def __init__(self, job: ConversionJob):
        super().__init__()
        self.job = job

    def run(self):
        from ..core import ffmpeg

        res = ffmpeg.convert(self.job)
        self.done.emit("转换完成: " + str(self.job.target) if res.ok else f"失败 rc={res.returncode}\n{res.stderr_tail}")


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("AMV Converter（重写版 · 逆向进行中）")
        root = QWidget()
        lay = QVBoxLayout(root)

        row1 = QHBoxLayout()
        self.src_edit = QLineEdit()
        row1.addWidget(QLabel("源文件"))
        row1.addWidget(self.src_edit)
        pick = QPushButton("…")
        pick.clicked.connect(self._pick)
        row1.addWidget(pick)
        lay.addLayout(row1)

        row2 = QHBoxLayout()
        self.size_combo = QComboBox()
        for (w, h) in profiles.UI_PROFILES:
            self.size_combo.addItem(f"{w}x{h}")
        self.fps_combo = QComboBox()
        for f in profiles.EXACT_DIVISOR_FPS[6:]:  # 只列 14~30 的常用合法帧率
            self.fps_combo.addItem(str(f))
        self.fps_combo.setCurrentText("15")
        row2.addWidget(QLabel("分辨率"))
        row2.addWidget(self.size_combo)
        row2.addWidget(QLabel("帧率"))
        row2.addWidget(self.fps_combo)
        lay.addLayout(row2)

        self.run_btn = QPushButton("转换为 AMV")
        self.run_btn.clicked.connect(self._run)
        lay.addWidget(self.run_btn)

        self.log = QTextEdit(readOnly=True)
        lay.addWidget(self.log)
        self.setCentralWidget(root)

    def _pick(self):
        p, _ = QFileDialog.getOpenFileName(self, "选择视频", "", "视频 (*.mp4 *.avi *.mkv *.mp3 *.mov *.wmv *.flv)")
        if p:
            self.src_edit.setText(p)

    def _run(self):
        src = Path(self.src_edit.text())
        if not src.is_file():
            self.log.append("请选择存在的源文件")
            return
        w, h = (int(v) for v in self.size_combo.currentText().split("x"))
        job = ConversionJob(source=src, target=src.with_suffix(".amv"), profile=Profile(w, h, int(self.fps_combo.currentText())))
        self.run_btn.setEnabled(False)
        self._worker = _Worker(job)
        self._worker.done.connect(lambda msg: (self.log.append(msg), self.run_btn.setEnabled(True)))
        self._worker.start()


def main():
    app = QApplication(sys.argv)
    MainWindow().show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
