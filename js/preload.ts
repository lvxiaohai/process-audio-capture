import { contextBridge, ipcRenderer } from "electron";
import type { AudioData, ProcessAudioCaptureApi } from "./types";
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
  isCapturing: () => ipcRendererInvoke(`${PREFIX}:is-capturing`),
  onCapturing: (callback) => {
    const id = uuid();
    const listener = (_event: Electron.IpcRendererEvent, data: boolean) =>
      callback(data);
    ipcRenderer.send(`${PREFIX}:on-capturing`, id);
    ipcRenderer.on(`${PREFIX}:on-capturing:${id}`, listener);

    return () => {
      ipcRenderer.off(`${PREFIX}:on-capturing:${id}`, listener);
      ipcRenderer.send(`${PREFIX}:off-capturing`, id);
    };
  },
  onAudioData: (callback) => {
    const id = uuid();
    const listener = (_event: Electron.IpcRendererEvent, data: AudioData) =>
      callback(data);
    ipcRenderer.send(`${PREFIX}:on-audio-data`, id);
    ipcRenderer.on(`${PREFIX}:on-audio-data:${id}`, listener);

    return () => {
      ipcRenderer.off(`${PREFIX}:on-audio-data:${id}`, listener);
      ipcRenderer.send(`${PREFIX}:off-audio-data`, id);
    };
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

const uuid = () => {
  return crypto.randomUUID();
};
