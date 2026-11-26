AMV Converter（C++ 版，版本号见 GitHub Release 页）
================================

用途：面向 Actions ATJ21xx 系列 MP3/视频播放器（如 RUIZU M7）的 AMV/AVI 转换器。
编码内核为自研比特流编码器，复刻原版工具输出格式（逐位一致由 CI 语料回归保证）。

运行
----
双击 amvconv_gui.exe 即可，免安装（本目录自带 Qt 运行库与 ffmpeg）。
命令行版：amvconv_cli.exe --mode amv|avi --in 输入 --out 输出 --w 宽 --h 高 [--fps 帧率]

页面
----
· 转换：添加/拖入文件 → 选模式（AMV 设备用 / AVI 播放器友好）→ 开始全部，点击转换中行可取消
· 预览：内嵌含声播放器，逐帧预览任意媒体文件（所见即所得的设备显示取向）
· 关于：版本、运行环境、第三方库许可

许可
----
本软件：GPL-3.0（源码 https://github.com/CreatorCSIE/mp3-amv-avi-converter）
随附组件：ffmpeg（GPL 构建，-version 可查）、Qt 6（LGPL-3.0）、Qt-Fluent-Widgets（GPL-3.0）
获取完整对应源码：见上方仓库（含 third_party submodule 与构建说明）。

注意
----
PC 端第三方播放器（VLC 3.x / PotPlayer）对 AMV 容器存在已知解码缺陷，
请用本工具"预览"页校验转换结果。
