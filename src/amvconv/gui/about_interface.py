"""关于页（内容结构借鉴 Attic 启动器 SettingsAbout）：

头区（名称+版本+版权+仓库按钮）→ 法律声明 → 引用项目（许可证逐字实查，不虚构）→
设备兼容说明。
"""

from __future__ import annotations

from PySide6.QtCore import Qt, QUrl
from PySide6.QtGui import QDesktopServices
from PySide6.QtWidgets import QVBoxLayout, QWidget

from qfluentwidgets import (
    CaptionLabel, CardWidget, FluentIcon, PrimaryPushButton, PushButton,
    StrongBodyLabel, SubtitleLabel,
)

REPO_URL = "https://github.com/CreatorCSIE/mp3-amv-avi-converter"

# 许可证均经实查（各上游仓库 LICENSE 原文），不虚构
REFERENCES = [
    ("FFmpeg", "解封装/缩放/donor 容器（外部进程调用）", "LGPL-3.0 / GPL-2.0（按构建）",
     "https://github.com/FFmpeg/FFmpeg"),
    ("PySide6", "GUI 框架", "LGPL-3.0",
     "https://www.qt.io/qt-for-python"),
    ("PySide6-Fluent-Widgets", "Fluent Design 组件库", "GPL-3.0",
     "https://github.com/zhiyiYo/PySide6-Fluent-Widgets"),
    ("NumPy", "DCT/量化/熵编码矩阵运算", "BSD-3-Clause",
     "https://github.com/numpy/numpy"),
    ("atj2157-toolkit (zanzipanzi)", "ATJ2157 固件解析与 mmm_vp walker 预言机", "MIT",
     "https://github.com/zanzipanzi/atj2157-toolkit"),
    ("媒体播放器管理工具 4.46", "逆向参照对象——仅比特流行为对照与实机标定，不随附任何代码/资源",
     "专有软件", None),
]


class AboutInterface(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("aboutInterface")
        lay = QVBoxLayout(self)
        lay.setContentsMargins(36, 20, 36, 20)
        lay.setSpacing(16)

        head = CardWidget()
        hl = QVBoxLayout(head)
        hl.addWidget(SubtitleLabel("AMV Converter"))
        try:
            from importlib.metadata import version
            ver = version("amvconv")
        except Exception:
            ver = "0.0.1"
        hl.addWidget(CaptionLabel(f"v{ver} · 面向 Actions ATJ21xx 系列 MP4/AMV 播放器的"
                                  f"自研比特流转换器"))
        hl.addWidget(CaptionLabel("© 2026 CreatorCSIE"))
        repo_btn = PushButton(FluentIcon.GITHUB, "GitHub 仓库")
        repo_btn.clicked.connect(lambda: QDesktopServices.openUrl(QUrl(REPO_URL)))
        hl.addWidget(repo_btn, 0, Qt.AlignLeft)
        lay.addWidget(head)

        legal = CardWidget()
        ll = QVBoxLayout(legal)
        ll.addWidget(StrongBodyLabel("开源协议"))
        ll.addWidget(CaptionLabel("本项目以 GPL-3.0-only 发布；LICENSE 见仓库根目录或"
                                  " GitHub 仓库。"))
        lic_btn = PushButton(FluentIcon.LINK, "查看 LICENSE")
        lic_btn.clicked.connect(lambda: QDesktopServices.openUrl(QUrl(REPO_URL + "/blob/HEAD/LICENSE")))
        ll.addWidget(lic_btn, 0, Qt.AlignLeft)
        lay.addWidget(legal)

        refs = CardWidget()
        rl = QVBoxLayout(refs)
        rl.addWidget(StrongBodyLabel("引用的项目"))
        for name, desc, lic, url in REFERENCES:
            rl.addWidget(CaptionLabel(f"<b>{name}</b>（{lic}） — {desc}"))
            if url:
                b = PushButton(FluentIcon.LINK, name)
                b.setFixedWidth(240)
                b.clicked.connect(lambda _=False, u=url: QDesktopServices.openUrl(QUrl(u)))
                rl.addWidget(b, 0, Qt.AlignLeft)

        dev = CardWidget()
        dl = QVBoxLayout(dev)
        dl.addWidget(StrongBodyLabel("设备兼容（实机验证）"))
        dl.addWidget(CaptionLabel(
            "RUIZU M7（Actions ATJ2157）：128x128@30 / 160x120@30 / 320x240@25 全过；"
            "AMV 快进依赖 idx1（本工具已按原版布局写入）。"
            "PC 预览请用 ffplay 或本工具预览窗——VLC/PotPlayer 对 AMV 容器存在已知缺陷。"))
        lay.addWidget(dev)
        lay.addStretch(1)
