# Magpie Experimental v0.6.8

## 更新内容

### DLSSNR：Multi Pass

- 新增 `Multi Pass` 下拉框，可选 **1／2／3 次**，默认 **1 次**，位于“NR 界面修正”下方。
- 每一层可独立调整 NR 风格、NR 强度、局部色调强度、局部结构强度、皮肤结构强度、自动遮罩和界面修正。选择 2／3 次后会显示对应参数列；窗口较窄时可横向滚动。
- 新增层使用默认参数；减少次数会保留隐藏层的设置，再次增加时恢复。旧效果组保持单次处理。
- 输入分辨率、细节控制和光流设置由各层共用。多次处理会增加 GPU 耗时与显存占用。

### DLSSNR：抗闪烁

在第二列 `Multi Pass` 下方新增“抗闪烁”，默认 **无**，支持以下互斥选项：

| 选项 | 作用 |
| --- | --- |
| 无 | 关闭额外的抗闪烁处理，保持原有 NR 输出 |
| 静态累积 | 平滑静止区域的修正，画面变化时采用当前结果 |
| 光流累积 | 使用光流对齐历史，平滑运动画面中的修正 |
| 光流累积+ | 在光流累积基础上，进一步缓和修正反复出现、消失造成的闪烁 |
| 低频时域重建 | 主要稳定低频修正，并重建到原输出分辨率，保留当前帧高频细节 |

单次处理时稳定本次 NR 的修正；多次处理时主要稳定后续层新增的修正。后三种路线复用光流设置；光流方法为“无”时，会自动使用 AMD 质量光流进行抗闪烁处理，不改变 NR 自身的光流选择。

抗闪烁会增加处理开销，可能带来拖影或细节响应延迟；低频时域重建仍可能保留高频闪烁，也不会减少 NR 模型的推理成本。可切换为“无”恢复原有输出。更改 Multi Pass 次数或抗闪烁路线需重启缩放生效；旧配置默认关闭抗闪烁。

### Frame Generation

- **统一 XeSS 效果器**：原来的 x2 与 MFG 合并为一个 `XeSS_FrameGeneration`。新建时默认 **2×、AMD 光流、质量**。升级后，旧 XeSS 效果自动转换为统一效果器，倍率设为 2×，保留此前的光流方法与质量选择；已经使用统一效果器的配置保留所选倍率。
- **XeSS 多帧生成**：倍率可选 **2×／3×／4×**；需要时自动启用多帧兼容模式，无需额外勾选。3×／4×支持“无”或 AMD 光流，NVIDIA 光流仅用于 2×。
- **改善 XeSS 3×／4×启动**：修正画面已出现、但帧率迟迟未升上去的问题。
- **可选重复帧过滤**：DLSS FG 与 XeSS FG 的“帧数倍率”和“光流方法”之间新增复选框，默认勾选。勾选时过滤像素完全相同的捕获帧；取消后关闭这项检测，优先于全局重复帧检测设置。选项随效果组保存，运行中修改需重启缩放生效。

### 界面与修正

- 配置文件中的“参数调整焦点切换”从“常规”移到“高级”，已有选择保持不变。
- 修正重复捕获时间戳反复重置抗闪烁历史的问题，用于改善 DLSSNR 截图后的持续闪烁。不同应用与抗闪烁路线的实际效果仍需确认。

## 使用说明

### 从 0.6.7 升级，或首次安装

1. 备份需要保留的设置与截图，并从托盘完全退出 Magpie。
2. 将 `Magpie-Experimental-x64.zip` **完整解压到新目录**，再运行其中的 `Magpie.exe`。不要在压缩包内运行，也不要仅替换 EXE。
3. 普通安装继续使用 `%LOCALAPPDATA%\Magpie\config\v4e\config.json`。便携用户可将原设置复制到新程序目录内的 `config\v4e\config.json`，并保留备份以便回退。

### 已知限制

- RTSS 开启时，反复切换 XeSS FG 倍率或重新启用效果仍可能崩溃。遇到此情况，请暂时退出 RTSS，再重启 Magpie；本版不代表已解决全部 RTSS 兼容性问题。
- 多层 DLSSNR、抗闪烁与 HDR／不同应用的组合仍需实机确认。若画质、性能或稳定性不理想，可先恢复 **Multi Pass 1、抗闪烁“无”**，XeSS FG 恢复 **2×**。

## 附件的作用与使用

| 附件 | 用途与使用方法 |
| --- | --- |
| `Magpie-Experimental-x64.zip` | **必选完整主包**，包含程序、匹配的界面资源、效果与运行组件。完整解压到新目录使用。 |
| `DLSSNR-DLL-Options-310.8.0.0.zip` | 沿用 067 的可选 DLL 包，提供 NVIDIA 官方版、RTX 40/50 社区兼容版与 SF-v2。需要切换 NR DLL 时下载；完全退出 Magpie，备份现有 DLL，按包内中英说明选择一个版本放到 `Magpie.exe` 旁。 |
| `NGX_OTA_Switch.bat` | 沿用 067 的可选工具，用于查看或切换 NGX OTA 设置，普通安装无需运行。相关操作需管理员权限并影响系统级设置；使用 **Restore default** 恢复，删除 BAT 不会撤销设置。 |

---

# Magpie Experimental v0.6.8

## Updates

### DLSSNR: Multi Pass

- Adds a `Multi Pass` selector with **1 / 2 / 3 passes**, defaulting to **1**, below NR UI Correction.
- Each pass has independent NR Style, NR Intensity, Local Tone Strength, Local Structure Strength, Skin Structure Strength, Automatic Mask and UI Correction controls. Selecting 2 or 3 passes reveals the corresponding columns; narrow windows support horizontal scrolling.
- Additional passes start with default settings. Reducing the pass count preserves hidden settings for later reuse. Existing groups retain single-pass processing.
- Input resolution, detail controls and optical-flow settings are shared across passes. Additional passes increase GPU processing time and memory use.

### DLSSNR: Anti-flicker

Adds an Anti-flicker selector below `Multi Pass` in the second column. It defaults to **None**, with these mutually exclusive choices:

| Option | Purpose |
| --- | --- |
| None | Disables additional anti-flicker processing and retains the original NR output |
| Static Accumulation | Smooths corrections in static areas, using the current result when the image changes |
| Optical Flow Accumulation | Aligns history with optical flow to smooth corrections in moving images |
| Optical Flow Accumulation+ | Further reduces flicker caused by corrections repeatedly appearing and disappearing |
| Low-frequency Temporal Reconstruction | Mainly stabilizes low-frequency corrections and reconstructs them at the original output resolution while retaining current-frame high-frequency detail |

With one pass, anti-flicker stabilizes that pass's NR corrections. With multiple passes, it mainly stabilizes corrections added by the later passes. The last three options reuse the optical-flow settings. If the method is None, AMD Quality optical flow is requested for anti-flicker without changing NR's own optical-flow selection.

Anti-flicker adds processing overhead and may introduce ghosting or slower detail response. Low-frequency reconstruction can still retain high-frequency flicker and does not reduce NR model inference cost. Choose None to restore the original output. Changing the pass count or anti-flicker option requires restarting scaling. Anti-flicker defaults to disabled for existing configurations.

### Frame Generation

- **Unified XeSS effect**: The previous x2 and MFG effects become one `XeSS_FrameGeneration` effect. New instances default to **2×, AMD optical flow, Quality**. Existing legacy XeSS effects are converted to the unified effect at 2× while retaining their previous optical-flow method and quality. Configurations already using the unified effect retain their selected multiplier.
- **XeSS multi-frame generation**: Select **2× / 3× / 4×**. Multi-frame compatibility is enabled automatically when needed. 3× and 4× support None or AMD optical flow; NVIDIA optical flow is available at 2× only.
- **Improved XeSS 3× / 4× startup**: Fixes a pacing issue where the scaled image appeared before the frame rate increased.
- **Optional duplicate frame filtering**: DLSS FG and XeSS FG gain a checkbox between Frame Multiplier and Optical Flow Method, enabled by default. Checked filters captured frames with identical pixels; unchecked disables this comparison and overrides the global duplicate-detection setting. The choice is saved with the effect group and requires restarting scaling when changed during a session.

### Interface and Fixes

- Moves Switch focus for parameter adjustment from General to Advanced in profile settings, preserving existing choices.
- Fixes repeated capture timestamps unnecessarily resetting anti-flicker history, addressing persistent DLSSNR flicker after screenshots. Results across applications and anti-flicker options still need confirmation.

## Usage

### Upgrade from 0.6.7 or Install for the First Time

1. Back up any settings and screenshots you want to keep, then fully exit Magpie from the system tray.
2. **Extract the entire** `Magpie-Experimental-x64.zip` **into a new folder**, then run its `Magpie.exe`. Do not run inside the ZIP or replace only the EXE.
3. Normal installations continue using `%LOCALAPPDATA%\Magpie\config\v4e\config.json`. Portable users can copy their existing settings to `config\v4e\config.json` in the new program folder and keep a backup for rollback.

### Known Limitations

- With RTSS running, repeatedly switching XeSS FG multipliers or restarting effects can still crash. If this occurs, temporarily exit RTSS and restart Magpie. This version does not resolve every RTSS compatibility issue.
- Multi-pass DLSSNR, anti-flicker and combinations with HDR or different applications still need real-world verification. If image quality, performance or stability is unsatisfactory, start with **Multi Pass 1, Anti-flicker None**, and **XeSS FG 2×**.

## Assets: Purpose and Instructions

| Asset | Purpose and Instructions |
| --- | --- |
| `Magpie-Experimental-x64.zip` | **Required complete package**, containing the application, matching UI resources, effects and runtime components. Extract it fully into a new folder. |
| `DLSSNR-DLL-Options-310.8.0.0.zip` | Reuses the optional 067 DLL package with official NVIDIA, community RTX 40/50-compatible and SF-v2 variants. Download only to switch NR DLLs. Fully exit Magpie, back up the existing DLL, and follow the Chinese/English instructions to place one variant beside `Magpie.exe`. |
| `NGX_OTA_Switch.bat` | Reuses the optional 067 tool for inspecting or changing NGX OTA settings. Normal installation does not require it. Relevant operations require administrator privileges and affect system-wide settings. Use **Restore default** to undo changes; deleting the BAT does not restore settings. |
