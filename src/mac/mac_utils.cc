#ifdef __APPLE__

#include "../../include/mac/mac_utils.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libproc.h>
#include <stdio.h>
#include <string>
#include <sys/param.h>
#include <vector>

/**
 * @file mac_utils.cc
 * @brief macOS 音频工具函数实现
 *
 * 提供与 Core Audio API 交互的核心功能，包括音频进程管理和信息获取。
 */

namespace audio_capture {
namespace mac_utils {

/**
 * @brief 将 CFStringRef 转换为 std::string
 *
 * 安全地将 Core Foundation 字符串对象转换为 C++ 标准字符串，
 * 处理内存分配和 UTF-8 编码转换。
 *
 * @param cfString Core Foundation 字符串引用
 * @return 转换后的 std::string，失败时返回空字符串
 */
std::string CFStringToStdString(CFStringRef cfString) {
  if (cfString == nullptr) {
    return "";
  }

  CFIndex length = CFStringGetLength(cfString);
  CFIndex maxSize =
      CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  char *buffer = static_cast<char *>(malloc(maxSize));

  if (buffer == nullptr) {
    return "";
  }

  if (CFStringGetCString(cfString, buffer, maxSize, kCFStringEncodingUTF8)) {
    std::string result(buffer);
    free(buffer);
    return result;
  }

  free(buffer);
  return "";
}

/**
 * @brief 获取 AudioObjectID 的属性值
 *
 * 通用模板函数，用于从 Core Audio 对象获取指定属性的值。
 *
 * @tparam T 属性值的类型
 * @param objectID 音频对象 ID
 * @param selector 属性选择器
 * @param value 输出参数，用于存储获取的属性值
 * @return 是否成功获取属性值
 */
template <typename T>
bool GetAudioObjectProperty(AudioObjectID objectID,
                            AudioObjectPropertySelector selector, T &value) {
  AudioObjectPropertyAddress address = {selector,
                                        kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};

  UInt32 dataSize = sizeof(T);
  OSStatus status = AudioObjectGetPropertyData(objectID, &address, 0, nullptr,
                                               &dataSize, &value);

  return status == noErr;
}

/**
 * @brief 获取系统中所有具有音频活动的进程列表
 *
 * 使用 Core Audio API 获取当前在系统中注册的音频进程。
 * 返回的进程列表包含所有可能有音频活动的进程。
 *
 * @return 包含音频进程 AudioObjectID 的向量
 */
std::vector<AudioObjectID> GetProcessList() {
  std::vector<AudioObjectID> result;

  // 创建属性地址结构
  AudioObjectPropertyAddress address = {kAudioHardwarePropertyProcessObjectList,
                                        kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};

  // 检查属性是否存在
  if (!AudioObjectHasProperty(kAudioObjectSystemObject, &address)) {
    return result;
  }

  // 获取数据大小
  UInt32 dataSize = 0;
  OSStatus status = AudioObjectGetPropertyDataSize(
      kAudioObjectSystemObject, &address, 0, nullptr, &dataSize);

  if (status != noErr || dataSize == 0) {
    return result;
  }

  // 计算进程数量并分配内存
  int count = dataSize / sizeof(AudioObjectID);
  if (count <= 0 || count > 1000) { // 合理性检查
    return result;
  }

  result.resize(count);

  // 获取实际数据
  status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0,
                                      nullptr, &dataSize, result.data());

  if (status != noErr) {
    result.clear();
  }

  return result;
}

/**
 * @brief 检查进程是否正在播放音频
 *
 * 使用 Core Audio API 检查指定进程是否当前有音频活动。
 * 这可以用来过滤出真正在播放音频的进程。
 *
 * @param processID 进程的 AudioObjectID
 * @return 如果进程正在播放音频则返回 true
 */
bool IsProcessPlayingAudio(AudioObjectID processID) {
  UInt32 isRunning = 0;
  AudioObjectPropertyAddress address = {kAudioProcessPropertyIsRunning,
                                        kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};

  UInt32 dataSize = sizeof(isRunning);
  OSStatus status = AudioObjectGetPropertyData(processID, &address, 0, nullptr,
                                               &dataSize, &isRunning);

  return (status == noErr) && (isRunning != 0);
}

/**
 * @brief 获取进程的 PID
 *
 * 从 AudioObjectID 获取对应的进程 ID (PID)。
 * 这是连接 Core Audio 对象和系统进程的关键功能。
 *
 * @param processID 进程的 AudioObjectID
 * @return 进程 PID，获取失败时返回 0
 */
pid_t GetProcessPID(AudioObjectID processID) {
  AudioObjectPropertyAddress address = {kAudioProcessPropertyPID,
                                        kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};

  pid_t pid = 0;
  UInt32 dataSize = sizeof(pid);
  OSStatus status = AudioObjectGetPropertyData(processID, &address, 0, nullptr,
                                               &dataSize, &pid);

  return (status == noErr) ? pid : 0;
}

/**
 * @brief 根据 PID 获取对应的 AudioObjectID
 *
 * 遍历所有音频进程，找到与指定 PID 匹配的 AudioObjectID。
 * 这个函数用于音频捕获功能。
 *
 * @param pid 进程 ID
 * @return 对应的 AudioObjectID，未找到时返回 kAudioObjectUnknown
 */
AudioObjectID GetAudioObjectIDForPID(uint32_t pid) {
  if (pid == 0) {
    return kAudioObjectUnknown;
  }

  std::vector<AudioObjectID> processIDs = GetProcessList();

  for (AudioObjectID processID : processIDs) {
    if (processID == kAudioObjectUnknown || processID == 0) {
      continue;
    }

    pid_t currentPid = GetProcessPID(processID);
    if (currentPid == static_cast<pid_t>(pid)) {
      return processID;
    }
  }

  return kAudioObjectUnknown;
}

} // namespace mac_utils
} // namespace audio_capture

#endif // __APPLE__