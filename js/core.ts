import bindings from "bindings";
import type {
  AudioCaptureEvents,
  AudioData,
  PermissionStatus,
  ProcessInfo,
} from "./types";
import { EventEmitter } from "events";
import * as os from "os";
import path from "path";

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

  /** 检查是否正在捕获音频 */
  isCapturing(): boolean;
}

interface OsVersion {
  majorVersion: number;
  minorVersion: number;
}

interface NativeModule {
  AudioCaptureAddon: {
    new (): AudioCaptureAddon;
  };
}

const native: NativeModule = (() => {
  let native: NativeModule;

  // 项目名称
  const projectName = "process-audio-capture";
  // 绑定名称 与 binding.gyp 中的 target_name 一致
  const bindingsName = "process-audio-capture";
  const options: bindings.Options = {
    bindings: bindingsName,
  };

  try {
    // 加载原生插件
    native = bindings({ ...options });
  } catch (error) {
    // electron asar打包时 会走到这里
    const root = bindings.getRoot(bindings.getFileName());
    const moduleRoot = path.join(root, "/", "node_modules", "/", projectName);

    native = bindings({
      ...options,
      module_root: moduleRoot,
      try: [
        // node-gyp's linked version in the "build" dir
        ["module_root", "build", "bindings"],
        // node-waf and gyp_addon (a.k.a node-gyp)
        ["module_root", "build", "Debug", "bindings"],
        ["module_root", "build", "Release", "bindings"],
        // Debug files, for development (legacy behavior, remove for node v0.9)
        ["module_root", "out", "Debug", "bindings"],
        ["module_root", "Debug", "bindings"],
        // Release files, but manually compiled (legacy behavior, remove for node v0.9)
        ["module_root", "out", "Release", "bindings"],
        ["module_root", "Release", "bindings"],
        // Legacy from node-waf, node <= 0.4.x
        ["module_root", "build", "default", "bindings"],
        // Production "Release" buildtype binary (meh...)
        ["module_root", "compiled", "version", "platform", "arch", "bindings"],
        // node-qbs builds
        ["module_root", "addon-build", "release", "install-root", "bindings"],
        ["module_root", "addon-build", "debug", "install-root", "bindings"],
        ["module_root", "addon-build", "default", "install-root", "bindings"],
        // node-pre-gyp path ./lib/binding/{node_abi}-{platform}-{arch}
        ["module_root", "lib", "binding", "nodePreGyp", "bindings"],
      ],
    });
  }

  return native;
})();

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

  /**
   * 当前平台是否支持音频捕获
   *
   * 仅支持
   * - macOS 14.4+
   * - Windows 10+
   * - Linux 不支持
   */
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

  constructor() {
    super();
    // 创建C++类的实例
    this.addon = new native.AudioCaptureAddon();
  }

  public get isCapturing(): boolean {
    return this.addon.isCapturing();
  }

  isPlatformSupported(): boolean {
    const platform = os.platform();
    const { majorVersion, minorVersion } = this.getOsVersion();

    // macOS 支持检查
    if (platform === "darwin") {
      // macOS版本映射：Darwin 23.4.0 = macOS 14.4.0
      // Darwin主版本号 = macOS主版本号 + 9 (从macOS 10开始)
      // 要求macOS 14.4+ (Darwin 23.4+)
      return majorVersion >= 24 || (majorVersion === 23 && minorVersion >= 4);
    }

    // Windows 支持检查
    if (platform === "win32") {
      // Windows 10的内核版本是10.0.x
      // 要求Windows 10+ (内核版本10.0+)
      return majorVersion >= 10;
    }

    // 其他平台不支持
    return false;
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

    try {
      const result = this.addon.startCapture(pid, (audioData) => {
        callback?.(audioData);
        this.emit("audio-data", audioData);
      });
      if (result) {
        this.emit("capturing", true);
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
      this.emit("capturing", false);
    }

    return result;
  }

  private getOsVersion(): OsVersion {
    try {
      const osRelease = os.release();
      const version = osRelease.split(".");
      const majorVersion = parseInt(version[0], 10) || 0;
      const minorVersion = parseInt(version[1], 10) || 0;
      return { majorVersion, minorVersion };
    } catch (error) {
      return { majorVersion: 0, minorVersion: 0 };
    }
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
