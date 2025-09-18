# process-audio-capture

**English** | [中文](./README.zh.md)

Node.js and Electron native plugin for capturing audio from **specific processes**

## Quick Start

### System Requirements

- macOS 14.4+
- Windows 10+
- Linux not supported

### Installation

```bash
npm install process-audio-capture
```

## Usage Examples

### Basic Electron Application Usage

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

// Get process list
const processes = await window.processAudioCapture.getProcessList();

// Listen to audio data
window.processAudioCapture.onAudioData((audioData) => {
  console.log("Audio data:", audioData);
});

// Start capturing audio from specified process
const pid = processes[0].pid; // Example: capture from first process
await window.processAudioCapture.startCapture(pid);
```

### Custom Electron Application Implementation

```typescript
// Import audioCapture object or AudioCapture class in main process
// Implement custom IPC handling logic
import { audioCapture, AudioCapture } from "process-audio-capture";
```

### Node.js Application

```javascript
const { audioCapture } = require("process-audio-capture");

// Check platform support
if (audioCapture.isPlatformSupported()) {
  // Request permission
  const permission = await audioCapture.requestPermission();

  if (permission.status === "authorized") {
    // Get process list
    const processes = audioCapture.getProcessList();

    // Start capturing audio
    audioCapture.startCapture(processes[0].pid, (audioData) => {
      console.log("Audio data:", audioData);
    });
  }
}
```

### Complete Examples

- [Node.js Example](./examples/node/) - Command line audio capture tool
- [Electron Example](./examples/electron/) - GUI audio capture application

## API Reference

### Core Methods

| Method                        | Description                            | Return Value                |
| ----------------------------- | -------------------------------------- | --------------------------- |
| `isPlatformSupported()`       | Check if current platform is supported | `boolean`                   |
| `checkPermission()`           | Check audio capture permission         | `PermissionStatus`          |
| `requestPermission()`         | Request audio capture permission       | `Promise<PermissionStatus>` |
| `getProcessList()`            | Get list of processes with audio       | `ProcessInfo[]`             |
| `startCapture(pid, callback)` | Start capturing audio from process     | `boolean`                   |
| `stopCapture()`               | Stop audio capture                     | `boolean`                   |

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
