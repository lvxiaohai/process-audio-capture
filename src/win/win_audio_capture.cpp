#ifdef _WIN32

#include "../../include/win/win_audio_capture.h"
#include "../../include/win/audio_tap.h"
#include "../../include/win/win_utils.h"
#include <comdef.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <wrl/module.h>

/**
 * @file win_audio_capture.cpp
 * @brief Windows平台的音频捕获实现
 *
 * 本文件实现了Windows平台特定的音频捕获功能。
 * 使用Windows Audio Session API (WASAPI)实现进程级音频捕获。
 */

namespace audio_capture {

WinAudioCapture::WinAudioCapture() {
  // 设置控制台输出为UTF-8编码
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  Initialize();
}

WinAudioCapture::~WinAudioCapture() {
  if (capturing_) {
    StopCapture();
  }
  Cleanup();
}

bool WinAudioCapture::Initialize() {
  if (initialized_) {
    return true;
  }

  // 初始化COM库
  if (!win_utils::InitializeCOM()) {
    return false;
  }

  initialized_ = true;
  return true;
}

void WinAudioCapture::Cleanup() {
  if (initialized_) {
    win_utils::CleanupCOM();
    initialized_ = false;
  }
}

bool WinAudioCapture::StartCapture(uint32_t pid, AudioDataCallback callback) {
  if (capturing_) {
    return false;
  }

  callback_ = callback;
  current_pid_ = pid;

  // 创建音频捕获对象
  Microsoft::WRL::ComPtr<win_audio::AudioTap> audio_tap;
  HRESULT hr =
      Microsoft::WRL::MakeAndInitialize<win_audio::AudioTap>(&audio_tap, pid);
  if (FAILED(hr)) {
    return false;
  }
  process_capture_ = std::move(audio_tap);

  // 初始化并开始捕获
  if (!process_capture_->Initialize() || !process_capture_->Start(callback)) {
    return false;
  }

  capturing_ = true;
  return true;
}

bool WinAudioCapture::StopCapture() {
  if (!capturing_) {
    return false;
  }

  capturing_ = false;

  // 停止音频捕获
  if (process_capture_) {
    process_capture_->Stop();
    process_capture_.Reset();
  }

  CleanupCapture();
  return true;
}

void WinAudioCapture::CleanupCapture() {
  callback_ = nullptr;
  current_pid_ = 0;
}

std::unique_ptr<AudioCapture> CreatePlatformAudioCapture() {
  return std::make_unique<WinAudioCapture>();
}

} // namespace audio_capture

#endif // _WIN32
