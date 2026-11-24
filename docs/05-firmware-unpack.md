# 逆向记录 05 — 固件解包（M7 + X02，2026-11-22）

## 工具链（已验证可用）

- `rockbox/utils/atj2137/atjboottool`（WSL Debian 编译，需 `zlib1g-dev`）：`--fwu-decrypt` 完成 FWU v3 ECIES 密钥派生 + ATJ2127 密码/解扰
- `a02-os/tools/flash/fwkey.c`：独立密钥派生（与 atjboottool 互证）
- 解出的明文**不是** AFI 目录，而是 **SQLite 数据库**（ADFU 量产包清单）——`sqlite3` 直接以 ro 模式打开

## .fw 容器结构（ATJ2157 量产包）

```
.fw = FWU v3 信封（magic 1122...75, blk 512, ECIES 块 A@0x400/B@0x200 选择子 0x1EE/0x1FE）
 └ 解密 → SQLite DB（4KB 页）
     ├ 97 张清单表：FWIM(147 文件) / BREC / FWSC / HWSC / NAND_ID / SDK_VER...
     ├ FuncSpec 表：55 段 ADFU 脚本正文（MakeFwImage/AdfuUpgrade/LFI/...——量产打包逻辑全公开）
     └ FileTable 表：180 行 (Keyword, FileName, FileLength, File BLOB) ← 全部固件文件实体
```

## 成果

| 包 | 结果 |
|---|---|
| M7 全部 12 变体 | **完整解出**（FileTable 166~172 文件，FWIM 136~140 模块，全部含 mmm_vp.al）→ `%TEMP%\amvtest\fw\batch\` + 首变体文件级 `%TEMP%\amvtest\fw\m7_files\`。注意：SQLite 头页只覆盖 27MB，**FileTable 数据区延伸到 70MB**——早期"模板盘"结论作废（当时只扫了 DB 头部页） |
| X02 (20241211 US215A) | **完整解出 180 文件** → `%TEMP%\amvtest\fw\x02_files\` |

M7 vs X02 模块 diff：kernel.drv/card.drv/fat32n.drv 等驱动尺寸不同（NAND vs SPI 平台），`mmm_vp.al` M7=24KB / X02=16KB（**M7 版更新更大，优先分析它**），M7 独有 adAAX/adACT/aeACT/boxgame/patch.bin/ef_C4.bin。

## X02 关键模块（同 ATJ2157，SDK=USDK130 20121228_SVN9494）

- `mmm_vp.al` 16KB——AMV/AVI 视频解析器：字符串含 `AVI AMV LIST$(`、`JUNKmovistrhstrfvidsauds`、`video format invalid!`、`not support video!`；YQHX 头（小写魔数@4，ARM 小端）
- `adMP3/adAAC/adFLAC/adAPE/adOGG/adWMA/adWAV.al`——音频解码全家
- `FWDec.al` + `fwupdate.AP`——设备端自升级链（解释了 MAKEFWU=encrypt_fw）
- `UNICODE.FON`/`*.TBL`/`*.sty`/`*.mcg`——字体与 UI 资源
- `kernel.drv` 74KB + KER_TEXT/INIT/DATA

## JPEG 量化/哈夫曼表（未定位）

- 标准表原值/扫描序均无命中；aeMP3 的 `[15,19,22,25...]` 是 MP3 音频量化表（假阳性）
- 推测：mmm_vp.al 内嵌定制/缩放表，需反汇编（下阶段：zanzipanzi `mmm_vp_walker_emu.py` Unicorn 预言机）
- **后续（2026-11-23）**：表实际不在固件 mmm_vp.al——量化表 T_ORIG_Y/C 提取自
  **AMV_EncDLL.dll VA 0xe348**（自然序），Huffman 为固件内置标准表（与 AVI 模式
  原版帧头 DHT 逐字节一致）。见 docs/06 实机标定节。

## 安全边界

解出文件全部留在 %TEMP%，**不入仓库**；docs 只记格式与结论（净室+固件分发规避）。

## 下阶段

1. ~~Unicorn 跑 mmm_vp.al 头解析器 → 校验重写版 writer~~ ✅（walker_emu + 预言机校验，docs/06）
2. ~~反汇编找量化表~~ ✅（在 EncDLL 不在固件，见上节后续）
3. ~~M7 实机 A/B~~ ✅（三档 + idx1 全定案，docs/06）
