"""共享播放组件：内嵌含声播放（音频主时钟 + 视频追帧）+ 拖动定位 + 音量。

侧边栏预览页与转换结果对话框共用；画面即 ffmpeg 解码输出 = 设备显示取向
（AMV 解码自带 vflip 补偿），所见即所得。声音走 ffmpeg 解码 s16le PCM →
读线程入有界队列（管道背压天然限速，-re 对 AMV 残缺时间戳会卡死不可用）→
主线程泵入 QAudioSink，不经 QMediaPlayer（其多媒体后端受原版容器 quirk
影响不可用，见 docs/06）；ffplay 外置窗口方案已退役。

同步策略：QTimer 数帧和声卡消费是两条独立时间线，必然漂移——所以以
**音频已消费字节折算的时钟为主**，视频 tick 高频检查、落后则追帧；
无音频流/无声卡时退回 QElapsedTimer 实时钟。队列刻意压小（2×2KB）降低
起播预缓冲带来的固定超前。
"""

from __future__ import annotations

import contextlib
import queue
import subprocess
import threading
from pathlib import Path

from PySide6.QtCore import QElapsedTimer, Qt, QTimer, Signal
from PySide6.QtGui import QImage, QPixmap
from PySide6.QtMultimedia import QAudioFormat, QAudioSink
from PySide6.QtWidgets import QHBoxLayout, QLabel, QVBoxLayout, QWidget
from qfluentwidgets import CaptionLabel, FluentIcon, PushButton, Slider

from ..core.preview import Preview

CHUNK = 2048  # 46ms @22050/16bit/mono


class MediaPlayerWidget(QWidget):
    mediaClosed = Signal()  # 用户主动关闭文件（页面据此清路径显示）

    def __init__(self, path=None, max_w=640, max_h=480, parent=None):
        super().__init__(parent)
        self.max_w, self.max_h = max_w, max_h
        self.pv: Preview | None = None
        self._aproc: subprocess.Popen | None = None
        self._aq: queue.Queue | None = None
        self._astop = threading.Event()
        self._a_rest = b""
        self._a_written = 0
        self._a_start = 0.0
        self._sink: QAudioSink | None = None
        self._aio = None
        self._byte_rate = 44100
        self._elapsed = QElapsedTimer()
        self._t0 = 0.0

        self._vtimer = QTimer(self)   # 高频追帧（15ms）
        self._vtimer.timeout.connect(self._tick)
        self._atimer = QTimer(self)   # PCM 泵（20ms）
        self._atimer.timeout.connect(self._pump_audio)
        self._seek_timer = QTimer(self)  # 点槽跳转的防抖 seek
        self._seek_timer.setSingleShot(True)
        self._seek_timer.timeout.connect(lambda: self._apply_seek(self._click_resume))
        self._dragging = False
        self._was_playing = False
        self._click_resume = False
        self._syncing = False

        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(10)
        self.img = QLabel("未选择文件")
        self.img.setMinimumSize(max_w, max_h)
        self.img.setAlignment(Qt.AlignCenter)
        lay.addWidget(self.img, 1)

        row = QHBoxLayout()
        self.play_btn = PushButton(FluentIcon.PLAY, "播放")
        self.play_btn.clicked.connect(self._toggle)
        self.stop_btn = PushButton(FluentIcon.PAUSE, "停止")
        self.stop_btn.clicked.connect(self._stop)
        self.slider = Slider(Qt.Horizontal)
        self.slider.setRange(0, 1)
        self.slider.sliderPressed.connect(self._begin_drag)
        self.slider.sliderMoved.connect(self._preview_seek)
        self.slider.sliderReleased.connect(self._end_drag)
        self.slider.valueChanged.connect(self._on_value)
        self.time_lbl = CaptionLabel("0:00 / 0:00")
        row.addWidget(self.play_btn)
        row.addWidget(self.stop_btn)
        row.addWidget(self.slider, 1)
        row.addWidget(self.time_lbl)
        row.addWidget(CaptionLabel("音量"))
        self.vol_slider = Slider(Qt.Horizontal)
        self.vol_slider.setRange(0, 100)
        self.vol_slider.setValue(100)
        self.vol_slider.setFixedWidth(110)
        self.vol_slider.valueChanged.connect(
            lambda v: self._sink is not None and self._sink.setVolume(v / 100.0))
        row.addWidget(self.vol_slider)
        self.close_btn = PushButton(FluentIcon.CLOSE, "关闭")
        self.close_btn.clicked.connect(self._close_clicked)
        row.addWidget(self.close_btn)
        lay.addLayout(row)

        self._set_enabled(False)
        if path is not None:
            self.set_path(path)

    # ---------- 对外 ----------

    def set_path(self, path) -> bool:
        """装载文件；失败时在画面区提示原因，返回是否成功。"""
        self.close_media()
        try:
            self.pv = Preview(Path(path))
        except Exception as e:  # noqa: BLE001 —— probe/起流失败都要在页面上说清楚
            self.img.setText(f"无法打开：{e}")
            return False
        self.slider.setRange(0, max(1, int(self.pv.duration * 1000)))
        self._sync_slider(0)
        self.time_lbl.setText("0:00 / " + self._fmt(self.pv.duration))
        self._make_sink()
        self._set_enabled(True)
        self._show(self.pv.read())
        self.pv.seek(0.0)
        return True

    def close_media(self):
        self._vtimer.stop()
        self.play_btn.setText("播放")
        self._stop_audio()
        self._sink = None  # 先于窗口析构释放 WASAPI 客户端
        if self.pv is not None:
            self.pv.close()
            self.pv = None
        self.img.setText("未选择文件")
        self._set_enabled(False)

    # ---------- 内部 ----------

    def _set_enabled(self, on: bool):
        for w in (self.play_btn, self.stop_btn, self.slider, self.vol_slider,
                  self.close_btn):
            w.setEnabled(on)

    # ---------- 音频 ----------

    def _make_sink(self):
        """按文件音频参数建 QAudioSink；无音频流/无声卡则退回实时钟纯画面。"""
        self._sink = None
        if not self.pv or not self.pv.audio_rate:
            return
        fmt = QAudioFormat()
        fmt.setSampleRate(self.pv.audio_rate)
        fmt.setChannelCount(max(1, self.pv.audio_ch))
        fmt.setSampleFormat(QAudioFormat.SampleFormat.Int16)
        sink = QAudioSink(fmt)
        if sink.isNull():
            return
        self._sink = sink
        self._byte_rate = self.pv.audio_rate * max(1, self.pv.audio_ch) * 2

    def _start_audio(self, seconds: float):
        self._stop_audio()
        self._a_start = seconds
        if self._sink is None or self.pv is None:
            self._t0 = seconds
            self._elapsed.start()
            return
        self._astop.clear()
        self._a_rest = b""
        self._a_written = 0
        self._aq = queue.Queue(maxsize=2)
        self._aproc = subprocess.Popen(
            [self.pv.ff, "-hide_banner", "-v", "error",
             "-ss", f"{seconds:.3f}", "-i", str(self.pv.path),
             "-vn", "-f", "s16le", "-acodec", "pcm_s16le", "pipe:1"],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

        def reader(proc=self._aproc, q=self._aq):  # 闭包捕获，避免停止时 _aproc 置 None 竞态
            while not self._astop.is_set():
                try:
                    chunk = proc.stdout.read(CHUNK)
                except Exception:  # noqa: BLE001 —— stdout.close() 打断读
                    break
                if not chunk:
                    break
                while not self._astop.is_set():
                    try:
                        q.put(chunk, timeout=0.1)
                        break
                    except queue.Full:
                        pass
            with contextlib.suppress(Exception):
                q.put(None, timeout=0.2)  # EOF 哨兵

        threading.Thread(target=reader, daemon=True).start()
        self._sink.setVolume(self.vol_slider.value() / 100.0)
        self._aio = self._sink.start()
        self._atimer.start(20)

    def _pump_audio(self):
        if self._aio is None or self._sink is None:
            return
        free = self._sink.bytesFree()
        while free >= 512:
            if not self._a_rest:
                try:
                    chunk = self._aq.get_nowait()
                except queue.Empty:
                    return
                if chunk is None:  # 音频流尾：停泵（视频追帧处统一重启）
                    self._atimer.stop()
                    return
                self._a_rest = chunk
            n = min(free, len(self._a_rest) - len(self._a_rest) % 2)
            self._aio.write(self._a_rest[:n])
            self._a_written += n
            self._a_rest = self._a_rest[n:]
            free -= n

    def _stop_audio(self):
        self._atimer.stop()
        if self._sink is not None:
            self._sink.stop()
        self._aio = None
        self._astop.set()
        if self._aproc is not None:
            with contextlib.suppress(Exception):
                self._aproc.stdout.close()
            if self._aproc.poll() is None:
                self._aproc.terminate()
            with contextlib.suppress(subprocess.TimeoutExpired):
                self._aproc.wait(timeout=1)
            self._aproc = None

    def _clock(self) -> float:
        """主时钟：音频已消费字节折算（扣除声卡缓冲占用）；无音频走实时钟。"""
        if self._aproc is None:
            if self._sink is not None:
                return self._a_start  # 暂停：冻结在本段起点
            if not self._elapsed.isValid() or not self._elapsed.hasStarted():
                return self._t0
            return self._t0 + self._elapsed.elapsed() / 1000.0
        buffered = max(0, self._sink.bufferSize() - self._sink.bytesFree())
        consumed = max(0, self._a_written - buffered)
        return self._a_start + consumed / self._byte_rate

    # ---------- 视频 ----------

    def hideEvent(self, e):
        self._vtimer.stop()
        self._stop_audio()
        self._dragging = False
        self.play_btn.setText("播放")
        super().hideEvent(e)

    def _fmt(self, s: float) -> str:
        s = max(0, int(s))
        return f"{s // 60}:{s % 60:02d}"

    def _toggle(self):
        if self._vtimer.isActive():
            self._vtimer.stop()
            self._stop_audio()
            self.play_btn.setText("播放")
        else:
            self.play_btn.setText("暂停")
            self._start_audio(self.pv.position)
            self._vtimer.start(15)

    def _tick(self):
        clock = self._clock()
        for _ in range(4):  # 落后追帧，单 tick 最多补 4 帧
            if self.pv.position > clock + 0.02:
                break
            frame = self.pv.read()
            if frame is None:  # 流尾：整点重来
                self.pv.seek(0.0)
                self._start_audio(0.0)
                return
            self._show(frame)
        if not self._seek_timer.isActive():  # 跳转挂起时不覆写滑条
            self._sync_slider(int(self.pv.position * 1000))
        self.time_lbl.setText(self._fmt(self.pv.position) + " / " + self._fmt(self.pv.duration))

    # ---------- 拖动/跳转 ----------

    def _begin_drag(self):
        self._dragging = True
        self._was_playing = self._vtimer.isActive()
        self._vtimer.stop()
        self._stop_audio()
        self.play_btn.setText("播放")

    def _preview_seek(self, ms: int):
        if self.pv is not None:
            self.time_lbl.setText(self._fmt(ms / 1000.0) + " / " + self._fmt(self.pv.duration))

    def _end_drag(self):
        self._dragging = False
        self._apply_seek(self._was_playing)

    def _sync_slider(self, ms: int):
        # 不能用 blockSignals：qfluentwidgets 的圆点位置由 valueChanged 驱动
        self._syncing = True
        try:
            self.slider.setValue(ms)
        finally:
            self._syncing = False

    def _on_value(self, _ms: int):
        if self._syncing or self._dragging or self.pv is None:
            return
        self._click_resume = self._vtimer.isActive()
        self._seek_timer.start(150)  # 点槽/键盘跳转：防抖后 seek

    def _apply_seek(self, resume: bool):
        """真正 seek：重启视频/音频解码流到目标位置，可选续播。"""
        if self.pv is None:
            return
        self._seek_timer.stop()
        self._vtimer.stop()
        self._stop_audio()
        self.pv.seek(self.slider.value() / 1000.0)
        self._show(self.pv.read())
        self._sync_slider(int(self.pv.position * 1000))
        self.time_lbl.setText(self._fmt(self.pv.position) + " / " + self._fmt(self.pv.duration))
        if resume:
            self.play_btn.setText("暂停")
            self._start_audio(self.pv.position)
            self._vtimer.start(15)

    def _stop(self):
        """停止并回到开头（解码进程保留）。"""
        if self.pv is None:
            return
        self._sync_slider(0)
        self._apply_seek(False)

    def _close_clicked(self):
        self.close_media()
        self.mediaClosed.emit()

    def _show(self, frame):
        if frame is None:
            return
        h, w, _ = frame.shape
        qimg = QImage(frame.data, w, h, 3 * w, QImage.Format.Format_RGB888).copy()
        pix = QPixmap.fromImage(qimg)
        if pix.width() > self.max_w or pix.height() > self.max_h:
            pix = pix.scaled(self.max_w, self.max_h, Qt.KeepAspectRatio, Qt.SmoothTransformation)
        self.img.setPixmap(pix)
