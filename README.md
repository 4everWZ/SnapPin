# SnapPin（瞬钉）

一个轻量的 Windows 常驻效率工具：截图 → 标注 → OCR → 贴图（Pin），并支持长截图与录屏。  
目标：**低内存、低 CPU 常驻**，功能不做会员解锁，全功能默认可用。

> 本仓库以 “spec-first” 开发：先 PRD/API/ADR/DESIGN/DEV（docs/），再按接口（Contract）实现。

---

## 平台与定位

- **桌面端：Windows（主力）**
  - 截图/贴图/剪贴板/录屏这类能力强平台绑定，Windows 端优先实现 PixPin 体验。
- **服务器端：Linux（命令行）**
  - 服务器侧不需要截图能力（可用于同步/分发/配置等后续能力）。

> 未来如果需要 macOS，会单独实现 mac 平台层，但当前以效率为王，优先把 Windows 端做好。

---

## 关键用户语义（PixPin 风格）

- **Ctrl+1**：进入截图，完成后**默认把截图图片写入系统剪贴板**  
  → 在任意软件直接 **Ctrl+V** 就能粘贴截图。
- **Ctrl+2**：从系统剪贴板创建 Pin  
  - 剪贴板是图片：直接 Pin  
  - 剪贴板是文本：把文本 **渲染成图片** 再 Pin

**重要：不会抢占系统快捷键**
- 不注册/不拦截全局 `Ctrl+C` / `Ctrl+V`（以及常见编辑快捷键）
- 全局热键只使用 `Ctrl+1/Ctrl+2` 等组合（详见 docs/API/2011）

---

## 技术栈（Windows MVP）

### 语言与构建
- **C++20**
- **CMake**（推荐配合 Ninja）
- 工具链：
  - **MSVC 2022（主开发/主支持）**
  - MinGW64：可保留，但不作为主路径（best-effort）

### Windows 平台技术
- **Win32**：窗口、消息循环、托盘、热键、剪贴板、文件 IO
- **Direct2D / DirectWrite**：Overlay/Toolbar/Pin/Annotate 自绘 UI
- **D3D11 + DXGI**
- **Windows Graphics Capture（WGC, WinRT）**：优先捕获后端
- **DXGI Desktop Duplication**：回退捕获后端
- **WIC**：PNG/JPEG 编码与缩略图
- **Media Foundation**：录屏输出 MP4(H.264)（MVP 先视频无音频）
- **WinRT OCR**：`Windows.Media.Ocr`（本地离线）

---

## 为什么 MSVC 2022 是主路径
本项目深度使用 Win32 + D2D/DWrite + D3D/WinRT/MediaFoundation。  
MSVC 与 Windows SDK/WinRT 的配套、调试与符号支持更成熟，能显著减少踩坑成本。

---

## 目录结构

```text
SnapPin/
├─ docs/            # PRD/ADR/API/DESIGN/DEV（规范与契约）
├─ src/             # app/core/ui/capture/export/...（模块化 targets）
├─ tests/
├─ qa/
└─ cmake/
```

详细见：`docs/DEV/0107-Repo-Layout-and-Build-Plan.md`

## 开发环境（VS Code + MSVC）

------

## 文档导航（从这里开始）

- 功能需求：`docs/PRD/0001-Functional-Requirements.md`
- WBS 与里程碑：`docs/PRD/0003-WBS-and-Milestones.md`
- Action 目录：`docs/API/2008-Action-Catalog.md`
- 错误码：`docs/API/2009-Error-Codes-and-Messages.md`
- 配置：`docs/API/2010-Config-Schema.md`
- 热键：`docs/API/2011-Keybindings-Schema.md`
- Clipboard 合约：`docs/DEV/0102-Clipboard-Contracts.md`
- M1 实现路线图：`docs/DEV/0106-Implementation-Roadmap-M1.md`

------

## 构建说明（占位）

> 工程骨架落地后补充具体命令与 CI。

------

## 许可证（占位）

TBD
