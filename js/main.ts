import { ipcMain } from "electron";
import { audioCapture } from "./core";
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

  ipcMain.handle(`${PREFIX}:start-capture`, (event, pid) => {
    try {
      return audioCapture.startCapture(pid, (audioData) => {
        event.sender.send(`${PREFIX}:audio-data`, audioData);
      });
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
};
