"""PySide6 + QFluentWidgets 主窗口：批量转换队列（AMV/AVI 双模式）。

运行：python -m amvconv.gui.main_window（或 amv-conv-gui 入口）
"""

from __future__ import annotations

import sys
import threading
from pathlib import Path

from PySide6.QtCore import Qt, QThread, Signal
from PySide6.QtGui import QDragEnterEvent, QDropEvent
from PySide6.QtWidgets import (
    QApplication, QFileDialog, QHBoxLayout, QHeaderView, QTableWidgetItem,
    QVBoxLayout, QWidget,
)
from qfluentwidgets import (
    CaptionLabel, ComboBox, FluentIcon, FluentWindow, InfoBar, InfoBarPosition,
    LineEdit, PrimaryPushButton, ProgressBar, PushButton, TableWidget,
)

from ..core import profiles
from ..core.models import ConversionJob
from ..core.profiles import Profile

VIDEO_EXT = {".mp4", ".avi", ".mkv", ".mp3", ".mov", ".wmv", ".flv", ".webm", ".ts", ".wav"}


class _QueueWorker(QThread):
    """单线程顺序消费队列（转换是 CPU 重活，不做并行）。"""

    progress = Signal(int, int, int)      # row, done, total
    finished_row = Signal(int, bool, str)  # row, ok, msg

    def __init__(self, parent=None):
        super().__init__(parent)
        self._jobs: list[tuple[int, object, threading.Event]] = []
        self._lock = threading.Lock()

    def enqueue(self, row: int, job, cancel: threading.Event) -> None:
        with self._lock:
            self._jobs.append((row, job, cancel))

    def run(self):
        while True:
            with self._lock:
                if not self._jobs:
                    return
                row, job, cancel = self._jobs.pop(0)
            try:
                ok, msg = self._run_one(job, cancel, row)
            except Exception as e:  # noqa: BLE001 —— 单行失败不拖垮队列
                ok, msg = False, f"异常: {e}"
            self.finished_row.emit(row, ok, msg)

    def _run_one(self, job, cancel, row) -> tuple[bool, str]:
        prog = lambda d, t: self.progress.emit(row, d, t or 0)  # noqa: E731
        if isinstance(job, tuple):  # AVI: (source, target, w, h, q)
            from ..core import aviwriter, ffmpeg as ffmod
            src, dst, w, h, q = job
            try:
                dur = float(ffmod.probe(src)["format"].get("duration", 0)) or None
            except Exception:
                dur = None
            res = aviwriter.convert_avi(src, dst, w, h, progress=prog, cancel=cancel,
                                        duration=dur, q=q)
            return res.returncode == 0, res.stderr if res.returncode else str(dst)
        from ..core import ffmpeg
        res = ffmpeg.convert(job, progress=prog, cancel=cancel)
        return res.ok, (str(job.target) if res.ok else res.stderr_tail)


class ConvertInterface(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("convertInterface")
        self._cancels: dict[int, threading.Event] = {}
        self._src_dirs: list[Path] = []
        self._build()
        self.setAcceptDrops(True)

    def _build(self):
        lay = QVBoxLayout(self)
        lay.setContentsMargins(36, 20, 36, 20)
        lay.setSpacing(12)

        row = QHBoxLayout()
        self.mode_combo = ComboBox()
        self.mode_combo.addItems(["AMV（设备用）", "AVI（播放器友好）"])
        self.size_combo = ComboBox()
        for (w, h) in profiles.UI_PROFILES:
            self.size_combo.addItem(f"{w}x{h}")
        self.size_combo.setCurrentText("160x120")
        self.fps_combo = ComboBox()
        self._refresh_fps()
        self.size_combo.currentTextChanged.connect(self._refresh_fps)
        self.mode_combo.currentIndexChanged.connect(self._on_mode)
        row.addWidget(self._label("模式"))
        row.addWidget(self.mode_combo)
        row.addWidget(self._label("分辨率"))
        row.addWidget(self.size_combo)
        row.addWidget(self._label("帧率"))
        row.addWidget(self.fps_combo)
        row.addStretch(1)
        lay.addLayout(row)

        row2 = QHBoxLayout()
        self.out_edit = LineEdit()
        self.out_edit.setPlaceholderText("输出目录（留空 = 源文件同目录）")
        pick = PushButton(FluentIcon.FOLDER_ADD, "…")
        pick.clicked.connect(self._pick_out)
        add_btn = PrimaryPushButton(FluentIcon.ADD, "添加文件")
        add_btn.clicked.connect(self._add_files)
        row2.addWidget(self.out_edit, 1)
        row2.addWidget(pick)
        row2.addWidget(add_btn)
        lay.addLayout(row2)

        self.table = TableWidget()
        self.table.setBorderVisible(True)
        self.table.setBorderRadius(8)
        self.table.setColumnCount(4)
        self.table.setHorizontalHeaderLabels(["源文件", "输出", "状态", "进度"])
        self.table.verticalHeader().setVisible(False)
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.table.setColumnWidth(1, 220)
        self.table.setColumnWidth(2, 120)
        self.table.setColumnWidth(3, 200)
        self.table.cellClicked.connect(self._on_row_clicked)
        self.table.cellDoubleClicked.connect(self._on_preview)
        lay.addWidget(self.table, 1)

        row3 = QHBoxLayout()
        self.start_btn = PrimaryPushButton(FluentIcon.PLAY, "开始全部")
        self.start_btn.clicked.connect(self._start)
        self.clear_btn = PushButton(FluentIcon.DELETE, "清除已完成")
        self.clear_btn.clicked.connect(self._clear_done)
        self.hint = self._label("")
        row3.addWidget(self.start_btn)
        row3.addWidget(self.clear_btn)
        row3.addWidget(self.hint, 1)
        lay.addLayout(row3)

        self.worker = _QueueWorker(self)
        self.worker.progress.connect(self._on_progress)
        self.worker.finished_row.connect(self._on_done)

    @staticmethod
    def _label(text):
        return CaptionLabel(text)

    def _refresh_fps(self):
        w, h = (int(v) for v in self.size_combo.currentText().split("x"))
        self.fps_combo.clear()
        for f in profiles.UI_PROFILES.get((w, h), [25]):
            mark = "" if 22050 % f == 0 else "（自动改15）"
            self.fps_combo.addItem(f"{f}{mark}")
        idx = self.fps_combo.findText("25")
        self.fps_combo.setCurrentIndex(idx if idx >= 0 else 0)

    def _on_mode(self, idx):
        self.fps_combo.setEnabled(idx == 0)
        self.hint.setText("" if idx == 0 else "AVI 模式帧率恒 ~21.68（音频驱动），质量 q=4")

    def _pick_out(self):
        d = QFileDialog.getExistingDirectory(self, "选择输出目录")
        if d:
            self.out_edit.setText(d)

    def _add_files(self):
        files, _ = QFileDialog.getOpenFileNames(
            self, "选择视频/音频", "",
            "媒体文件 (*.mp4 *.avi *.mkv *.mp3 *.mov *.wmv *.flv *.webm *.ts *.wav)")
        self._append(files)

    def dragEnterEvent(self, e: QDragEnterEvent):
        if e.mimeData().hasUrls():
            e.acceptProposedAction()

    def dropEvent(self, e: QDropEvent):
        self._append([u.toLocalFile() for u in e.mimeData().urls()
                      if Path(u.toLocalFile()).suffix.lower() in VIDEO_EXT])

    def _append(self, files):
        files = [f for f in files if Path(f).is_file()
                 and Path(f).suffix.lower() in VIDEO_EXT]
        if not files:
            return
        self._src_dirs = list({*self._src_dirs, *(Path(f).parent for f in files)})
        for f in files:
            p = Path(f)
            r = self.table.rowCount()
            self.table.insertRow(r)
            self.table.setItem(r, 0, QTableWidgetItem(p.name))
            self.table.setItem(r, 2, QTableWidgetItem("等待中"))
            dst = self._target(p)
            it = QTableWidgetItem(dst.name)
            it.setData(Qt.UserRole, str(dst))
            self.table.setItem(r, 1, it)
            pb = ProgressBar()
            pb.setRange(0, 100)
            pb.setValue(0)
            self.table.setCellWidget(r, 3, pb)

    def _target(self, src: Path) -> Path:
        out_dir = self.out_edit.text().strip()
        ext = ".amv" if self.mode_combo.currentIndex() == 0 else ".avi"
        return (Path(out_dir) if out_dir else src.parent) / (src.stem + ext)

    def _start(self):
        avi = self.mode_combo.currentIndex() == 1
        w, h = (int(v) for v in self.size_combo.currentText().split("x"))
        fps = int(self.fps_combo.currentText().split("（")[0])
        started = 0
        for r in range(self.table.rowCount()):
            if self.table.item(r, 2).text() != "等待中":
                continue
            src = self._row_source(r)
            if src is None:
                continue
            dst = self._target(src)
            if avi:
                job = (src, dst, w, h, 4)
            else:
                prof = Profile(w, h, fps)
                if not prof.ffmpeg_safe:
                    prof = Profile(w, h, profiles.pick_closest_fps(fps))
                job = ConversionJob(source=src, target=dst, profile=prof)
            ev = threading.Event()
            self._cancels[r] = ev
            self.worker.enqueue(r, job, ev)
            self.table.item(r, 2).setText("转换中（点击取消）")
            started += 1
        if started and not self.worker.isRunning():
            self.worker.start()

    def _on_row_clicked(self, r, _c):
        item = self.table.item(r, 2)
        if item is not None and item.text().startswith("转换中"):
            self._cancels.get(r, threading.Event()).set()
            item.setText("取消中")

    def _on_preview(self, r, _c):
        if self.table.item(r, 2).text() != "完成":
            return
        dst = Path(self.table.item(r, 1).data(Qt.UserRole))
        if not dst.is_file():
            return
        from .preview_dialog import PreviewDialog
        PreviewDialog(dst, self).exec()

    def _row_source(self, r) -> Path | None:
        name = self.table.item(r, 0).text()
        out_dir = self.out_edit.text().strip()
        for d in ([Path(out_dir)] if out_dir else []) + list(self._src_dirs):
            p = d / name
            if p.is_file():
                return p
        return None

    def _on_progress(self, r, done, total):
        pb = self.table.cellWidget(r, 3)
        if pb is not None:
            pb.setValue(int(done * 100 / total) if total else 0)

    def _on_done(self, r, ok, msg):
        self.table.item(r, 2).setText("完成" if ok else "失败")
        pb = self.table.cellWidget(r, 3)
        if ok and pb is not None:
            pb.setValue(100)
        if not ok:
            InfoBar.error("转换失败", msg[-300:], parent=self,
                          position=InfoBarPosition.TOP, duration=5000)

    def _clear_done(self):
        for r in reversed(range(self.table.rowCount())):
            if self.table.item(r, 2).text() in ("完成", "失败"):
                self._cancels.pop(r, None)
                self.table.removeRow(r)


class MainWindow(FluentWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("AMV Converter")
        self.resize(980, 680)
        self.convert_iface = ConvertInterface(self)
        self.addSubInterface(self.convert_iface, FluentIcon.VIDEO, "转换")
        from .about_interface import AboutInterface
        from qfluentwidgets import NavigationItemPosition
        self.addSubInterface(AboutInterface(self), FluentIcon.INFO, "关于",
                             position=NavigationItemPosition.BOTTOM)
        self.navigationInterface.setExpandWidth(180)


def main():
    app = QApplication(sys.argv)
    MainWindow().show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
