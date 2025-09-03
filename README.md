# electron-audio-capture

Electron 原生插件示例，提供捕获指定进程音频的功能。仅支持 macOS 平台。

## 功能特点

- 获取系统中可捕获音频的进程列表
- 捕获指定进程的音频输出
- 实时回调 PCM 音频数据
- 专为 macOS 平台设计

## 项目结构

```
electron-audio-capture/
├── include/                # 头文件目录
│   ├── audio_capture.h     # 通用接口定义
│   └── mac/                # macOS 平台头文件
├── src/                    # 源代码目录
│   ├── addon_main.cc       # 插件主入口
│   └── mac/                # macOS 平台实现
└── js/                     # JavaScript 包装层
    ├── index.js            # JS 接口
    └── index.d.ts          # TypeScript 类型定义
```

## 作为本地依赖使用

### 方法 1：使用 npm link

在此库目录中执行：

```bash
# 在electron-audio-capture目录中
npm link
```

然后在你的项目中执行：

```bash
# 在你的项目目录中
npm link electron-audio-capture
```

### 方法 2：直接在 package.json 中引用本地路径

在你的项目的 package.json 中添加：

```json
"dependencies": {
  "electron-audio-capture": "file:/path/to/electron-audio-capture"
}
```

## 在项目中使用

### JavaScript 中使用

```javascript
// 引入插件
const audioCapture = require("electron-audio-capture");

// 初始化
audioCapture.initialize();

// 获取进程列表
const processes = audioCapture.getProcessList();
console.log("可捕获的进程列表:", processes);

// 选择一个进程并开始捕获
const targetPid = processes[0].pid;
audioCapture.startCapture(targetPid, (audioData) => {
  // audioData包含PCM数据和格式信息
  console.log(`收到音频数据: ${audioData.data.length} 字节, 
               ${audioData.channels} 通道, 
               ${audioData.sampleRate} Hz`);

  // 处理音频数据...
});

// 停止捕获
setTimeout(() => {
  audioCapture.stopCapture();
}, 10000);

// 测试方法
const result = audioCapture.helloWorld("测试消息");
console.log(result);
```

### TypeScript 中使用

```typescript
// 引入插件
import audioCapture, { ProcessInfo, AudioData } from "electron-audio-capture";

// 初始化
audioCapture.initialize();

// 获取进程列表
const processes: ProcessInfo[] = audioCapture.getProcessList();
console.log("可捕获的进程列表:", processes);

// 选择一个进程并开始捕获
const targetPid: number = processes[0].pid;
audioCapture.startCapture(targetPid, (audioData: AudioData) => {
  // audioData包含PCM数据和格式信息
  console.log(`收到音频数据: ${audioData.data.length} 字节, 
               ${audioData.channels} 通道, 
               ${audioData.sampleRate} Hz`);

  // 处理音频数据...
});

// 停止捕获
setTimeout(() => {
  audioCapture.stopCapture();
}, 10000);

// 测试方法
const result: string = audioCapture.helloWorld("TypeScript测试");
console.log(result);
```

## 事件监听

```javascript
// 监听音频数据事件
audioCapture.on("audio-data", (audioData) => {
  console.log("收到音频数据事件");
});

// 监听捕获开始事件
audioCapture.on("capture-started", (info) => {
  console.log(`开始捕获进程 ${info.pid} 的音频`);
});

// 监听捕获停止事件
audioCapture.on("capture-stopped", () => {
  console.log("音频捕获已停止");
});
```

## 重新构建

如果你的 Electron 版本与构建此插件时的 Node.js 版本不同，你可能需要重新构建插件：

```bash
# 安装electron-rebuild
npm install --save-dev electron-rebuild

# 重新构建原生模块
npx electron-rebuild
```

## 注意事项

- 仅支持 macOS 平台
- 确保你的 Electron 版本与此插件兼容
- 在主进程中使用此插件，而不是渲染进程
- 如需在渲染进程中使用，请通过 IPC 与主进程通信
- 捕获其他应用的音频可能需要特定的系统权限
