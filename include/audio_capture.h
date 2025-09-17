#pragma once

#include "process_manager.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

/**
 * @file audio_capture.h
 * @brief 音频捕获模块的主要接口定义
 *
 * 本文件定义了音频捕获模块的核心接口和数据结构，包括进程信息、权限状态、
 * 音频数据回调等。该接口设计为跨平台，但目前仅实现了macOS平台。
 */

namespace audio_capture {

// 使用process_manager中的类型
using ProcessInfo = process_manager::ProcessInfo;

/**
 * @typedef AudioDataCallback
 * @brief PCM音频数据回调函数类型
 *
 * 当捕获到新的音频数据时，将通过此回调函数传递给调用者。
 *
 * @param data 指向PCM音频数据的指针
 * @param length 数据长度（字节数）
 * @param channels 音频通道数
 * @param sampleRate 采样率（Hz）
 */
using AudioDataCallback = std::function<void(const uint8_t *data, size_t length,
                                             int channels, int sampleRate)>;

/**
 * @class AudioCapture
 * @brief 音频捕获接口类
 *
 * 定义了音频捕获模块的核心接口，包括权限管理、进程列表获取、音频捕获等功能。
 * 这是一个抽象基类，需要由平台特定的实现类继承并实现所有方法。
 */
class AudioCapture {
public:
  /**
   * @brief 虚析构函数
   *
   * 确保派生类的析构函数被正确调用。
   */
  virtual ~AudioCapture() = default;

  /**
   * @brief 开始捕获指定进程的音频
   * @param pid 目标进程ID
   * @param callback 接收音频数据的回调函数
   * @return 是否成功启动捕获
   *
   * 开始捕获指定进程的音频，并通过回调函数返回PCM数据。
   */
  virtual bool StartCapture(uint32_t pid, AudioDataCallback callback) = 0;

  /**
   * @brief 停止捕获
   * @return 是否成功停止捕获
   *
   * 停止当前的音频捕获，释放相关资源。
   */
  virtual bool StopCapture() = 0;
};

/**
 * @brief 工厂函数 - 创建平台特定的实现
 * @return 平台特定的AudioCapture实例
 *
 * 根据当前运行平台创建相应的音频捕获实现实例。
 */
std::unique_ptr<AudioCapture> CreatePlatformAudioCapture();

} // namespace audio_capture