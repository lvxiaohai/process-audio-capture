import bindings from "bindings";
import type {AudioData, PermissionStatus, ProcessInfo} from "./types";
import {EventEmitter} from "events";

/**
 * 原生插件接口
 */
interface AudioCaptureAddon {
  /** 检查音频捕获权限状态 */
  checkPermission(): PermissionStatus;

  /** 请求音频捕获权限 */
  requestPermission(callback: (result: PermissionStatus) => void): void;

  /** 获取可捕获音频的进程列表 */
  getProcessList(): ProcessInfo[];

  /** 开始捕获指定进程的音频 */
  startCapture(pid: number, callback: (audioData: AudioData) => void): boolean;

  /** 停止捕获 */
  stopCapture(): boolean;
}

interface NativeModule {
  AudioCaptureAddon: {
    new (): AudioCaptureAddon;
  };
}

// 加载原生插件
const native: NativeModule = bindings("audio_capture");

/**
 * 音频捕获事件映射
 */
export interface AudioCaptureEvents {
  /** 是否正在捕获音频状态变化 */
  capturing: [capturing: boolean];

  /** 音频数据 */
  "audio-data": [audioData: AudioData];
}

/**
 * 音频捕获类
 */
class AudioCaptureStub extends EventEmitter<AudioCaptureEvents> {
  constructor() {
    super();
  }

  /** 是否正在捕获音频 */
  public get isCapturing(): boolean {
    return false;
  }

  private set isCapturing(_value: boolean) {
    // do nothing
  }

  /** 当前平台是否支持音频捕获 */
  isPlatformSupported(): boolean {
    return false;
  }

  /** 检查音频捕获权限状态 */
  checkPermission(): PermissionStatus {
    return { status: "authorized" };
  }

  /** 请求音频捕获权限 */
  requestPermission(): Promise<PermissionStatus> {
    return Promise.resolve({ status: "authorized" });
  }

  /** 获取可捕获音频的进程列表 */
  getProcessList(): ProcessInfo[] {
    return [];
  }

  /** 开始捕获指定进程的音频 */
  startCapture(
    _pid: number,
    _callback?: (audioData: AudioData) => void
  ): boolean {
    return false;
  }

  /** 停止捕获 */
  stopCapture(): boolean {
    return false;
  }
}

/**
 * 音频捕获类
 */
export class AudioCapture extends AudioCaptureStub {
  private addon: AudioCaptureAddon;
  private _capturing: boolean;

  constructor() {
    super();
    // 创建C++类的实例
    this.addon = new native.AudioCaptureAddon();
    this._capturing = false;
  }

  public get isCapturing(): boolean {
    return this._capturing;
  }

  private set isCapturing(value: boolean) {
    if (value === this._capturing) {
      return;
    }
    this._capturing = value;
    this.emit("capturing", value);
  }

  isPlatformSupported(): boolean {
    return true;
  }

  checkPermission(): PermissionStatus {
    return this.addon.checkPermission();
  }

  requestPermission(): Promise<PermissionStatus> {
    return new Promise<PermissionStatus>((resolve) => {
      this.addon.requestPermission((result) => {
        resolve(result);
      });
    });
  }

  getProcessList(): ProcessInfo[] {
    // 检查权限
    const permission = this.checkPermission();
    if (permission.status !== "authorized") {
      throw new Error("没有音频捕获权限");
    }

    return this.addon.getProcessList();
  }

  startCapture(
    pid: number,
    callback?: (audioData: AudioData) => void
  ): boolean {
    // 检查权限
    const permission = this.checkPermission();
    if (permission.status !== "authorized") {
      throw new Error("没有音频捕获权限");
    }

    if (this.isCapturing) {
      throw new Error("已经在捕获音频，请先停止");
    }

    try {
      const result = this.addon.startCapture(pid, (audioData) => {
        callback?.(audioData);
        this.emit("audio-data", audioData);
      });
      if (result) {
        this.isCapturing = true;
      }

      return result;
    } catch (error) {
      throw error;
    }
  }

  stopCapture(): boolean {
    if (!this.isCapturing) {
      return false;
    }

    const result = this.addon.stopCapture();
    if (result) {
      this.isCapturing = false;
    }

    return result;
  }
}

/** 音频捕获实例 */
let audioCapture: AudioCaptureStub;

if (process.platform === "darwin" || process.platform === "win32") {
  audioCapture = new AudioCapture();
} else {
  // 为不支持的平台提供回退方案
  audioCapture = new AudioCaptureStub();
}

export { audioCapture };
