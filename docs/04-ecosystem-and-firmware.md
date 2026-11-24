# 逆向记录 04 — ATJ2157 生态仓库与固件获取（2026-11-22）

## 背景确认

RUIZU M7 = **君正 ATJ2157**（ARM Cortex-M4F，GD25Q32 4MB SPI flash，屏 320×240，
USB 枚举 MSC `10d6:1101`）。AMV = **A**ctions **M**edia **V**ideo，原工具即君正 OEM 套件。
本仓库逆向结论与下列社区工作全部互证吻合。

## 仓库 1：zanzipanzi/atj2157-player-toolkit（MIT，核心参照）

- `tools/make_player_avi.py`：任意视频 → 播放器可播的 AVI/AMV（与本项目目标几乎重合，
  作为布局参照实现，我们的差异点：以原工具二进制样本为基准 + Python 包架构）
- `tools/mmm_vp_walker_emu.py`：**用 Unicorn 跑固件自己的 AVI/AMV 头解析器**（MMM_VP.AL）
  → 无需实机即可当我们的 writer 预言机（oracle），重大利好
- `tools/lfi_tool.py`：LFI 固件 validate/extract
- docs/technical-notes.md 第 9 节 = 视频要求（与 docs/03 实测互证：AVI1@342、MJPEG、
  21.68fps/512B 音频块体系）
- releases 有 **预编译只读 SPI dump payload**（spiid/spistat/spiread + SHA256SUMS），免编译器
- 明确不含固件（版权），要求 dump 自己的设备

## 仓库 2：kilicdev/actions_flash（= ilyakurdyukov/actions_flash 同源）

- ADFU 协议客户端 `actions_dump`：MSC→ADFU(`10d6:1101`→`10d6:10d6`) 用 vendor SCSI
  `adfu_reboot` 一键切换，免按键组合
- `payload_arm/`：ATJ2157 ARM 裸机 payload（adfus/nandread/nandhwsc）
- `fwhelper`：LFI repair/scan/unpack（.AP/.DRV/.AL/.STY/CONFIG.BIN，yqhx 可执行头）
- RE_NOTES.md：YQHX overlay 分页模型、syscall ABI、寄存器表
- 注意：ATJ2157 变体用 SPI flash，dump 走 zanzipanzi 的 spiread 路径更成熟

## 固件获取路线（用户正在找）

1. **官方升级包（首选）**：RUIZU M7 说明书自带 Google Drive 链接：
   `https://drive.google.com/file/d/10LoD3nSY8cq2UIkZN8oT3FD-nzbceMye/view`
   （zip 名 "RUIZU M7"，内含 setup 升级工具 + 固件；来源：device.report/manual/9757121）
   - 拿到后先看包内格式：UPGRADE.HEX/.FWU/.upf → Rockbox atjboottool 或 fwhelper 解
   - 解出 MMM_VP.AL（视频解析模块）+ 字体 + 资源 → 提取 JPEG 量化/哈夫曼表（固件固化）
2. **自 dump 兜底**：Linux + actions_dump + adfus.bin + spiread（只读，4MB×2 raw/plain）
3. 参考同芯片生态：TheWirelessPhoenix/a02-os（AGPTEK A02 开源替代系统，官方更新下载+哈希验证流程可借鉴）

## 固件获取进展（2026-11-22 更新）

- M7 官方 Drive 链接已 DMCA takedown；manuals 镜像全指向死链
- **RUIZU 官网 ruizutek.com 有 Firmware Update 页**，但 X02 老链接 404（文件被删）
- **Wayback CDX 命中 2025 存档**（正在下载至 %TEMP%\amvtest\fw\）：
  1. `X02【2157-亚马逊-英文版-蓝牙-带计步】.zip` 60,415,888B —— **文件名自证 ATJ2157**，X02 官方固件（用户手上有 X02！同芯片家族）
  2. `new tool 2.06 version.rar` 31,323,983B —— 官方升级工具（即 a02-os 文档说的"脚本解释器"工具，内含 FuncSpec/AdfuUpgrade 逻辑与 fwsc 闪驱）
  3. `How to use video convert tool.xlsx` 261,252B —— **官方视频转换工具教程**（可能直接描述原工具用法/档位）
- X02 规格：1.8" 屏、视频格式 **128×160 AMV**、8GB+TF——AMV 生态又一实证（128X160 不在原工具 VIDEOSIZE 表？待查表内 160X128 是否即此档的横竖写法）

## 对本项目的行动项

- [x] 官方固件获取 → M7 全部 12 变体 + X02 完整解包（docs/05）
- [x] clone zanzipanzi toolkit → mmm_vp_walker_emu 预言机已用于 writer 校验（docs/06）
- [x] 量化表提取 → T_ORIG_Y/C 提自 AMV_EncDLL VA 0xe348（自然序），设备解码模型
      pixel = IDCT(q×T)/8+128 实机定案；Huffman = 固件内置标准表（docs/06）
- [x] M7 实机 A/B → 三档全过（128x128@30 / 320x240@25 / 160x120@30），
      idx1 快进硬依赖定案，AMV 模式与原版功能对齐（docs/06）
