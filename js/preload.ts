import { contextBridge, ipcRenderer } from "electron";
import type { ProcessAudioCaptureApi } from "./types";
import { AUDIO_CAPTURE_IPC_PREFIX } from "./shared";

const PREFIX = AUDIO_CAPTURE_IPC_PREFIX;

/**
 * 定义暴露给渲染进程的API
 *
 * 可以通过 exposeAudioCaptureApi() 暴露给渲染进程
 *
 * 也可以通过 contextBridge.exposeInMainWorld() 暴露给渲染进程
 *
 * 配合 main.ts 中的 setupAudioCaptureIpc() 使用
 */
export const processAudioCapture: ProcessAudioCaptureApi = {
  checkPermission: () => ipcRendererInvoke(`${PREFIX}:check-permission`),
  requestPermission: () => ipcRendererInvoke(`${PREFIX}:request-permission`),
  getProcessList: () => ipcRendererInvoke(`${PREFIX}:get-process-list`),
  startCapture: (pid) => ipcRendererInvoke(`${PREFIX}:start-capture`, pid),
  stopCapture: () => ipcRendererInvoke(`${PREFIX}:stop-capture`),
  onAudioData: (callback) => {
    ipcRenderer.on(`${PREFIX}:audio-data`, (_event, data) => callback(data));
  },
};

/**
 * 暴露API给渲染进程
 */
export const exposeAudioCaptureApi = () => {
  if (process.contextIsolated) {
    contextBridge.exposeInMainWorld("processAudioCapture", processAudioCapture);
  } else {
    window.processAudioCapture = processAudioCapture;
  }
};

/**
 * 通用的ipcRenderer.invoke方法
 * 能自动解包主线程返回的错误并重新抛出
 * @param channel
 * @param args
 */
const ipcRendererInvoke: <T>(
  channel: string,
  ...args: unknown[]
) => Promise<T> = async (channel, ...args) => {
  const result = await ipcRenderer.invoke(channel, ...args);
  if (isError(result)) {
    throw result;
  } else {
    return result;
  }
};

/**
 * 判断main进程返回的结果是否是个错误
 * @param err
 */
const isError = (err: any): err is Error => {
  return (
    err instanceof Error ||
    (err && typeof err.message === "string" && typeof err.stack === "string")
  );
};
