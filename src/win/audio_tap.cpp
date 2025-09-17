#ifdef _WIN32

#include "../../include/win/audio_tap.h"
#include <comdef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <thread>
#include <wrl/implements.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

/**
 * @file audio_tap.cpp
 * @brief Windows进程级音频捕获实现
 *
 * 使用WASAPI进程级loopback模式捕获指定进程的音频输出
 */

namespace audio_capture {
namespace win_audio {

// 音频缓冲区持续时间（100ns单位）
#define CAPTURE_BUFFER_DURATION 200000
#define BITS_PER_BYTE 8

AudioTap::AudioTap()
    : target_pid_(0), mix_format_(nullptr), buffer_frame_count_(0),
      capture_event_(nullptr), activate_completed_event_(nullptr),
      activate_result_(E_FAIL) {}

HRESULT AudioTap::RuntimeClassInitialize(uint32_t pid) {
  target_pid_ = pid;

  // 创建事件对象
  capture_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  activate_completed_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);

  if (!capture_event_ || !activate_completed_event_) {
    return E_FAIL;
  }

  return S_OK;
}

AudioTap::~AudioTap() {
  Stop();
  Cleanup();

  if (capture_event_) {
    CloseHandle(capture_event_);
    capture_event_ = nullptr;
  }

  if (activate_completed_event_) {
    CloseHandle(activate_completed_event_);
    activate_completed_event_ = nullptr;
  }

  if (mix_format_) {
    CoTaskMemFree(mix_format_);
    mix_format_ = nullptr;
  }
}

bool AudioTap::Initialize() {
  // 初始化COM为单线程单元（STA）
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    if (hr == RPC_E_CHANGED_MODE) {
      CoUninitialize();
      hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
      if (FAILED(hr)) {
        SetError("Failed to initialize COM as STA");
        return false;
      }
    } else {
      SetError("Failed to initialize COM");
      return false;
    }
  }

  // 初始化Media Foundation
  hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
  if (FAILED(hr)) {
    SetError("Failed to initialize Media Foundation");
    return false;
  }

  return ActivateProcessLoopbackAudioClient();
}

bool AudioTap::Start(AudioDataCallback callback) {
  if (is_capturing_.load()) {
    return false;
  }

  if (!audio_client_ || !capture_client_) {
    SetError("Audio client not initialized");
    return false;
  }

  callback_ = callback;

  // 启动音频客户端
  HRESULT hr = audio_client_->Start();
  if (FAILED(hr)) {
    SetError("Failed to start audio client");
    return false;
  }

  is_capturing_.store(true);
  stop_capture_.store(false);

  // 启动捕获线程
  capture_thread_ = std::thread(&AudioTap::CaptureThreadProc, this);

  return true;
}

void AudioTap::Stop() {
  if (!is_capturing_.load()) {
    return;
  }

  is_capturing_.store(false);
  stop_capture_.store(true);

  // 等待捕获线程结束
  if (capture_thread_.joinable()) {
    capture_thread_.join();
  }

  // 停止音频客户端
  if (audio_client_) {
    audio_client_->Stop();
  }
}

void AudioTap::Cleanup() {
  Stop();

  capture_client_.Reset();
  audio_client_.Reset();
  audio_device_.Reset();
  device_enumerator_.Reset();
  session_manager_.Reset();
  target_session_.Reset();

  MFShutdown();
  CoUninitialize();
}

void AudioTap::SetError(const std::string &message) {
  error_message_ = message;
  std::wcerr << L"AudioTap Error: "
             << std::wstring(message.begin(), message.end()) << std::endl;
}

bool AudioTap::ActivateProcessLoopbackAudioClient() {
  // 检查目标进程是否存在
  HANDLE process_handle =
      OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, target_pid_);
  if (!process_handle) {
    SetError("Target process does not exist or cannot be accessed");
    return false;
  }
  CloseHandle(process_handle);

  // 设置进程级loopback参数
  AUDIOCLIENT_ACTIVATION_PARAMS activation_params = {};
  activation_params.ActivationType =
      AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
  activation_params.ProcessLoopbackParams.ProcessLoopbackMode =
      PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
  activation_params.ProcessLoopbackParams.TargetProcessId = target_pid_;

  PROPVARIANT activate_params = {};
  activate_params.vt = VT_BLOB;
  activate_params.blob.cbSize = sizeof(activation_params);
  activate_params.blob.pBlobData = reinterpret_cast<BYTE *>(&activation_params);

  // 异步激活音频接口
  ComPtr<IActivateAudioInterfaceAsyncOperation> async_op;
  HRESULT hr = ActivateAudioInterfaceAsync(
      VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
      &activate_params, this, &async_op);

  if (FAILED(hr)) {
    SetError("Failed to activate audio interface async - HRESULT: 0x" +
             std::to_string(hr));
    return false;
  }

  // 等待激活完成（10秒超时）
  DWORD wait_result = WaitForSingleObject(activate_completed_event_, 10000);
  if (wait_result != WAIT_OBJECT_0) {
    SetError("Timeout waiting for audio interface activation");
    return false;
  }

  return SUCCEEDED(activate_result_);
}

HRESULT
AudioTap::ActivateCompleted(IActivateAudioInterfaceAsyncOperation *operation) {
  // 获取激活结果
  HRESULT hr_activate_result = E_UNEXPECTED;
  ComPtr<IUnknown> audio_interface;

  HRESULT hr =
      operation->GetActivateResult(&hr_activate_result, &audio_interface);
  if (FAILED(hr)) {
    activate_result_ = hr;
    SetEvent(activate_completed_event_);
    return hr;
  }

  if (FAILED(hr_activate_result)) {
    SetError("Audio interface activation failed - HRESULT: 0x" +
             std::to_string(hr_activate_result));
    activate_result_ = hr_activate_result;
    SetEvent(activate_completed_event_);
    return hr_activate_result;
  }

  // 获取音频客户端接口
  hr = audio_interface.As(&audio_client_);
  if (FAILED(hr)) {
    SetError("Failed to get IAudioClient interface");
    activate_result_ = hr;
    SetEvent(activate_completed_event_);
    return hr;
  }

  // 初始化音频客户端
  activate_result_ = InitializeAudioClientInCallback();

  // 通知激活完成
  SetEvent(activate_completed_event_);

  return S_OK;
}

HRESULT AudioTap::InitializeAudioClientInCallback() {
  // 设置固定的16位PCM格式（与demo保持一致）
  if (mix_format_) {
    CoTaskMemFree(mix_format_);
  }
  mix_format_ = (WAVEFORMATEX *)CoTaskMemAlloc(sizeof(WAVEFORMATEX));
  if (!mix_format_) {
    return E_OUTOFMEMORY;
  }

  mix_format_->wFormatTag = WAVE_FORMAT_PCM;
  mix_format_->nChannels = 2;
  mix_format_->nSamplesPerSec = 44100;
  mix_format_->wBitsPerSample = 16;
  mix_format_->nBlockAlign =
      mix_format_->nChannels * mix_format_->wBitsPerSample / BITS_PER_BYTE;
  mix_format_->nAvgBytesPerSec =
      mix_format_->nSamplesPerSec * mix_format_->nBlockAlign;
  mix_format_->cbSize = 0;

  // 初始化音频客户端
  HRESULT hr = audio_client_->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
      CAPTURE_BUFFER_DURATION, AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM, mix_format_,
      nullptr);

  if (FAILED(hr)) {
    SetError("Audio client initialization failed - HRESULT: 0x" +
             std::to_string(hr));
    return hr;
  }

  // 获取缓冲区大小
  hr = audio_client_->GetBufferSize(&buffer_frame_count_);
  if (FAILED(hr)) {
    return hr;
  }

  // 获取捕获客户端
  hr = audio_client_->GetService(
      __uuidof(IAudioCaptureClient),
      reinterpret_cast<void **>(capture_client_.GetAddressOf()));
  if (FAILED(hr)) {
    return hr;
  }

  // 设置事件句柄
  hr = audio_client_->SetEventHandle(capture_event_);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

void AudioTap::CaptureThreadProc() {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

  while (!stop_capture_.load()) {
    DWORD wait_result = WaitForSingleObject(capture_event_, 1000);

    if (wait_result != WAIT_OBJECT_0 || stop_capture_.load()) {
      continue;
    }

    // 处理所有可用的音频数据包
    UINT32 packet_length = 0;
    HRESULT hr = capture_client_->GetNextPacketSize(&packet_length);

    while (SUCCEEDED(hr) && packet_length != 0) {
      BYTE *data;
      UINT32 frames_available;
      DWORD flags;
      UINT64 device_position, qpc_position;

      hr = capture_client_->GetBuffer(&data, &frames_available, &flags,
                                      &device_position, &qpc_position);

      if (SUCCEEDED(hr)) {
        ProcessAudioData(data, frames_available, flags);
        hr = capture_client_->ReleaseBuffer(frames_available);
      }

      if (FAILED(hr)) {
        break;
      }

      hr = capture_client_->GetNextPacketSize(&packet_length);
    }
  }
}

void AudioTap::ProcessAudioData(BYTE *data, UINT32 frames, DWORD flags) {
  if (!callback_ || !mix_format_ || !data || frames == 0) {
    return;
  }

  // 计算样本数和转换后的数据大小
  UINT32 sample_count = frames * mix_format_->nChannels;
  size_t float_data_size = sample_count * sizeof(float);

  // 使用线程局部存储避免频繁内存分配
  thread_local std::vector<float> float_buffer;
  float_buffer.resize(sample_count);

  // 将16位整数转换为32位浮点数
  int16_t *int_samples = reinterpret_cast<int16_t *>(data);
  for (UINT32 i = 0; i < sample_count; ++i) {
    float_buffer[i] = static_cast<float>(int_samples[i]) / 32768.0f;
  }

  // 通过回调传递音频数据
  callback_(reinterpret_cast<const uint8_t *>(float_buffer.data()),
            float_data_size, mix_format_->nChannels,
            mix_format_->nSamplesPerSec);
}

} // namespace win_audio
} // namespace audio_capture

#endif // _WIN32
