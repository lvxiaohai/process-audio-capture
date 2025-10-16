#ifdef __APPLE__

#include "../../include/mac/audio_tap.h"
#include "../../include/mac/mac_utils.h"
#include <AVFoundation/AVFoundation.h>
#include <AppKit/AppKit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/AudioHardwareTapping.h>
#include <CoreAudio/CATapDescription.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <libproc.h>
#include <stdexcept>
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
  void *format;                    // AVAudioFormat对象指针
  AudioObjectID aggregateDeviceID; // 聚合设备ID
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
  if (!data) {
    return noErr;
  }

  if (!data->active) {
    return noErr;
  }

  if (!data->callback) {
    return noErr;
  }

  if (!data->format) {
    return noErr;
  }

  // 检查输入数据有效性
  if (!inInputData || inInputData->mNumberBuffers == 0) {
    return noErr;
  }

  // 计算总数据大小并验证每个缓冲区
  size_t totalBytes = 0;
  for (UInt32 i = 0; i < inInputData->mNumberBuffers; ++i) {
    // 检查缓冲区指针有效性
    if (!inInputData->mBuffers[i].mData) {
      return noErr;
    }
    // 检查数据大小合理性
    if (inInputData->mBuffers[i].mDataByteSize >
        16 * 1024 * 1024) { // 限制单个缓冲区最大16MB
      return noErr;
    }
    totalBytes += inInputData->mBuffers[i].mDataByteSize;
  }

  if (totalBytes == 0 || totalBytes > 64 * 1024 * 1024) { // 限制总数据大小64MB
    return noErr;
  }

  // 使用预先创建的AVAudioFormat对象
  AVAudioFormat *format = (__bridge AVAudioFormat *)data->format;
  if (!format) {
    return noErr;
  }

  // 获取音频参数
  UInt32 channels = format.channelCount;
  int sampleRate = format.sampleRate;

  // 查询聚合设备的实际采样率（更准确）
  if (data->aggregateDeviceID != kAudioObjectUnknown) {
    Float64 actualRate = 0;
    AudioObjectPropertyAddress actualRateAddr = {
        kAudioDevicePropertyActualSampleRate, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};
    UInt32 size = sizeof(actualRate);
    OSStatus err =
        AudioObjectGetPropertyData(data->aggregateDeviceID, &actualRateAddr, 0,
                                   nullptr, &size, &actualRate);
    if (err == noErr && actualRate > 0) {
      sampleRate = static_cast<int>(actualRate);
    }
  }

  // 重新计算帧数：如果是非交错数据，每个缓冲区包含一个声道的所有帧
  UInt32 frameCount;
  if (inInputData->mNumberBuffers > 1) {
    // 非交错：每个缓冲区是一个声道的帧数据
    frameCount = inInputData->mBuffers[0].mDataByteSize / sizeof(float);
  } else {
    // 交错：所有声道在一个缓冲区中
    frameCount = totalBytes / (4 * channels);
  }

  // 使用AVAudioPCMBuffer直接处理音频数据，参考Swift实现
  AVAudioPCMBuffer *pcmBuffer =
      [[AVAudioPCMBuffer alloc] initWithPCMFormat:format
                                 bufferListNoCopy:inInputData
                                      deallocator:nil];

  if (!pcmBuffer) {
    return noErr;
  }

  // 设置有效帧数
  pcmBuffer.frameLength = frameCount;

  // 获取音频数据指针和大小
  uint8_t *buffer = nullptr;
  size_t bufferSize = 0;

  if (pcmBuffer.format.isInterleaved) {
    // 交错格式：直接使用数据
    buffer = (uint8_t *)pcmBuffer.audioBufferList->mBuffers[0].mData;
    bufferSize = pcmBuffer.audioBufferList->mBuffers[0].mDataByteSize;

  } else {
    // 非交错格式：转换为交错格式
    bufferSize = frameCount * channels * sizeof(float);
    buffer = new uint8_t[bufferSize];
    float *outputFloat = reinterpret_cast<float *>(buffer);

    // 将非交错数据转换为交错数据
    for (UInt32 frame = 0; frame < frameCount; ++frame) {
      for (UInt32 channel = 0; channel < channels; ++channel) {
        const float *channelData =
            (const float *)pcmBuffer.audioBufferList->mBuffers[channel].mData;
        outputFloat[frame * channels + channel] = channelData[frame];
      }
    }
  }

  // 创建数据副本并调用回调函数，确保数据生命周期安全
  if (buffer && bufferSize > 0 && bufferSize <= 64 * 1024 * 1024) {
    try {
      std::vector<uint8_t> bufferCopy(buffer, buffer + bufferSize);
      data->callback(bufferCopy.data(), bufferSize, channels, sampleRate);
    } catch (const std::exception &e) {
      // 内存分配失败，跳过这帧数据
    } catch (...) {
      // 其他异常，跳过这帧数据
    }
  }

  // 清理临时缓冲区（仅在非交错格式时需要清理，因为我们分配了新内存）
  if (!pcmBuffer.format.isInterleaved) {
    delete[] buffer;
  }

  return noErr;
}

ProcessTap::ProcessTap(uint32_t pid) : pid_(pid) {}

ProcessTap::~ProcessTap() { Stop(); }

bool ProcessTap::Initialize() {
  if (initialized_) {
    return true;
  }

  // 获取进程的AudioObjectID
  AudioObjectID objectID =
      audio_capture::mac_utils::GetAudioObjectIDForPID(pid_);
  if (objectID == kAudioObjectUnknown) {
    return false;
  }

  // 准备进程音频捕获
  if (!Prepare(objectID)) {
    return false;
  }

  initialized_ = true;
  return true;
}

bool ProcessTap::Prepare(AudioObjectID objectID) {
  error_message_ = "";

  @try {
    @autoreleasepool {

      // 创建进程音频捕获描述
      CATapDescription *tapDescription = nil;

      @try {
        tapDescription = [[CATapDescription alloc]
            initStereoMixdownOfProcesses:@[ @(objectID) ]];
      } @catch (NSException *exception) {
        error_message_ = "创建CATapDescription时出现异常: " +
                         std::string([exception.description UTF8String]);
        return false;
      }

      if (!tapDescription) {
        error_message_ = "创建CATapDescription失败";
        return false;
      }
      tapDescription.UUID = [NSUUID UUID];
      tapDescription.muteBehavior = CATapUnmuted;
      tapDescription.name =
          [NSString stringWithFormat:@"AudioCapture-%u", objectID];

      // 创建进程音频捕获
      AudioObjectID tapID = kAudioObjectUnknown;
      OSStatus tapErr = noErr;

      @try {
        tapErr = AudioHardwareCreateProcessTap(tapDescription, &tapID);
      } @catch (NSException *exception) {
        error_message_ = "调用AudioHardwareCreateProcessTap时出现异常: " +
                         std::string([exception.description UTF8String]);
        return false;
      }

      if (tapErr != noErr) {
        error_message_ =
            "进程音频捕获创建失败，错误码: " + std::to_string(tapErr);
        return false;
      }

      process_tap_id_ = tapID;

      // 获取默认输出设备
      AudioObjectID systemOutputID = kAudioObjectUnknown;
      AudioObjectPropertyAddress outputAddress = {
          kAudioHardwarePropertyDefaultSystemOutputDevice,
          kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};

      UInt32 outputDataSize = sizeof(systemOutputID);
      OSStatus outputErr = AudioObjectGetPropertyData(
          kAudioObjectSystemObject, &outputAddress, 0, nullptr, &outputDataSize,
          &systemOutputID);

      if (outputErr != noErr) {
        error_message_ =
            "获取默认输出设备失败，错误码: " + std::to_string(outputErr);
        return false;
      }

      // 获取输出设备UID
      CFStringRef outputUID = nullptr;
      outputAddress.mSelector = kAudioDevicePropertyDeviceUID;
      outputDataSize = sizeof(CFStringRef);
      outputErr =
          AudioObjectGetPropertyData(systemOutputID, &outputAddress, 0, nullptr,
                                     &outputDataSize, &outputUID);

      if (outputErr != noErr) {
        error_message_ =
            "获取设备UID失败，错误码: " + std::to_string(outputErr);
        return false;
      }

      // 创建聚合设备描述
      NSString *aggregateUID = [[NSUUID UUID] UUIDString];
      NSString *tapUUID = [tapDescription.UUID UUIDString];

      // 使用字符串字面量而不是CoreAudio框架的常量
      NSDictionary *description = @{
        @"name" : [NSString stringWithFormat:@"Tap-%u", objectID],
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

      // 获取音频流格式
      AudioStreamBasicDescription tapStreamDescription = {};
      AudioObjectPropertyAddress streamAddress = {
          kAudioTapPropertyFormat, kAudioObjectPropertyScopeGlobal,
          kAudioObjectPropertyElementMain};
      UInt32 streamDataSize = sizeof(tapStreamDescription);
      OSStatus streamErr =
          AudioObjectGetPropertyData(tapID, &streamAddress, 0, nullptr,
                                     &streamDataSize, &tapStreamDescription);

      if (streamErr != noErr) {
        error_message_ =
            "获取音频流格式失败，错误码: " + std::to_string(streamErr);
        return false;
      }
      tap_stream_description_ = tapStreamDescription;

      // 创建聚合设备
      AudioObjectID aggregateDeviceID = kAudioObjectUnknown;
      OSStatus aggregateErr = AudioHardwareCreateAggregateDevice(
          (__bridge CFDictionaryRef)description, &aggregateDeviceID);

      if (aggregateErr != noErr) {
        error_message_ =
            "创建聚合设备失败，错误码: " + std::to_string(aggregateErr);
        return false;
      }
      aggregate_device_id_ = aggregateDeviceID;

      // 释放资源
      if (outputUID) {
        CFRelease(outputUID);
      }
    }

    return true;
  } @catch (NSException *exception) {
    error_message_ = "准备过程中出现异常: " +
                     std::string([exception.description UTF8String]);
    return false;
  }
}

bool ProcessTap::Start(AudioDataCallback callback) {

  if (!initialized_) {
    error_message_ = "音频捕获未初始化";
    return false;
  }

  if (capturing_) {
    error_message_ = "音频捕获已经在运行";
    return false;
  }

  // 创建AVAudioFormat对象（参考Swift版本在启动时获取格式）
  AVAudioFormat *format = [[AVAudioFormat alloc]
      initWithStreamDescription:&tap_stream_description_];
  if (!format) {
    error_message_ = "创建AVAudioFormat失败";
    return false;
  }

  // 存储格式对象以供回调使用
  audio_format_ = (__bridge void *)format;

  // 创建回调数据
  AudioCallbackData *callbackData = new AudioCallbackData;
  callbackData->callback = callback;
  callbackData->active = true;
  callbackData->format = audio_format_; // 传递格式对象给回调
  callbackData->aggregateDeviceID = aggregate_device_id_;

  callback_ = callback;
  callback_data_ = callbackData;

  // 创建音频IO过程
  OSStatus err = AudioDeviceCreateIOProcID(
      aggregate_device_id_, AudioIOProcFunc, callbackData, &device_proc_id_);

  if (err != noErr) {
    error_message_ = "创建音频IO过程失败，错误码: " + std::to_string(err);
    delete callbackData;
    callback_data_ = nullptr;
    return false;
  }

  // 启动音频设备
  err = AudioDeviceStart(aggregate_device_id_, device_proc_id_);
  if (err != noErr) {
    error_message_ = "启动音频设备失败，错误码: " + std::to_string(err);
    AudioDeviceDestroyIOProcID(aggregate_device_id_, device_proc_id_);
    device_proc_id_ = nullptr;
    delete callbackData;
    callback_data_ = nullptr;
    return false;
  }

  capturing_ = true;
  return true;
}

bool ProcessTap::Stop() {

  if (!capturing_) {
    return true;
  }

  capturing_ = false;

  HandleInvalidation();
  return true;
}

void ProcessTap::HandleInvalidation() {

  // 停止音频设备
  if (aggregate_device_id_ != kAudioObjectUnknown &&
      device_proc_id_ != nullptr) {
    OSStatus status = AudioDeviceStop(aggregate_device_id_, device_proc_id_);
    if (status != noErr) {
      error_message_ = "停止音频设备失败，错误码: " + std::to_string(status);
    }
  }

  Cleanup();
}

void ProcessTap::Cleanup() {

  // 销毁音频IO过程ID
  if (aggregate_device_id_ != kAudioObjectUnknown &&
      device_proc_id_ != nullptr) {
    AudioDeviceDestroyIOProcID(aggregate_device_id_, device_proc_id_);
    device_proc_id_ = nullptr;
  }

  // 销毁聚合设备
  if (aggregate_device_id_ != kAudioObjectUnknown) {
    AudioHardwareDestroyAggregateDevice(aggregate_device_id_);
    aggregate_device_id_ = kAudioObjectUnknown;
  }

  // 销毁进程音频捕获
  if (process_tap_id_ != kAudioObjectUnknown) {
    @try {
      AudioHardwareDestroyProcessTap(process_tap_id_);
    } @catch (NSException *exception) {
    }
    process_tap_id_ = kAudioObjectUnknown;
  }

  // 清理AVAudioFormat对象
  if (audio_format_) {
    CFRelease(audio_format_);
    audio_format_ = nullptr;
  }

  // 清理回调数据
  if (callback_data_) {
    AudioCallbackData *data = static_cast<AudioCallbackData *>(callback_data_);
    data->active = false;
    delete data;
    callback_data_ = nullptr;
  }

  callback_ = nullptr;
}

std::string ProcessTap::GetErrorMessage() const { return error_message_; }

} // namespace audio_tap
} // namespace audio_capture

#endif // __APPLE__