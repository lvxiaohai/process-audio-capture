# Process Audio Capture - Node.js 示例

这是一个使用 `process-audio-capture` 库的 Node.js 示例程序，演示如何在纯 Node.js 环境中捕获进程级音频。

## 安装依赖

在运行示例之前，请先安装依赖：

```bash
# 进入示例目录
cd examples/node

# 安装依赖
npm install
```

## 运行示例

```bash
# 启动示例程序
npm start

# 或者直接运行
node main.js
```

## 示例输出

```
=== Process Audio Capture Node.js 示例 ===

✅ 平台支持音频捕获功能

📋 检查音频捕获权限...
当前权限状态: authorized
✅ 已获得音频捕获权限

📋 获取可捕获音频的进程列表...
找到 3 个可捕获音频的进程:

1. Music (PID: 1234)
   路径: /System/Applications/Music.app/Contents/MacOS/Music
   描述: Music

2. Safari (PID: 5678)
   路径: /Applications/Safari.app/Contents/MacOS/Safari
   描述: Safari

3. Chrome (PID: 9012)
   路径: /Applications/Google Chrome.app/Contents/MacOS/Google Chrome
   描述: Google Chrome

🎯 开始捕获进程 "Music" (PID: 1234) 的音频...

✅ 音频捕获开始成功
📊 实时音频数据显示 (按 Ctrl+C 停止):

🎵 音频数据 #  42 | 通道: 2 | 采样率: 44100Hz | 音量: [████████░░░░░░░░░░░░] 38%
```

## 相关资源

- [Electron 示例](../electron/) - Electron 版本
- [项目主页](../../README.zh.md) - 完整文档
