#ifdef _WIN32

#include "../../include/win/audio_tap.h"
#include <comdef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <thread>
#include <tlhelp32.h>
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

bool AudioTap::CheckTargetProcessExists() {
  /**
   * 检查目标进程是否存在的多层级验证策略
   *
   * 为了最大化成功率，使用两种互补的检查方法：
   * 1. 直接进程访问：尝试不同权限级别打开进程
   * 2. 进程快照遍历：当直接访问失败时的备用方案
   */

  // 定义权限级别数组，按从低到高的顺序排列
  // 这样可以优先使用最小权限，减少被系统拒绝的可能性
  static const DWORD access_levels[] = {
      PROCESS_QUERY_LIMITED_INFORMATION,          // 最小权限，适用于大多数场景
      PROCESS_QUERY_INFORMATION,                  // 标准查询权限
      PROCESS_QUERY_INFORMATION | PROCESS_VM_READ // 包含内存访问权限
  };

  // 方案1: 渐进式权限检查
  // 从最小权限开始，逐步尝试更高权限级别
  for (DWORD access : access_levels) {
    HANDLE process_handle = OpenProcess(access, FALSE, target_pid_);
    if (process_handle) {
      CloseHandle(process_handle);
      return true; // 成功打开进程，说明进程存在且可访问
    }
  }

  // 方案2: 进程快照备用检查
  // 当所有直接访问方式都失败时，使用系统快照进行验证
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return false; // 无法创建快照，可能是系统权限问题
  }

  // 使用局部RAII类管理快照句柄，确保资源正确释放
  struct SnapshotGuard {
    HANDLE handle;
    explicit SnapshotGuard(HANDLE h) : handle(h) {}
    ~SnapshotGuard() {
      if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
      }
    }
  } guard(snapshot);

  // 遍历系统中的所有进程，查找目标进程ID
  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);

  if (Process32First(snapshot, &pe32)) {
    do {
      if (pe32.th32ProcessID == target_pid_) {
        return true; // 在系统快照中找到目标进程
      }
    } while (Process32Next(snapshot, &pe32));
  }

  return false; // 所有检查方法都失败，进程可能不存在
}

bool AudioTap::ActivateProcessLoopbackAudioClient() {
  /**
   * 激活进程级音频回环捕获客户端
   *
   * 这是音频捕获的核心初始化过程，包括：
   * 1. 验证目标进程的存在性和可访问性
   * 2. 配置进程级音频回环参数
   * 3. 异步激活音频接口
   * 4. 等待激活完成
   */

  // 步骤1: 验证目标进程
  // 使用多层级检查策略确保目标进程存在且可访问
  if (!CheckTargetProcessExists()) {
    SetError("Target process does not exist or cannot be accessed");
    return false;
  }

  // 步骤2: 配置进程级音频回环参数
  // 设置音频捕获的目标进程和捕获模式
  AUDIOCLIENT_ACTIVATION_PARAMS activation_params = {};
  activation_params.ActivationType =
      AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;

  // 配置为包含目标进程树模式，这样可以捕获主进程及其子进程的音频
  activation_params.ProcessLoopbackParams.ProcessLoopbackMode =
      PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
  activation_params.ProcessLoopbackParams.TargetProcessId = target_pid_;

  // 将参数封装为PROPVARIANT格式，供Windows音频系统使用
  PROPVARIANT activate_params = {};
  activate_params.vt = VT_BLOB;
  activate_params.blob.cbSize = sizeof(activation_params);
  activate_params.blob.pBlobData = reinterpret_cast<BYTE *>(&activation_params);

  // 步骤3: 异步激活音频接口
  // 使用异步方式激活音频客户端，避免阻塞当前线程
  ComPtr<IActivateAudioInterfaceAsyncOperation> async_op;
  HRESULT hr = ActivateAudioInterfaceAsync(
      VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
      &activate_params, this, &async_op);

  if (FAILED(hr)) {
    SetError("Failed to activate audio interface async - HRESULT: 0x" +
             std::to_string(hr));
    return false;
  }

  // 步骤4: 等待激活完成
  // 设置合理的超时时间（10秒），防止无限等待
  DWORD wait_result = WaitForSingleObject(activate_completed_event_, 10000);
  if (wait_result != WAIT_OBJECT_0) {
    SetError("Timeout waiting for audio interface activation");
    return false;
  }

  // 返回激活结果，由ActivateCompleted回调函数设置
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
  // 使用更现代的默认格式：48000Hz, 32-bit Float
  // 这是大多数现代应用（Chrome、游戏、视频播放器）使用的格式
  // 使用 IEEE Float 格式可以减少格式转换损失
  if (mix_format_) {
    CoTaskMemFree(mix_format_);
  }
  mix_format_ = (WAVEFORMATEX *)CoTaskMemAlloc(sizeof(WAVEFORMATEX));
  if (!mix_format_) {
    return E_OUTOFMEMORY;
  }

  mix_format_->wFormatTag = WAVE_FORMAT_IEEE_FLOAT; // 使用 Float 格式
  mix_format_->nChannels = 2;
  mix_format_->nSamplesPerSec = 48000; // 48kHz 是现代应用的标准
  mix_format_->wBitsPerSample = 32;    // 32-bit Float
  mix_format_->nBlockAlign =
      mix_format_->nChannels * mix_format_->wBitsPerSample / BITS_PER_BYTE;
  mix_format_->nAvgBytesPerSec =
      mix_format_->nSamplesPerSec * mix_format_->nBlockAlign;
  mix_format_->cbSize = 0;

  // 初始化音频客户端
  // AUTOCONVERTPCM 让 Windows 自动处理格式转换
  // 共享模式下，周期参数必须为 0
  HRESULT hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                         AUDCLNT_STREAMFLAGS_LOOPBACK |
                                             AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                             AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
                                         CAPTURE_BUFFER_DURATION,
                                         0, // shared mode, period must be 0
                                         mix_format_, nullptr);

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
        // 只在非静音时处理数据
        if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
          ProcessAudioData(data, frames_available, flags);
        }
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

  // 计算样本数和数据大小
  UINT32 sample_count = frames * mix_format_->nChannels;
  size_t float_data_size = sample_count * sizeof(float);

  // 使用线程局部存储避免频繁内存分配
  thread_local std::vector<float> float_buffer;
  float_buffer.resize(sample_count);

  // 根据我们请求的格式处理数据
  // 使用 AUTOCONVERTPCM 时，Windows 通常会按我们请求的格式返回数据
  // 但为了健壮性，仍然检查实际格式
  if (mix_format_->wFormatTag == WAVE_FORMAT_IEEE_FLOAT &&
      mix_format_->wBitsPerSample == 32) {
    // 32-bit Float 格式 - 直接复制（最高效，最常见的情况）
    float *float_samples = reinterpret_cast<float *>(data);
    memcpy(float_buffer.data(), float_samples, float_data_size);
  } else if (mix_format_->wFormatTag == WAVE_FORMAT_PCM &&
             mix_format_->wBitsPerSample == 16) {
    // 16-bit PCM - 可能在某些旧设备上发生
    int16_t *int_samples = reinterpret_cast<int16_t *>(data);
    for (UINT32 i = 0; i < sample_count; ++i) {
      float_buffer[i] = static_cast<float>(int_samples[i]) / 32768.0f;
    }
  } else if (mix_format_->wFormatTag == WAVE_FORMAT_PCM &&
             mix_format_->wBitsPerSample == 32) {
    // 32-bit PCM - 理论上可能发生
    int32_t *int_samples = reinterpret_cast<int32_t *>(data);
    for (UINT32 i = 0; i < sample_count; ++i) {
      float_buffer[i] = static_cast<float>(int_samples[i]) / 2147483648.0f;
    }
  } else {
    // 未预期的格式 - 输出警告并尝试按 Float 处理
    static bool warned = false;
    if (!warned) {
      std::wcerr << L"[AudioTap] Warning: Unexpected audio format - Tag: 0x"
                 << std::hex << mix_format_->wFormatTag << std::dec
                 << L", Bits: " << mix_format_->wBitsPerSample << std::endl;
      warned = true;
    }
    // 尝试按 Float 处理（最可能的情况）
    float *float_samples = reinterpret_cast<float *>(data);
    memcpy(float_buffer.data(), float_samples, float_data_size);
  }

  // 通过回调传递音频数据
  callback_(reinterpret_cast<const uint8_t *>(float_buffer.data()),
            float_data_size, mix_format_->nChannels,
            mix_format_->nSamplesPerSec);
}

} // namespace win_audio
} // namespace audio_capture

#endif // _WIN32
