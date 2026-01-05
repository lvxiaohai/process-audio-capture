#ifdef __APPLE__

#include "../../include/mac/mac_audio_capture.h"
#include "../../include/mac/audio_tap.h"
#include "../../include/mac/mac_utils.h"
#include "../../include/permission_manager.h"
#include "../../include/process_manager.h"
#include <AVFoundation/AVFoundation.h>
#include <AppKit/AppKit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <libproc.h>
#include <sstream>
#include <string>
#include <sys/param.h>
#include <vector>

/**
 * @file mac_audio_capture.mm
 * @brief macOS平台的音频捕获实现
 *
 * 本文件实现了macOS平台特定的音频捕获功能。
 * 使用CoreAudio框架实现音频捕获。
 */

namespace audio_capture {

MacAudioCapture::MacAudioCapture() { Initialize(); }

MacAudioCapture::~MacAudioCapture() {
  if (capturing_) {
    StopCapture();
  }
  Cleanup();
}

bool MacAudioCapture::Initialize() {
  if (initialized_) {
    return true;
  }

  initialized_ = true;
  return true;
}

void MacAudioCapture::Cleanup() { initialized_ = false; }

bool MacAudioCapture::StartCapture(uint32_t pid, AudioDataCallback callback) {
  if (capturing_) {
    return false;
  }

  callback_ = callback;
  current_pid_ = pid;

  // 创建音频捕获对象
  process_tap_ = std::make_unique<audio_tap::ProcessTap>(pid);

  // 初始化音频捕获
  if (!process_tap_->Initialize()) {
    return false;
  }

  // 开始捕获
  if (!process_tap_->Start(callback)) {
    return false;
  }

  capturing_ = true;
  return true;
}

bool MacAudioCapture::StopCapture() {
  if (!capturing_) {
    return false;
  }

  capturing_ = false;

  // 停止音频捕获
  if (process_tap_) {
    process_tap_->Stop();
    process_tap_.reset();
  }

  callback_ = nullptr;
  current_pid_ = 0;

  return true;
}

bool MacAudioCapture::IsCapturing() const { return capturing_.load(); }

/**
 * @brief 工厂函数 - 创建平台特定的实现
 * @return 平台特定的AudioCapture实例
 */
std::unique_ptr<AudioCapture> CreatePlatformAudioCapture() {
  return std::make_unique<MacAudioCapture>();
}

} // namespace audio_capture

#endif // __APPLE__
