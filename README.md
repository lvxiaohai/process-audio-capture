# process-audio-capture

[中文](./README.zh.md) | **English**

Electron native plugin for capturing audio from **specific processes**

## Quick Start

### System Requirements

- macOS 14.4+ / Windows 10+
- Linux not supported

### Installation

```bash
npm install process-audio-capture
```

### Basic Usage (see examples)

**1. Main Process Setup**

```typescript
import { setupAudioCaptureIpc } from "process-audio-capture/dist/main";
// Setup IPC handlers
setupAudioCaptureIpc();
```

**2. Preload Script**

```typescript
import { exposeAudioCaptureApi } from "process-audio-capture/dist/preload";
// Expose API to renderer process
exposeAudioCaptureApi();
```

**3. Renderer Process Usage**

```typescript
// Use window.processAudioCapture to access all APIs
const processes = await window.processAudioCapture.getProcessList();
await window.processAudioCapture.startCapture(pid);
```

### Advanced Usage

```typescript
import { audioCapture } from "process-audio-capture";
// Use directly in main process, implement IPC yourself
```

## API Reference

| Method                  | Description                        | Return Value                |
| ----------------------- | ---------------------------------- | --------------------------- |
| `checkPermission()`     | Check audio capture permission     | `Promise<PermissionStatus>` |
| `requestPermission()`   | Request audio capture permission   | `Promise<PermissionStatus>` |
| `getProcessList()`      | Get list of processes with audio   | `Promise<ProcessInfo[]>`    |
| `startCapture(pid)`     | Start capturing audio from process | `Promise<boolean>`          |
| `stopCapture()`         | Stop audio capture                 | `Promise<boolean>`          |
| `onAudioData(callback)` | Listen to audio data stream        | `void`                      |

## Permission Setup

### Windows

No special permission configuration required

### macOS

Requires `NSAudioCaptureUsageDescription` permission

> **⚠️ Development Notes**
>
> - Only packaged apps will show permission dialog when calling `requestPermission()`
> - In development, add your IDE or `Electron.app` to system permissions:
>   `System Settings` → `Privacy & Security` → `Screen & System Audio Recording` → `System Audio Only`

## Acknowledgments

Thanks to the following projects for inspiration and reference:

- **[AudioCap](https://github.com/insidegui/AudioCap)** - macOS process audio capture tool
- **[Windows ApplicationLoopback](https://github.com/microsoft/Windows-classic-samples/tree/2b94df5730177ec27e726b60017c01c97ef1a8fb/Samples/ApplicationLoopback)** - Microsoft official example

## Related Resources

- [electron-audio-loopback](https://github.com/alectrocute/electron-audio-loopback) - System audio loopback capture plugin
- [Reddit: Recording System Audio on macOS](https://www.reddit.com/r/electronjs/comments/1d9bjh9/recording_system_audio_on_macos/)

---

## License

MIT License
