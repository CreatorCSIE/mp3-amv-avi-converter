"""关于页（内容格式借鉴 ClassIsland：版本可复制 → 项目简介 → 运行环境 →
第三方组件列表 → 致谢 → 设备兼容 → 免责声明；许可证逐字实查不虚构）。
"""

from __future__ import annotations

import platform
import subprocess
import sys

from PySide6.QtCore import Qt, QUrl
from PySide6.QtGui import QGuiApplication, QDesktopServices
from PySide6.QtWidgets import QVBoxLayout, QWidget

from qfluentwidgets import (
    BodyLabel, CaptionLabel, CardWidget, FluentIcon, PushButton,
    SingleDirectionScrollArea, StrongBodyLabel, SubtitleLabel,
)

REPO_URL = "https://github.com/CreatorCSIE/mp3-amv-avi-converter"

# 许可证均经实查（各上游 LICENSE 原文），不虚构
REFERENCES = [
    ("FFmpeg", "解封装/缩放/donor 容器（外部进程调用）", "LGPL-3.0 / GPL-2.0（按构建）",
     "https://github.com/FFmpeg/FFmpeg"),
    ("PySide6", "GUI 框架", "LGPL-3.0", "https://www.qt.io/qt-for-python"),
    ("PySide6-Fluent-Widgets", "Fluent Design 组件库", "GPL-3.0",
     "https://github.com/zhiyiYo/PySide6-Fluent-Widgets"),
    ("NumPy", "DCT/量化/熵编码矩阵运算", "BSD-3-Clause",
     "https://github.com/numpy/numpy"),
    ("atj2157-toolkit (zanzipanzi)", "ATJ2157 固件解析与 mmm_vp walker 预言机", "MIT",
     "https://github.com/zanzipanzi/atj2157-toolkit"),
    ("FFmpeg 1.0 (avcodec-54)", "原版工具内置编码器——比特流行为对照基准（仅观察，不随附）",
     "LGPL-2.0+", "https://github.com/FFmpeg/FFmpeg"),
]


def _runtime_lines() -> list[str]:
    from amvconv.core import amvenc
    try:
        ff = subprocess.run(["ffmpeg", "-version"], capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=10)
        ff_line = ff.stdout.splitlines()[0] if ff.stdout else "未找到"
    except OSError:
        ff_line = "未找到"
    return [
        f"Python {platform.python_version()} · {platform.system()} {platform.release()}",
        f"PySide6 {getattr(__import__('PySide6'), '__version__', '?')}",
        ff_line,
        f"熵编码 C 核：{'已加载' if amvenc._FAST is not None else '未加载（纯 Python 回退）'}",
    ]


class AboutInterface(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("aboutInterface")
        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        scroll = SingleDirectionScrollArea(self, orient=Qt.Vertical)
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("QScrollArea{background:transparent;border:none}")
        content = QWidget()
        content.setObjectName("aboutContent")
        content.setStyleSheet("#aboutContent{background:transparent}")
        scroll.setWidget(content)
        outer.addWidget(scroll)
        lay = QVBoxLayout(content)
        lay.setContentsMargins(36, 20, 36, 32)
        lay.setSpacing(14)
        lay.addWidget(SubtitleLabel("关于"))

        # 头区：名称 + 版本（点击复制）+ 定位
        head = CardWidget()
        hl = QVBoxLayout(head)
        hl.addWidget(BodyLabel("<h2 style='margin:0'>AMV Converter</h2>"))
        try:
            from importlib.metadata import version
            ver = version("amvconv")
        except Exception:
            ver = "0.0.1"
        vb = PushButton(FluentIcon.COPY, f"v{ver}")
        vb.clicked.connect(lambda: (QGuiApplication.clipboard().setText(f"v{ver}"),
                                    vb.setText(f"v{ver} 已复制")))
        hl.addWidget(vb, 0, Qt.AlignLeft)
        hl.addWidget(CaptionLabel(
            "面向 Actions ATJ21xx 系列播放器的 AMV/AVI 转换器：自研比特流编码器复刻"
            "原版工具输出格式（逆向 + 实机标定），非简单 FFmpeg 包装。"))
        rb = PushButton(FluentIcon.GITHUB, "GitHub 仓库")
        rb.clicked.connect(lambda: QDesktopServices.openUrl(QUrl(REPO_URL)))
        hl.addWidget(rb, 0, Qt.AlignLeft)
        lay.addWidget(head)

        # 运行环境
        env = CardWidget()
        el = QVBoxLayout(env)
        el.addWidget(StrongBodyLabel("运行环境"))
        for line in _runtime_lines():
            el.addWidget(CaptionLabel(line))
        lay.addWidget(env)

        # 第三方组件
        refs = CardWidget()
        rl = QVBoxLayout(refs)
        rl.addWidget(StrongBodyLabel("本项目使用的第三方库和框架"))
        for name, desc, lic, url in REFERENCES:
            rl.addWidget(BodyLabel(f"<b>{name}</b>（{lic}）<br>"
                                   f"<span style='color:gray'>{desc}</span>"))
            b = PushButton(FluentIcon.LINK, "查看仓库")
            b.clicked.connect(lambda _=False, u=url: QDesktopServices.openUrl(QUrl(u)))
            rl.addWidget(b, 0, Qt.AlignLeft)
        lay.addWidget(refs)

        # 致谢
        thanks = CardWidget()
        tl = QVBoxLayout(thanks)
        tl.addWidget(StrongBodyLabel("致谢"))
        tl.addWidget(CaptionLabel(
            "· 《媒体播放器管理工具 4.46》及其内置 AMV_EncDLL/AMV_DecDLL 的原作者——"
            "格式逆向的行为参照（专有软件，本项目不随附其任何代码与资源）；<br>"
            "· RUIZU M7（ATJ2157）实机——全部比特流结论的验证平台；<br>"
            "· ClassIsland 与 Attic 启动器——关于页内容格式参照。"))
        lic_btn = PushButton(FluentIcon.LINK, "查看本项目 LICENSE（GPL-3.0）")
        lic_btn.clicked.connect(lambda: QDesktopServices.openUrl(QUrl(REPO_URL + "/blob/HEAD/LICENSE")))
        tl.addWidget(lic_btn, 0, Qt.AlignLeft)
        lay.addWidget(thanks)

        # 设备兼容
        dev = CardWidget()
        dl = QVBoxLayout(dev)
        dl.addWidget(StrongBodyLabel("设备兼容（实机验证）"))
        dl.addWidget(CaptionLabel(
            "RUIZU M7：128x128@30 / 160x120@30 / 320x240@25 播放与快进全过；"
            "AMV 快进依赖 idx1（已按原版布局写入）。<br>"
            "PC 预览建议 ffplay/本工具预览窗——VLC 3.x 与 PotPlayer 对 AMV 容器存在已知缺陷"
            "（详见仓库 docs/06）。"))
        lay.addWidget(dev)

        # 免责声明
        disc = CardWidget()
        dsl = QVBoxLayout(disc)
        dsl.addWidget(StrongBodyLabel("免责声明"))
        dsl.addWidget(CaptionLabel(
            "本软件按“现状”提供（GPL-3.0），不提供任何明示或暗示的保证；转换与分发"
            "内容的版权责任由使用者承担；对其他 ATJ 型号/固件版本的兼容性未作承诺。"))
        lay.addWidget(disc)
        lay.addStretch(1)
