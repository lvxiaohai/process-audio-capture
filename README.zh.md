# process-audio-capture

[English](./README.md) | **中文**

Node.js 和 Electron 原生插件，支持捕获**指定进程**的音频数据

## 快速开始

### 系统要求

- macOS 14.4+ 
- Windows 10+
- Linux 暂不支持

### 安装

```bash
npm install process-audio-capture
```

## 使用示例

### Electron 应用基本使用

**1. 主进程设置**

```typescript
import { setupAudioCaptureIpc } from "process-audio-capture/dist/main";
// 设置 IPC 处理程序
setupAudioCaptureIpc();
```

**2. Preload 脚本**

```typescript
import { exposeAudioCaptureApi } from "process-audio-capture/dist/preload";
// 暴露 API 给渲染进程
exposeAudioCaptureApi();
```

**3. 渲染进程使用**

```typescript
// 使用 window.processAudioCapture 访问所有 API

// 获取进程列表
const processes = await window.processAudioCapture.getProcessList();

// 监听音频数据
window.processAudioCapture.onAudioData((audioData) => {
  console.log("音频数据:", audioData);
});

// 开始捕获指定进程音频
const pid = processes[0].pid; // 示例：捕获第一个进程
await window.processAudioCapture.startCapture(pid);

```

### Electron 应用自定义实现

```typescript
// 主进程引入 auidioCapture 对象或 AudioCapture 类 
// 自主定义 IPC 处理逻辑
import { audioCapture, AudioCapture } from "process-audio-capture";
```

### Node.js 应用

```javascript
const {audioCapture} = require("process-audio-capture");

// 检查平台支持
if (audioCapture.isPlatformSupported()) {
  // 请求权限
  const permission = await audioCapture.requestPermission();

  if (permission.status === "authorized") {
    // 获取进程列表
    const processes = audioCapture.getProcessList();

    // 开始捕获音频
    audioCapture.startCapture(processes[0].pid, (audioData) => {
      console.log("音频数据:", audioData);
    });
  }
}
````

### 完整示例

- [Node.js 示例](./examples/node/) - 命令行音频捕获工具
- [Electron 示例](./examples/electron/) - 图形界面音频捕获应用

## API 参考

### 核心方法

| 方法                          | 描述                     | 返回值                      |
| ----------------------------- | ------------------------ | --------------------------- |
| `isPlatformSupported()`       | 检查当前平台是否支持     | `boolean`                   |
| `checkPermission()`           | 检查音频捕获权限         | `PermissionStatus`          |
| `requestPermission()`         | 请求音频捕获权限         | `Promise<PermissionStatus>` |
| `getProcessList()`            | 获取可捕获音频的进程列表 | `ProcessInfo[]`             |
| `startCapture(pid, callback)` | 开始捕获指定进程音频     | `boolean`                   |
| `stopCapture()`               | 停止音频捕获             | `boolean`                   |

## 权限配置

### Windows

无需特殊权限配置

### macOS

需要 `NSAudioCaptureUsageDescription` 权限

> **⚠️ 开发注意事项**
>
> - 只有打包后的应用调用 `requestPermission()` 才会弹出权限对话框
> - 开发环境下需要将 IDE 或 `Electron.app` 添加到系统权限中：
>   `系统设置` → `隐私与安全性` → `录屏与系统录音` → `仅系统录音`

## 致谢

感谢以下项目提供的灵感和参考：

- **[AudioCap](https://github.com/insidegui/AudioCap)** - macOS 进程音频捕获工具
- **[Windows ApplicationLoopback](https://github.com/microsoft/Windows-classic-samples/tree/2b94df5730177ec27e726b60017c01c97ef1a8fb/Samples/ApplicationLoopback)** - 微软官方示例

## 相关资源

- [electron-audio-loopback](https://github.com/alectrocute/electron-audio-loopback) - 系统音频环回捕获插件
- [Reddit: MacOS 录制系统音频讨论](https://www.reddit.com/r/electronjs/comments/1d9bjh9/recording_system_audio_on_macos/)

---

## 许可证

MIT License
