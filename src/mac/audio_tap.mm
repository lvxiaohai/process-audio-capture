#include "../../include/mac/audio_tap.h"
#include "../../include/mac/utils.h"

#ifdef __APPLE__
#include <AVFoundation/AVFoundation.h>
#include <AppKit/AppKit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/AudioHardwareTapping.h>
#include <CoreAudio/CATapDescription.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <libproc.h>
#include <string>
#include <sys/param.h>
#include <vector>

/**
 * @file audio_tap.mm
 * @brief 实现音频捕获功能
 *
 * 本文件实现了基于macOS的音频捕获功能。
 * 使用CoreAudio框架的CATapDescription和AudioHardwareTapping API。
 */

namespace audio_capture {
namespace audio_tap {

// 判断AudioObjectID是否有效
bool isValid(AudioObjectID objectID) { return objectID != kAudioObjectUnknown; }

// 音频回调结构体
struct AudioCallbackData {
  AudioDataCallback callback;
  bool active;
};

// 音频IO回调函数
static OSStatus AudioIOProcFunc(AudioDeviceID inDevice,
                                const AudioTimeStamp *inNow,
                                const AudioBufferList *inInputData,
                                const AudioTimeStamp *inInputTime,
                                AudioBufferList *outOutputData,
                                const AudioTimeStamp *inOutputTime,
                                void *inClientData) {
  AudioCallbackData *data = static_cast<AudioCallbackData *>(inClientData);
  if (!data || !data->active || !data->callback) {
    NSLog(@"[AudioIOProcFunc] 无效的回调数据");
    return noErr;
  }

  // 计算总数据大小
  size_t totalBytes = 0;
  for (UInt32 i = 0; i < inInputData->mNumberBuffers; ++i) {
    totalBytes += inInputData->mBuffers[i].mDataByteSize;
  }

  if (totalBytes == 0) {
    NSLog(@"[AudioIOProcFunc] 没有音频数据");
    return noErr;
  }

  NSLog(@"[AudioIOProcFunc] 接收到 %zu 字节的音频数据", totalBytes);

  // 创建临时缓冲区存储PCM数据
  uint8_t *buffer = new uint8_t[totalBytes];
  size_t offset = 0;

  // 复制所有缓冲区的数据
  for (UInt32 i = 0; i < inInputData->mNumberBuffers; ++i) {
    const AudioBuffer &audioBuffer = inInputData->mBuffers[i];
    memcpy(buffer + offset, audioBuffer.mData, audioBuffer.mDataByteSize);
    offset += audioBuffer.mDataByteSize;
  }

  // 获取音频格式信息
  AudioStreamBasicDescription format;
  UInt32 dataSize = sizeof(format);
  AudioObjectPropertyAddress address = {kAudioDevicePropertyStreamFormat,
                                        kAudioDevicePropertyScopeInput, 0};

  AudioObjectGetPropertyData(inDevice, &address, 0, nullptr, &dataSize,
                             &format);

  int channels = format.mChannelsPerFrame;
  int sampleRate = format.mSampleRate;

  NSLog(@"[AudioIOProcFunc] 音频格式 - 通道数: %d, 采样率: %d", channels,
        sampleRate);

  // 调用回调函数
  data->callback(buffer, totalBytes, channels, sampleRate);

  // 清理临时缓冲区
  delete[] buffer;

  return noErr;
}

ProcessTap::ProcessTap(uint32_t pid) : pid_(pid) {
  NSLog(@"[ProcessTap::ProcessTap] 创建进程音频捕获对象, PID: %u", pid);
}

ProcessTap::~ProcessTap() {
  NSLog(@"[ProcessTap::~ProcessTap] 销毁进程音频捕获对象");
  Stop();
}

bool ProcessTap::Initialize() {
  if (initialized_) {
    NSLog(@"[ProcessTap::Initialize] 已经初始化过");
    return true;
  }

  NSLog(@"[ProcessTap::Initialize] 开始初始化");

  // 准备进程音频捕获
  if (!Prepare(pid_)) {
    NSLog(@"[ProcessTap::Initialize] 初始化失败 - %s", error_message_.c_str());
    return false;
  }

  initialized_ = true;
  NSLog(@"[ProcessTap::Initialize] 初始化成功");
  return true;
}

bool ProcessTap::Prepare(AudioObjectID pid) {
  error_message_ = "";
  NSLog(@"[ProcessTap::Prepare] 准备进程音频捕获, PID: %u", pid);

  @try {
    @autoreleasepool {
      // 检查系统版本是否支持
      NSOperatingSystemVersion requiredVersion = {14, 2, 0};
      if (![[NSProcessInfo processInfo]
              isOperatingSystemAtLeastVersion:requiredVersion]) {
        error_message_ = "当前系统版本不支持，需要 macOS 14.2 或更高版本";
        NSLog(@"[ProcessTap::Prepare] 当前系统版本不支持，需要 macOS 14.2 "
              @"或更高版本");
        return false;
      }

      // 检查默认音频设备是否可用
      NSLog(@"[ProcessTap::Prepare] 检查默认音频设备状态");
      AudioObjectPropertyAddress deviceAddress = {
          kAudioHardwarePropertyDefaultOutputDevice,
          kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};

      AudioObjectID defaultDevice = kAudioObjectUnknown;
      UInt32 deviceDataSize = sizeof(defaultDevice);
      OSStatus deviceErr = AudioObjectGetPropertyData(
          kAudioObjectSystemObject, &deviceAddress, 0, nullptr, &deviceDataSize,
          &defaultDevice);

      if (deviceErr != noErr || defaultDevice == kAudioObjectUnknown) {
        error_message_ = "无法获取默认音频设备，请检查音频设备状态";
        NSLog(@"[ProcessTap::Prepare] 无法获取默认音频设备，错误码: %d",
              (int)deviceErr);
        return false;
      } else {
        NSLog(@"[ProcessTap::Prepare] 默认音频设备可用，设备ID: %u",
              defaultDevice);
      }

      // 获取进程的AudioObjectID
      AudioObjectID objectID = utils::GetAudioObjectIDForPID(pid);
      if (objectID == kAudioObjectUnknown) {
        error_message_ = "无法获取进程的AudioObjectID";
        NSLog(@"[ProcessTap::Prepare] 无法获取进程的AudioObjectID");
        return false;
      }

      NSLog(@"[ProcessTap::Prepare] 获取到进程的AudioObjectID: %u", objectID);

      // 创建进程音频捕获描述
      NSLog(@"[ProcessTap::Prepare] 创建CATapDescription");
      CATapDescription *tapDescription = nil;

      @try {
        tapDescription = [[CATapDescription alloc]
            initStereoMixdownOfProcesses:@[ @(objectID) ]];
      } @catch (NSException *exception) {
        error_message_ = "创建CATapDescription时出现异常: " +
                         std::string([exception.description UTF8String]);
        NSLog(@"[ProcessTap::Prepare] 创建CATapDescription时出现异常: %@",
              exception);
        return false;
      }

      if (!tapDescription) {
        error_message_ = "创建CATapDescription失败";
        NSLog(@"[ProcessTap::Prepare] 创建CATapDescription失败");
        return false;
      }

      NSLog(@"[ProcessTap::Prepare] 设置CATapDescription属性");
      tapDescription.UUID = [NSUUID UUID];
      tapDescription.muteBehavior = CATapMutedWhenTapped;
      tapDescription.name = [NSString stringWithFormat:@"AudioCapture-%u", pid];

      // 创建进程音频捕获
      NSLog(@"[ProcessTap::Prepare] 调用AudioHardwareCreateProcessTap");
      AudioObjectID tapID = kAudioObjectUnknown;
      OSStatus tapErr = noErr;

      @try {
        tapErr = AudioHardwareCreateProcessTap(tapDescription, &tapID);
      } @catch (NSException *exception) {
        error_message_ = "调用AudioHardwareCreateProcessTap时出现异常: " +
                         std::string([exception.description UTF8String]);
        NSLog(@"[ProcessTap::Prepare] "
              @"调用AudioHardwareCreateProcessTap时出现异常: %@",
              exception);
        return false;
      }

      if (tapErr != noErr) {
        error_message_ =
            "进程音频捕获创建失败，错误码: " + std::to_string(tapErr);
        NSLog(@"[ProcessTap::Prepare] 进程音频捕获创建失败，错误码: %d",
              (int)tapErr);
        return false;
      }

      NSLog(@"[ProcessTap::Prepare] 创建进程音频捕获成功，ID: %u", tapID);

      process_tap_id_ = tapID;

      // 获取默认输出设备
      NSLog(@"[ProcessTap::Prepare] 获取默认输出设备");
      AudioObjectID systemOutputID = kAudioObjectUnknown;
      AudioObjectPropertyAddress outputAddress = {
          kAudioHardwarePropertyDefaultOutputDevice,
          kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};

      UInt32 outputDataSize = sizeof(systemOutputID);
      OSStatus outputErr = AudioObjectGetPropertyData(
          kAudioObjectSystemObject, &outputAddress, 0, nullptr, &outputDataSize,
          &systemOutputID);

      if (outputErr != noErr) {
        error_message_ =
            "获取默认输出设备失败，错误码: " + std::to_string(outputErr);
        NSLog(@"[ProcessTap::Prepare] 获取默认输出设备失败，错误码: %d",
              (int)outputErr);
        return false;
      }

      NSLog(@"[ProcessTap::Prepare] 获取到默认输出设备ID: %u", systemOutputID);

      // 获取输出设备UID
      NSLog(@"[ProcessTap::Prepare] 获取输出设备UID");
      CFStringRef outputUID = nullptr;
      outputAddress.mSelector = kAudioDevicePropertyDeviceUID;
      outputDataSize = sizeof(outputUID);
      outputErr =
          AudioObjectGetPropertyData(systemOutputID, &outputAddress, 0, nullptr,
                                     &outputDataSize, &outputUID);

      if (outputErr != noErr) {
        error_message_ =
            "获取设备UID失败，错误码: " + std::to_string(outputErr);
        NSLog(@"[ProcessTap::Prepare] 获取设备UID失败，错误码: %d",
              (int)outputErr);
        return false;
      }

      NSLog(@"[ProcessTap::Prepare] 获取到输出设备UID: %@",
            (__bridge NSString *)outputUID);

      // 创建聚合设备描述
      NSLog(@"[ProcessTap::Prepare] 创建聚合设备描述");
      NSString *aggregateUID = [[NSUUID UUID] UUIDString];
      NSString *tapUUID = [tapDescription.UUID UUIDString];

      // 使用字符串字面量而不是CoreAudio框架的常量
      NSDictionary *description = @{
        @"name" : [NSString stringWithFormat:@"Tap-%u", pid],
        @"uid" : aggregateUID,
        @"master" : (__bridge NSString *)outputUID,
        @"private" : @YES,
        @"stacked" : @NO,
        @"tapautostart" : @YES,
        @"subdevices" : @[ @{@"uid" : (__bridge NSString *)outputUID} ],
        @"taps" : @[
          @{@"drift" : @YES,
            @"uid" : tapUUID}
        ]
      };

      NSLog(@"[ProcessTap::Prepare] 聚合设备描述: %@", description);

      // 获取音频流格式
      NSLog(@"[ProcessTap::Prepare] 获取音频流格式");
      AudioStreamBasicDescription tapStreamDescription = {};
      AudioObjectPropertyAddress streamAddress = {
          kAudioDevicePropertyStreamFormat, kAudioDevicePropertyScopeInput, 0};
      UInt32 streamDataSize = sizeof(tapStreamDescription);
      OSStatus streamErr =
          AudioObjectGetPropertyData(tapID, &streamAddress, 0, nullptr,
                                     &streamDataSize, &tapStreamDescription);

      if (streamErr != noErr) {
        error_message_ =
            "获取音频流格式失败，错误码: " + std::to_string(streamErr);
        NSLog(@"[ProcessTap::Prepare] 获取音频流格式失败，错误码: %d",
              (int)streamErr);
        return false;
      }

      NSLog(@"[ProcessTap::Prepare] 音频流格式 - 通道数: %u, 采样率: %f",
            tapStreamDescription.mChannelsPerFrame,
            tapStreamDescription.mSampleRate);
      tap_stream_description_ = tapStreamDescription;

      // 创建聚合设备
      NSLog(@"[ProcessTap::Prepare] 创建聚合设备");
      AudioObjectID aggregateDeviceID = kAudioObjectUnknown;
      OSStatus aggregateErr = AudioHardwareCreateAggregateDevice(
          (__bridge CFDictionaryRef)description, &aggregateDeviceID);

      if (aggregateErr != noErr) {
        error_message_ =
            "创建聚合设备失败，错误码: " + std::to_string(aggregateErr);
        NSLog(@"[ProcessTap::Prepare] 创建聚合设备失败，错误码: %d",
              (int)aggregateErr);
        return false;
      }

      NSLog(@"[ProcessTap::Prepare] 创建聚合设备成功，ID: %u",
            aggregateDeviceID);
      aggregate_device_id_ = aggregateDeviceID;

      // 释放资源
      if (outputUID) {
        CFRelease(outputUID);
      }
    }

    NSLog(@"[ProcessTap::Prepare] 准备完成");
    return true;
  } @catch (NSException *exception) {
    error_message_ = "准备过程中出现异常: " +
                     std::string([exception.description UTF8String]);
    NSLog(@"[ProcessTap::Prepare] 准备过程中出现异常: %@", exception);
    return false;
  }
}

bool ProcessTap::Start(AudioDataCallback callback) {
  NSLog(@"[ProcessTap::Start] 开始捕获");

  if (!initialized_) {
    error_message_ = "音频捕获未初始化";
    NSLog(@"[ProcessTap::Start] 音频捕获未初始化");
    return false;
  }

  if (capturing_) {
    error_message_ = "音频捕获已经在运行";
    NSLog(@"[ProcessTap::Start] 音频捕获已经在运行");
    return false;
  }

  // 创建回调数据
  NSLog(@"[ProcessTap::Start] 创建回调数据");
  AudioCallbackData *callbackData = new AudioCallbackData;
  callbackData->callback = callback;
  callbackData->active = true;
  callback_ = callback;
  callback_data_ = callbackData;

  // 创建音频IO过程
  NSLog(@"[ProcessTap::Start] 创建音频IO过程, 聚合设备ID: %u",
        aggregate_device_id_);
  OSStatus err = AudioDeviceCreateIOProcID(
      aggregate_device_id_, AudioIOProcFunc, callbackData, &device_proc_id_);

  if (err != noErr) {
    error_message_ = "创建音频IO过程失败，错误码: " + std::to_string(err);
    NSLog(@"[ProcessTap::Start] 创建音频IO过程失败，错误码: %d", (int)err);
    delete callbackData;
    callback_data_ = nullptr;
    return false;
  }

  NSLog(@"[ProcessTap::Start] 创建音频IO过程成功");

  // 启动音频设备
  NSLog(@"[ProcessTap::Start] 启动音频设备");
  err = AudioDeviceStart(aggregate_device_id_, device_proc_id_);
  if (err != noErr) {
    error_message_ = "启动音频设备失败，错误码: " + std::to_string(err);
    NSLog(@"[ProcessTap::Start] 启动音频设备失败，错误码: %d", (int)err);
    AudioDeviceDestroyIOProcID(aggregate_device_id_, device_proc_id_);
    device_proc_id_ = nullptr;
    delete callbackData;
    callback_data_ = nullptr;
    return false;
  }

  NSLog(@"[ProcessTap::Start] 启动音频设备成功");
  capturing_ = true;
  return true;
}

bool ProcessTap::Stop() {
  NSLog(@"[ProcessTap::Stop] 停止捕获");

  if (!capturing_) {
    NSLog(@"[ProcessTap::Stop] 当前没有在捕获，无需停止");
    return true;
  }

  capturing_ = false;

  HandleInvalidation();
  return true;
}

void ProcessTap::HandleInvalidation() {
  NSLog(@"[ProcessTap::HandleInvalidation] 处理无效化");

  // 停止音频设备
  if (aggregate_device_id_ != kAudioObjectUnknown &&
      device_proc_id_ != nullptr) {
    NSLog(@"[ProcessTap::HandleInvalidation] 停止音频设备");
    OSStatus status = AudioDeviceStop(aggregate_device_id_, device_proc_id_);
    if (status != noErr) {
      error_message_ = "停止音频设备失败，错误码: " + std::to_string(status);
      NSLog(@"[ProcessTap::HandleInvalidation] 停止音频设备失败，错误码: %d",
            (int)status);
    } else {
      NSLog(@"[ProcessTap::HandleInvalidation] 停止音频设备成功");
    }
  }

  Cleanup();
}

void ProcessTap::Cleanup() {
  NSLog(@"[ProcessTap::Cleanup] 清理资源");

  // 销毁音频IO过程ID
  if (aggregate_device_id_ != kAudioObjectUnknown &&
      device_proc_id_ != nullptr) {
    NSLog(@"[ProcessTap::Cleanup] 销毁音频IO过程ID");
    AudioDeviceDestroyIOProcID(aggregate_device_id_, device_proc_id_);
    device_proc_id_ = nullptr;
  }

  // 销毁聚合设备
  if (aggregate_device_id_ != kAudioObjectUnknown) {
    NSLog(@"[ProcessTap::Cleanup] 销毁聚合设备");
    AudioHardwareDestroyAggregateDevice(aggregate_device_id_);
    aggregate_device_id_ = kAudioObjectUnknown;
  }

  // 销毁进程音频捕获
  if (process_tap_id_ != kAudioObjectUnknown) {
    NSLog(@"[ProcessTap::Cleanup] 销毁进程音频捕获");
    @try {
      AudioHardwareDestroyProcessTap(process_tap_id_);
    } @catch (NSException *exception) {
      NSLog(@"[ProcessTap::Cleanup] 销毁进程音频捕获时出现异常: %@", exception);
    }
    process_tap_id_ = kAudioObjectUnknown;
  }

  // 清理回调数据
  if (callback_data_) {
    NSLog(@"[ProcessTap::Cleanup] 清理回调数据");
    AudioCallbackData *data = static_cast<AudioCallbackData *>(callback_data_);
    data->active = false;
    delete data;
    callback_data_ = nullptr;
  }

  callback_ = nullptr;
  NSLog(@"[ProcessTap::Cleanup] 资源清理完成");
}

std::string ProcessTap::GetErrorMessage() const { return error_message_; }

} // namespace audio_tap
} // namespace audio_capture
#endif // __APPLE__