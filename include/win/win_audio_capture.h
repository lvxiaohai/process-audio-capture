#pragma once

#ifdef _WIN32

#include "../audio_capture.h"
#include <atomic>
#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <mutex>
#include <psapi.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

/**
 * @file win_audio_capture.h
 * @brief Windows音频捕获实现
 *
 * Windows平台音频捕获接口实现
 */

namespace audio_capture {
// 前向声明
namespace win_audio {
class AudioTap;
}

/**
 * @brief Windows音频捕获实现类
 *
 * 使用WASAPI实现Windows平台的音频捕获功能
 */
class WinAudioCapture : public AudioCapture {
public:
  WinAudioCapture();
  ~WinAudioCapture() override;

  bool StartCapture(uint32_t pid, AudioDataCallback callback) override;
  bool StopCapture() override;
  bool IsCapturing() const override;

private:
  // 状态管理
  std::atomic<bool> capturing_{false};
  std::atomic<bool> initialized_{false};
  AudioDataCallback callback_;
  uint32_t current_pid_{0};

  // 音频捕获对象
  Microsoft::WRL::ComPtr<win_audio::AudioTap> process_capture_;

  // COM接口指针
  IMMDeviceEnumerator *device_enumerator_{nullptr};
  IMMDevice *audio_device_{nullptr};
  IAudioClient *audio_client_{nullptr};
  IAudioCaptureClient *capture_client_{nullptr};

  // 捕获线程
  std::thread capture_thread_;
  std::mutex capture_mutex_;

  // 内部方法
  bool Initialize();
  void Cleanup();
  void CleanupCapture();
  void CaptureThreadProc();
  IAudioSessionControl *GetProcessAudioSession(uint32_t pid);
  bool InitializeAudioCapture(uint32_t pid);
};
} // namespace audio_capture

#endif // _WIN32
