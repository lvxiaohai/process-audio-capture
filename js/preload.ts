import { contextBridge, ipcRenderer } from "electron";
import type {
  ProcessAudioCaptureApi,
  Unsubscribe,
  AudioCaptureEvents,
} from "./types";
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
  isPlatformSupported: () =>
    ipcRendererInvoke(`${PREFIX}:is-platform-supported`),
  checkPermission: () => ipcRendererInvoke(`${PREFIX}:check-permission`),
  requestPermission: () => ipcRendererInvoke(`${PREFIX}:request-permission`),
  getProcessList: () => ipcRendererInvoke(`${PREFIX}:get-process-list`),
  startCapture: (pid) => ipcRendererInvoke(`${PREFIX}:start-capture`, pid),
  stopCapture: () => ipcRendererInvoke(`${PREFIX}:stop-capture`),
  isCapturing: () => ipcRendererInvoke(`${PREFIX}:is-capturing`),
  on: <K extends keyof AudioCaptureEvents>(
    eventName: K,
    callback: (...args: AudioCaptureEvents[K]) => void
  ): Unsubscribe => {
    const id = uuid();
    const listener = (
      _event: Electron.IpcRendererEvent,
      ...args: AudioCaptureEvents[K]
    ) => callback(...args);
    ipcRenderer.send(`${PREFIX}:on-${eventName}`, id);
    ipcRenderer.on(`${PREFIX}:on-${eventName}:${id}`, listener);

    return () => {
      ipcRenderer.off(`${PREFIX}:on-${eventName}:${id}`, listener);
      ipcRenderer.send(`${PREFIX}:off-${eventName}`, id);
    };
  },
  off: <K extends keyof AudioCaptureEvents>(eventName?: K) => {
    ipcRenderer.send(`${PREFIX}:off-all`, eventName);
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
