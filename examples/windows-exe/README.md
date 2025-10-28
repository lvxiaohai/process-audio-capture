# Process Audio Capture - Windows EXE 示例

这是一个简单的 Windows 可执行文件示例，展示了如何使用 `process-audio-capture` 库创建独立的 Windows 应用程序来捕获指定进程的音频。

## 功能特点

- ✅ 独立的 Windows EXE 可执行文件，无需安装 Node.js
- 🎯 选择性捕获指定进程的音频
- 📊 实时显示音频音量
- 💾 自动保存为 WAV 格式
- 🔐 自动处理权限请求
- 🎨 友好的命令行界面

## 系统要求

- Windows 10 或更高版本
- 无需安装 Node.js（打包后的 EXE 自带运行时）

## 使用方法

### 方法一：直接运行 EXE（推荐）

1. 下载或构建 `AudioCapture.exe`
2. 双击运行 `AudioCapture.exe`
3. 按照提示操作：
   - 授予音频捕获权限（首次运行时）
   - 从列表中选择要捕获的进程
   - 程序开始录制，按 `Ctrl+C` 停止

### 方法二：从源码运行（开发模式）

#### 1. 安装依赖

```bash
npm install
# 或
yarn install
```

#### 2. 运行应用

```bash
npm start
# 或
node main.js
```

## 构建 EXE 文件

### 前置要求

- 安装 Node.js（v18 或更高版本）
- 安装依赖包

### 构建步骤

#### 1. 安装依赖

```bash
npm install
```

#### 2. 构建 64 位 EXE

```bash
npm run build
```

生成的文件：`dist/AudioCapture.exe`

#### 3. 构建多个版本（可选）

```bash
npm run build-all
```

这会生成：

- `dist/process-audio-capture-windows-exe-win-x64.exe` (64 位版本)
- `dist/process-audio-capture-windows-exe-win-x86.exe` (32 位版本)

### 构建配置说明

`package.json` 中的 `pkg` 配置：

```json
{
  "pkg": {
    "assets": ["node_modules/process-audio-capture/**/*"],
    "targets": ["node18-win-x64"],
    "outputPath": "dist"
  }
}
```

- **assets**: 包含原生模块文件
- **targets**: 目标平台（`node18-win-x64` 表示 Node.js 18 + Windows 64 位）
- **outputPath**: 输出目录

## 权限设置

### Windows 权限

首次运行时，程序会请求音频捕获权限。如果遇到权限问题：

1. 打开 **Windows 设置**
2. 进入 **隐私** → **麦克风**
3. 确保 "**允许桌面应用访问麦克风**" 已开启

## 使用示例

### 运行示例

```bash
C:\> AudioCapture.exe

=== Process Audio Capture Windows EXE 示例 ===

✅ 平台支持音频捕获功能

📋 检查音频捕获权限...
当前权限状态: authorized
✅ 已获得音频捕获权限

📋 获取可捕获音频的进程列表...

找到 3 个可捕获音频的进程:

1. chrome.exe (PID: 12345)
   路径: C:\Program Files\Google\Chrome\Application\chrome.exe

2. Spotify.exe (PID: 67890)
   路径: C:\Users\Username\AppData\Roaming\Spotify\Spotify.exe

3. vlc.exe (PID: 11223)
   路径: C:\Program Files\VideoLAN\VLC\vlc.exe

请输入要捕获的进程编号 (1-3): 2

🎯 开始捕获进程 "Spotify.exe" (PID: 67890) 的音频...

✅ 音频捕获开始成功
📊 实时音频数据显示:
💡 按 Ctrl+C 停止录制

🎵 录制中... | 音量: [████████████░░░░░░░░] 60% | 数据包: 142
```

### 输出文件

录制的音频文件会保存在当前目录，命名格式：

```
capture_[PID]_[时间戳].wav
```

例如：`capture_67890_1698765432123.wav`

## 常见问题

### 1. 找不到可捕获的进程

**原因**：没有应用程序正在播放音频

**解决方法**：

- 打开一个正在播放音频的应用（如浏览器播放视频、音乐播放器等）
- 确保该应用确实在输出音频

### 2. 权限被拒绝

**原因**：Windows 系统未授予音频捕获权限

**解决方法**：

1. 打开 Windows 设置
2. 隐私 → 麦克风
3. 启用 "允许桌面应用访问麦克风"

### 3. EXE 文件太大

**原因**：pkg 打包会包含整个 Node.js 运行时（约 50MB）

**说明**：这是正常的，因为要创建独立可执行文件需要内嵌 Node.js 运行时

**优化建议**：

- 使用 UPX 压缩：`upx --best AudioCapture.exe`（可减小约 50%）
- 考虑使用其他打包工具，如 nexe

### 4. 无法捕获某些应用的音频

**原因**：

- 应用使用了受保护的音频流（如某些 DRM 保护的内容）
- 应用可能在系统混音器中被静音
- 应用本身没有音频输出

**解决方法**：

- 检查 Windows 音量混合器中该应用是否有音频输出
- 尝试其他应用进行测试

## 技术说明

### 打包工具

使用 [pkg](https://github.com/vercel/pkg) 将 Node.js 应用打包成独立的可执行文件。

**优点**：

- ✅ 用户无需安装 Node.js
- ✅ 单文件分发
- ✅ 支持原生模块

**注意事项**：

- 📦 包含原生 Node.js 插件需要配置 `assets`
- 🔧 需要指定正确的 Node.js 版本目标

### 原生模块处理

`process-audio-capture` 是一个原生 Node.js 插件，打包时需要：

1. 在 `pkg.assets` 中包含原生模块文件
2. 确保 Node.js 版本匹配（本示例使用 Node 18）
3. 目标平台需要正确（win-x64 或 win-x86）

## 项目结构

```
windows-exe/
├── main.js              # 主应用程序代码
├── package.json         # npm 配置和 pkg 打包配置
├── README.md           # 本文档
└── dist/               # 构建输出目录
    └── AudioCapture.exe # 最终生成的可执行文件
```

## 相关链接

- [process-audio-capture](https://github.com/lvxiaohai/process-audio-capture) - 主项目
- [pkg](https://github.com/vercel/pkg) - Node.js 打包工具
- [wav](https://www.npmjs.com/package/wav) - WAV 文件处理库

## 许可证

MIT License

---

## 开发者提示

### 调试

在开发模式下运行以查看详细错误信息：

```bash
node main.js
```

### 测试原生模块

确保原生模块正确编译：

```bash
cd node_modules/process-audio-capture
npm run build
```

### 自定义构建目标

修改 `package.json` 中的 `pkg.targets` 来构建不同平台：

```json
{
  "pkg": {
    "targets": [
      "node18-win-x64", // Windows 64位
      "node18-win-x86" // Windows 32位
    ]
  }
}
```
