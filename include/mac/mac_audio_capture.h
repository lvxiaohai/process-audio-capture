#pragma once

#ifdef __APPLE__

#include "../audio_capture.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/**
 * @file mac_audio_capture.h
 * @brief macOS平台的音频捕获实现
 *
 * 本文件定义了macOS平台特定的音频捕获实现类MacAudioCapture，
 * 该类继承自通用接口AudioCapture，并实现了所有必要的方法。
 */

namespace audio_capture {
// 前向声明
namespace audio_tap {
class ProcessTap;
}

/**
 * @class MacAudioCapture
 * @brief macOS平台的音频捕获实现类
 *
 * 该类使用CoreAudio API实现了在macOS平台上捕获指定进程的音频数据的功能。
 * 它提供了获取进程列表、开始/停止捕获等功能，并通过回调函数将捕获的音频数据传递给JavaScript。
 * 权限管理功能由PermissionManager类处理。
 */
class MacAudioCapture : public AudioCapture {
public:
  /**
   * @brief 构造函数
   */
  MacAudioCapture();

  /**
   * @brief 析构函数
   */
  ~MacAudioCapture() override;

  /**
   * @brief 开始捕获指定进程的音频
   * @param pid 目标进程ID
   * @param callback 接收音频数据的回调函数
   * @return 是否成功启动捕获
   */
  bool StartCapture(uint32_t pid, AudioDataCallback callback) override;

  /**
   * @brief 停止音频捕获
   * @return 是否成功停止捕获
   */
  bool StopCapture() override;

  /**
   * @brief 检查是否正在捕获音频
   * @return 是否正在捕获
   */
  bool IsCapturing() const override;

private:
  std::atomic<bool> capturing_{false};   ///< 是否正在捕获音频
  std::atomic<bool> initialized_{false}; ///< 是否已初始化
  AudioDataCallback callback_;           ///< 音频数据回调函数
  uint32_t current_pid_{0};              ///< 当前捕获的进程ID

  // 音频捕获对象
  std::unique_ptr<audio_tap::ProcessTap> process_tap_;

  /**
   * @brief 初始化音频服务
   * @return 是否成功初始化
   */
  bool Initialize();

  /**
   * @brief 清理音频服务
   */
  void Cleanup();
};

} // namespace audio_capture

#endif // __APPLE__
