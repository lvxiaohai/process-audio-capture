#pragma once

#ifdef __APPLE__

#include "../audio_capture.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/AudioHardwareTapping.h>
#include <CoreAudio/CATapDescription.h>
#include <CoreAudio/CoreAudio.h>
#include <functional>

/**
 * @file audio_tap.h
 * @brief 声明音频捕获相关的函数和类
 *
 * 本文件声明了用于捕获指定进程音频的函数和类。
 * 基于macOS的AudioProcessTap API实现。
 */

namespace audio_capture {
namespace audio_tap {
// 常量定义
static const AudioObjectID kAudioObjectUnknown = 0;

// 扩展方法
bool isValid(AudioObjectID objectID);

/**
 * @class ProcessTap
 * @brief 进程音频捕获类
 *
 * 该类用于捕获指定进程的音频数据，基于macOS的CoreAudio API。
 */
class ProcessTap {
public:
  /**
   * @brief 构造函数
   * @param pid 目标进程ID
   */
  ProcessTap(uint32_t pid);

  /**
   * @brief 析构函数
   */
  ~ProcessTap();

  /**
   * @brief 初始化音频捕获
   * @return 是否成功初始化
   */
  bool Initialize();

  /**
   * @brief 开始捕获音频
   * @param callback 音频数据回调函数
   * @return 是否成功开始捕获
   */
  bool Start(AudioDataCallback callback);

  /**
   * @brief 停止捕获音频
   * @return 是否成功停止捕获
   */
  bool Stop();

  /**
   * @brief 获取错误信息
   * @return 错误信息字符串
   */
  std::string GetErrorMessage() const;

private:
  uint32_t pid_;                         ///< 目标进程ID
  bool initialized_ = false;             ///< 是否已初始化
  bool capturing_ = false;               ///< 是否正在捕获
  std::string error_message_;            ///< 错误信息
  AudioDataCallback callback_ = nullptr; ///< 音频数据回调函数
  void *callback_data_ = nullptr;        ///< 回调数据

  // macOS音频API相关成员
  AudioObjectID process_tap_id_ = kAudioObjectUnknown;      ///< 进程音频捕获ID
  AudioObjectID aggregate_device_id_ = kAudioObjectUnknown; ///< 聚合设备ID
  AudioDeviceIOProcID device_proc_id_ = nullptr;            ///< 设备IO过程ID
  AudioStreamBasicDescription tap_stream_description_ = {}; ///< 音频流格式
  void *audio_format_ = nullptr; ///< AVAudioFormat对象指针

  /**
   * @brief 准备进程音频捕获
   * @param objectID 进程对象ID
   * @return 是否成功准备
   */
  bool Prepare(AudioObjectID objectID);

  /**
   * @brief 清理资源
   */
  void Cleanup();

  /**
   * @brief 处理无效化
   */
  void HandleInvalidation();
};

} // namespace audio_tap
} // namespace audio_capture

#endif // __APPLE__