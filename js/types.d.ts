/**
 * 图标数据
 */
export interface IconData {
  data: Uint8Array;
  /** 图片格式，如: png, jpg */
  format: string;
  width: number;
  height: number;
}

/**
 * 进程信息
 */
export interface ProcessInfo {
  pid: number;
  name: string;
  description: string;
  path: string;
  icon?: IconData;
}

/**
 * 音频数据
 */
export interface AudioData {
  /** PCM音频数据 -1 ~ 1 */
  buffer: Float32Array;
  /** 音频通道数 */
  channels: number;
  /** 采样率（Hz） */
  sampleRate: number;
}

/**
 * 权限状态
 */
export interface PermissionStatus {
  status: "authorized" | "denied" | "unknown";
}

/**
 * 取消订阅
 */
export type Unsubscribe = () => void;

/**
 * 定义暴露给渲染进程的API接口
 */
export interface ProcessAudioCaptureApi {
  /** 检查音频捕获权限 */
  checkPermission: () => Promise<PermissionStatus>;

  /** 请求音频捕获权限 */
  requestPermission: () => Promise<PermissionStatus>;

  /** 获取进程列表 */
  getProcessList: () => Promise<ProcessInfo[]>;

  /** 开始捕获指定进程的音频 */
  startCapture: (pid: number) => Promise<boolean>;

  /** 停止捕获 */
  stopCapture: () => Promise<boolean>;

  /** 检查是否正在捕获音频 */
  isCapturing: () => Promise<boolean>;

  /** 监听音频数据 */
  onAudioData: (callback: (data: AudioData) => void) => Unsubscribe;

  /** 监听是否正在捕获音频 */
  onCapturing: (callback: (capturing: boolean) => void) => Unsubscribe;
}

declare global {
  interface Window {
    /**
     * Process Audio Capture API - 进程级音频捕获接口
     *
     * 在你的渲染进程代码中导入此文件以获得 window.processAudioCapture 的类型支持：
     *
     * 注意：在使用前，请确保
     *   - 在 main 进程中调用了 setupAudioCaptureIpc()
     *   - 在 preload 中调用了 exposeAudioCaptureApi()
     */
    processAudioCapture: ProcessAudioCaptureApi;
  }
}
