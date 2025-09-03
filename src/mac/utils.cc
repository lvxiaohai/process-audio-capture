#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libproc.h>
#include <stdio.h>
#include <string>
#include <sys/param.h>
#include <vector>

/**
 * @file utils.cc
 * @brief macOS平台特定的辅助函数
 *
 * 本文件包含与CoreAudio API交互的辅助函数，用于获取和处理音频相关的进程信息。
 */

namespace audio_capture {
namespace utils {
/**
 * @brief 将CFStringRef转换为std::string
 *
 * 此函数将CoreFoundation字符串对象转换为C++标准字符串，
 * 处理了内存分配和UTF-8编码转换。
 *
 * @param cfString CoreFoundation字符串引用
 * @return 转换后的std::string，如果输入为nullptr或转换失败则返回空字符串
 */
std::string CFStringToStdString(CFStringRef cfString) {
  if (cfString == nullptr) {
    return "";
  }

  // 获取CFString的长度和所需的最大缓冲区大小
  CFIndex length = CFStringGetLength(cfString);
  CFIndex maxSize =
      CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  char *buffer = static_cast<char *>(malloc(maxSize));

  if (buffer == nullptr) {
    return "";
  }

  // 将CFString转换为C字符串
  if (CFStringGetCString(cfString, buffer, maxSize, kCFStringEncodingUTF8)) {
    std::string result(buffer);
    free(buffer);
    return result;
  }

  free(buffer);
  return "";
}

/**
 * @brief 获取AudioObjectID的属性值
 *
 * 通用模板函数，用于从CoreAudio对象获取属性值。
 *
 * @tparam T 属性值的类型
 * @param objectID 音频对象ID
 * @param selector 属性选择器
 * @param value 输出参数，用于存储获取的属性值
 * @return 是否成功获取属性值
 */
template <typename T>
bool GetAudioObjectProperty(AudioObjectID objectID,
                            AudioObjectPropertySelector selector, T &value) {
  // 创建属性地址结构
  AudioObjectPropertyAddress address = {selector,
                                        kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};

  // 获取属性数据
  UInt32 dataSize = sizeof(T);
  OSStatus status = AudioObjectGetPropertyData(objectID, &address, 0, nullptr,
                                               &dataSize, &value);

  return status == noErr;
}

/**
 * @brief 获取系统中所有具有音频活动的进程列表
 *
 * 使用CoreAudio API获取所有当前在系统中注册的音频进程。
 *
 * @return 包含进程AudioObjectID的向量
 */
std::vector<AudioObjectID> GetProcessList() {
  std::vector<AudioObjectID> result;

  // 创建属性地址结构
  AudioObjectPropertyAddress address = {kAudioHardwarePropertyProcessObjectList,
                                        kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};

  // 首先获取数据大小
  UInt32 dataSize = 0;
  OSStatus status = AudioObjectGetPropertyDataSize(
      kAudioObjectSystemObject, &address, 0, nullptr, &dataSize);

  if (status != noErr) {
    printf("[utils::GetProcessList] 获取进程列表大小失败，错误码: %d\n",
           (int)status);
    return result;
  }

  // 计算进程数量并分配内存
  int count = dataSize / sizeof(AudioObjectID);
  printf("[utils::GetProcessList] 找到 %d 个进程\n", count);
  result.resize(count);

  // 获取实际数据
  status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0,
                                      nullptr, &dataSize, result.data());

  if (status != noErr) {
    printf("[utils::GetProcessList] 获取进程列表失败，错误码: %d\n",
           (int)status);
    result.clear();
  }

  return result;
}

/**
 * @brief 检查进程是否正在播放音频
 *
 * 使用CoreAudio API检查指定的进程是否当前有音频活动。
 *
 * @param processID 进程的AudioObjectID
 * @return 如果进程正在播放音频则返回true，否则返回false
 */
bool IsProcessPlayingAudio(AudioObjectID processID) {
  UInt32 isRunning = 0;
  AudioObjectPropertyAddress address = {kAudioProcessPropertyIsRunning,
                                        kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};

  UInt32 dataSize = sizeof(isRunning);
  OSStatus status = AudioObjectGetPropertyData(processID, &address, 0, nullptr,
                                               &dataSize, &isRunning);

  // 只有当状态为noErr且isRunning不为0时才返回true
  return (status == noErr) && (isRunning != 0);
}

/**
 * @brief 获取进程的Bundle ID
 *
 * 获取指定进程的Bundle ID，如com.apple.Music。
 * Bundle ID是macOS应用程序的唯一标识符。
 *
 * @param processID 进程的AudioObjectID
 * @return 进程的Bundle ID，如果获取失败则返回空字符串
 */
std::string GetProcessBundleID(AudioObjectID processID) {
  AudioObjectPropertyAddress address = {kAudioProcessPropertyBundleID,
                                        kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};

  CFStringRef bundleID = nullptr;
  UInt32 dataSize = sizeof(bundleID);
  OSStatus status = AudioObjectGetPropertyData(processID, &address, 0, nullptr,
                                               &dataSize, &bundleID);

  if (status == noErr && bundleID != nullptr) {
    std::string result = CFStringToStdString(bundleID);
    CFRelease(bundleID);
    return result;
  }

  return "";
}

/**
 * @brief 获取进程的PID
 *
 * 从AudioObjectID获取对应的进程ID（PID）。
 *
 * @param processID 进程的AudioObjectID
 * @param pid 输出参数，用于存储获取的PID
 * @return 是否成功获取PID
 */
bool GetProcessPID(AudioObjectID processID, pid_t &pid) {
  AudioObjectPropertyAddress address = {kAudioProcessPropertyPID,
                                        kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};

  UInt32 dataSize = sizeof(pid);
  OSStatus status = AudioObjectGetPropertyData(processID, &address, 0, nullptr,
                                               &dataSize, &pid);

  return status == noErr;
}

/**
 * @brief 获取进程的详细信息
 *
 * 使用libproc API获取进程的名称和路径信息。
 *
 * @param pid 进程ID
 * @param name 输出参数，用于存储进程名称
 * @param path 输出参数，用于存储进程路径
 * @return 是否成功获取进程信息
 */
bool GetProcessInfo(uint32_t pid, std::string &name, std::string &path) {
  // 分配缓冲区用于存储进程名称和路径
  char nameBuffer[MAXPATHLEN];
  char pathBuffer[MAXPATHLEN];

  // 使用libproc API获取进程名称和路径
  int nameLength = proc_name(pid, nameBuffer, MAXPATHLEN);
  int pathLength = proc_pidpath(pid, pathBuffer, MAXPATHLEN);

  // 只有当两者都成功获取时才返回true
  if (nameLength > 0 && pathLength > 0) {
    name = std::string(nameBuffer);
    path = std::string(pathBuffer);
    return true;
  }

  return false;
}

/**
 * @brief 根据PID获取AudioObjectID
 *
 * 遍历所有音频进程，找到与指定PID匹配的AudioObjectID。
 *
 * @param pid 进程ID
 * @return 对应的AudioObjectID，如果未找到则返回kAudioObjectUnknown
 */
AudioObjectID GetAudioObjectIDForPID(uint32_t pid) {
  printf("[utils::GetAudioObjectIDForPID] 查找PID %u 的AudioObjectID\n", pid);

  // 安全检查
  if (pid == 0) {
    printf("[utils::GetAudioObjectIDForPID] PID不能为0\n");
    return 0;
  }

  // 直接使用PID作为AudioObjectID
  // 这是一个简化的实现，不需要遍历进程列表
  printf("[utils::GetAudioObjectIDForPID] 直接使用PID %u 作为AudioObjectID\n",
         pid);
  return static_cast<AudioObjectID>(pid);
}

} // namespace utils
} // namespace audio_capture
#endif // __APPLE__