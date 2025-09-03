import { EventEmitter } from "events";

/**
 * 图标数据接口
 */
export interface IconData {
  /** 图标二进制数据 */
  data: Uint8Array;
  /** 图标格式 (例如 "png", "jpeg", "ico") */
  format: string;
  /** 图标宽度 */
  width: number;
  /** 图标高度 */
  height: number;
}

/**
 * 进程信息接口
 */
export interface ProcessInfo {
  /** 进程ID */
  pid: number;
  /** 进程名称 */
  name: string;
  /** 进程描述 */
  description: string;
  /** 进程可执行文件路径 */
  path: string;
  /** 进程图标 */
  icon?: IconData;
}

/**
 * 音频数据接口
 */
export interface AudioData {
  /** PCM音频数据 */
  data: Uint8Array;
  /** 音频通道数 */
  channels: number;
  /** 采样率 */
  sampleRate: number;
}

/**
 * 权限状态接口
 */
export interface PermissionStatus {
  /** 权限状态: 'authorized', 'denied', 'unknown' */
  status: "authorized" | "denied" | "unknown";
}

/**
 * 音频捕获模块的TypeScript接口定义
 */
declare interface AudioCapture extends EventEmitter {
  /**
   * 检查音频捕获权限状态
   * @returns 当前权限状态
   */
  checkPermission(): PermissionStatus;

  /**
   * 请求音频捕获权限
   * @returns Promise，解析为权限状态
   */
  requestPermission(): Promise<PermissionStatus>;

  /**
   * 获取可捕获音频的进程列表
   * @returns 进程列表
   * @throws 如果没有音频捕获权限
   */
  getProcessList(): ProcessInfo[];

  /**
   * 开始捕获指定进程的音频
   * @param pid 进程ID
   * @param callback 音频数据回调函数
   * @returns 是否成功开始捕获
   * @throws 如果没有音频捕获权限
   */
  startCapture(pid: number, callback: (audioData: AudioData) => void): boolean;

  /**
   * 停止捕获
   * @returns 是否成功停止捕获
   */
  stopCapture(): boolean;

  /**
   * 测试方法 - Hello World
   * @param input 输入字符串
   * @returns 处理后的字符串
   */
  helloWorld(input?: string): string;

  /**
   * 监听音频数据事件
   * @param event 事件名称 'audio-data'
   * @param listener 监听器函数
   */
  on(event: "audio-data", listener: (audioData: AudioData) => void): this;

  /**
   * 监听捕获开始事件
   * @param event 事件名称 'capture-started'
   * @param listener 监听器函数
   */
  on(event: "capture-started", listener: (info: { pid: number }) => void): this;

  /**
   * 监听捕获停止事件
   * @param event 事件名称 'capture-stopped'
   * @param listener 监听器函数
   */
  on(event: "capture-stopped", listener: () => void): this;

  /**
   * 监听权限变化事件
   * @param event 事件名称 'permission-changed'
   * @param listener 监听器函数
   */
  on(
    event: "permission-changed",
    listener: (status: PermissionStatus) => void
  ): this;
}

/**
 * 音频捕获模块的单例实例
 */
declare const audioCapture: AudioCapture;

export default audioCapture;
