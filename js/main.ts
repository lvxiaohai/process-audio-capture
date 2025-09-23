import { ipcMain } from "electron";
import { audioCapture } from "./core";
import type { AudioData } from "./types";
import { AUDIO_CAPTURE_IPC_PREFIX } from "./shared";

const PREFIX = AUDIO_CAPTURE_IPC_PREFIX;

let registered = false;

/**
 * 设置音频捕获IPC通信
 *
 * 在主进程中注册所有音频捕获相关的IPC处理器
 * 配合 preload.ts 中的 exposeAudioCaptureApi() 使用
 */
export const setupAudioCaptureIpc = () => {
  if (registered) {
    return;
  }
  registered = true;

  ipcMain.handle(`${PREFIX}:check-permission`, () => {
    try {
      return audioCapture.checkPermission();
    } catch (error: any) {
      return error;
    }
  });

  ipcMain.handle(`${PREFIX}:request-permission`, async () => {
    try {
      return audioCapture.requestPermission();
    } catch (error: any) {
      return error;
    }
  });

  ipcMain.handle(`${PREFIX}:get-process-list`, () => {
    try {
      return audioCapture.getProcessList();
    } catch (error: any) {
      return error;
    }
  });

  ipcMain.handle(`${PREFIX}:start-capture`, (_event, pid) => {
    try {
      return audioCapture.startCapture(pid);
    } catch (error: any) {
      return error;
    }
  });

  ipcMain.handle(`${PREFIX}:stop-capture`, () => {
    try {
      return audioCapture.stopCapture();
    } catch (error: any) {
      return error;
    }
  });

  ipcMain.handle(`${PREFIX}:is-capturing`, () => {
    try {
      return audioCapture.isCapturing;
    } catch (error: any) {
      return error;
    }
  });

  listenAudioData();

  listenCapturing();
};

const listenAudioData = () => {
  const eventName = "audio-data";
  const listeners = new Map<string, (audioData: AudioData) => void>();

  ipcMain.on(`${PREFIX}:on-${eventName}`, (event, id) => {
    listeners.set(id, (audioData) => {
      if (!event.sender.isDestroyed()) {
        event.sender.send(`${PREFIX}:on-${eventName}:${id}`, audioData);
      }
    });
  });

  ipcMain.on(`${PREFIX}:off-${eventName}`, (_event, id) => {
    listeners.delete(id);
  });

  ipcMain.on(`${PREFIX}:off-all`, (_event, name) => {
    if (name === eventName || !name) {
      listeners.clear();
    }
  });

  audioCapture.on(eventName, (audioData) => {
    listeners.forEach((listener) => {
      listener(audioData);
    });
  });
};

const listenCapturing = () => {
  const eventName = "capturing";
  const listeners = new Map<string, (capturing: boolean) => void>();

  ipcMain.on(`${PREFIX}:on-${eventName}`, (event, id) => {
    listeners.set(id, (capturing) => {
      if (!event.sender.isDestroyed()) {
        event.sender.send(`${PREFIX}:on-${eventName}:${id}`, capturing);
      }
    });
  });

  ipcMain.on(`${PREFIX}:off-${eventName}`, (_event, id) => {
    listeners.delete(id);
  });

  ipcMain.on(`${PREFIX}:off-all`, (_event, name) => {
    if (name === eventName || !name) {
      listeners.clear();
    }
  });

  audioCapture.on(eventName, (capturing) => {
    listeners.forEach((listener) => {
      listener(capturing);
    });
  });
};
