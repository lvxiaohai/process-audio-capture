#pragma once

#ifdef _WIN32

#include "../audio_capture.h"
#include <atomic>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <string>
#include <thread>
#include <windows.h>
#include <wrl/client.h>
#include <wrl/ftm.h>
#include <wrl/implements.h>

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::FtmBase;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

/**
 * @file audio_tap.h
 * @brief Windows进程级音频捕获
 *
 * 使用WASAPI进程级loopback模式捕获指定进程的音频输出
 */

namespace audio_capture {
namespace win_audio {

/**
 * @class AudioTap
 * @brief Windows进程级音频捕获类
 *
 * 使用WASAPI进程级loopback模式捕获指定进程的音频输出
 */
class AudioTap : public RuntimeClass<RuntimeClassFlags<ClassicCom>, FtmBase,
                                     IActivateAudioInterfaceCompletionHandler> {
public:
  AudioTap();
  ~AudioTap();

  // WRL初始化方法
  HRESULT RuntimeClassInitialize(uint32_t pid);

  // 主要接口
  bool Initialize();
  bool Start(AudioDataCallback callback);
  void Stop();

  // 状态查询
  bool IsCapturing() const { return is_capturing_.load(); }
  std::string GetErrorMessage() const { return error_message_; }

  // IActivateAudioInterfaceCompletionHandler实现
  STDMETHOD(ActivateCompleted)(
      IActivateAudioInterfaceAsyncOperation *operation) override;

private:
  // 基本属性
  uint32_t target_pid_;
  std::atomic<bool> is_capturing_{false};
  std::string error_message_;
  AudioDataCallback callback_;

  // COM接口
  ComPtr<IMMDeviceEnumerator> device_enumerator_;
  ComPtr<IMMDevice> audio_device_;
  ComPtr<IAudioClient> audio_client_;
  ComPtr<IAudioCaptureClient> capture_client_;
  ComPtr<IAudioSessionManager2> session_manager_;
  ComPtr<IAudioSessionControl> target_session_;

  // 线程和同步
  std::thread capture_thread_;
  std::atomic<bool> stop_capture_{false};
  HANDLE capture_event_;
  HANDLE activate_completed_event_;

  // 音频格式
  WAVEFORMATEX *mix_format_;
  UINT32 buffer_frame_count_;
  HRESULT activate_result_;

  // 内部方法
  void Cleanup();
  void SetError(const std::string &message);
  bool ActivateProcessLoopbackAudioClient();
  HRESULT InitializeAudioClientInCallback();
  void CaptureThreadProc();
  void ProcessAudioData(BYTE *data, UINT32 frames, DWORD flags);
};

} // namespace win_audio
} // namespace audio_capture

#endif // _WIN32
