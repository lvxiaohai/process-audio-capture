#include "../../include/mac/mac_impl.h"
#include "../../include/mac/audio_tap.h"
#include "../../include/mac/process_list.h"
#include "../../include/mac/utils.h"

#ifdef __APPLE__
#include <AVFoundation/AVFoundation.h>
#include <AppKit/AppKit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <libproc.h>
#include <string>
#include <sys/param.h>
#include <vector>

/**
 * @file mac_impl.mm
 * @brief macOS平台的音频捕获实现
 *
 * 本文件实现了macOS平台特定的音频捕获功能。
 * 包括权限管理、进程列表获取、音频捕获启动和停止等功能。
 */

namespace audio_capture {

/**
 * @brief 构造函数
 */
MacAudioCapture::MacAudioCapture() {
  NSLog(@"[MacAudioCapture::MacAudioCapture] 构造函数");
}

/**
 * @brief 析构函数
 */
MacAudioCapture::~MacAudioCapture() {
  NSLog(@"[MacAudioCapture::~MacAudioCapture] 析构函数");
  if (capturing_) {
    StopCapture();
  }
}

/**
 * @brief 检查音频捕获权限
 * @return 权限状态
 */
PermissionStatus MacAudioCapture::CheckPermission() {
  NSLog(@"[MacAudioCapture::CheckPermission]");

  // 获取麦克风权限状态
  AVAuthorizationStatus status =
      [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];

  switch (status) {
  case AVAuthorizationStatusAuthorized:
    return PermissionStatus::Authorized;
  case AVAuthorizationStatusDenied:
  case AVAuthorizationStatusRestricted:
    return PermissionStatus::Denied;
  case AVAuthorizationStatusNotDetermined:
  default:
    return PermissionStatus::Unknown;
  }
}

/**
 * @brief 请求音频捕获权限
 * @param callback 权限请求结果回调
 */
void MacAudioCapture::RequestPermission(PermissionCallback callback) {
  NSLog(@"[MacAudioCapture::RequestPermission]");

  permission_callback_ = callback;

  // 请求麦克风权限
  [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                           completionHandler:^(BOOL granted) {
                             PermissionStatus status =
                                 granted ? PermissionStatus::Authorized
                                         : PermissionStatus::Denied;

                             // 在主线程调用回调
                             dispatch_async(dispatch_get_main_queue(), ^{
                               if (permission_callback_) {
                                 permission_callback_(status);
                               }
                             });
                           }];
}

/**
 * @brief 获取可捕获音频的进程列表
 * @return 进程信息列表
 */
std::vector<ProcessInfo> MacAudioCapture::GetProcessList() {
  NSLog(@"[MacAudioCapture::GetProcessList]");

  // 检查权限
  if (CheckPermission() != PermissionStatus::Authorized) {
    NSLog(@"[MacAudioCapture::GetProcessList] 没有音频捕获权限");
    return {};
  }

  // 使用process_list模块获取进程列表
  return process_list::GetProcessList();
}

/**
 * @brief 开始捕获指定进程的音频
 * @param pid 进程ID
 * @param callback 音频数据回调
 * @return 是否成功开始捕获
 */
bool MacAudioCapture::StartCapture(uint32_t pid, AudioDataCallback callback) {
  NSLog(@"[MacAudioCapture::StartCapture] PID: %u", pid);

  // 检查权限
  if (CheckPermission() != PermissionStatus::Authorized) {
    NSLog(@"[MacAudioCapture::StartCapture] 没有音频捕获权限");
    return false;
  }

  if (capturing_) {
    NSLog(@"[MacAudioCapture::StartCapture] 已经在捕获");
    return false;
  }

  callback_ = callback;

  // 创建音频捕获对象
  process_tap_ = std::make_unique<audio_tap::ProcessTap>(pid);

  // 初始化音频捕获
  if (!process_tap_->Initialize()) {
    NSLog(@"[MacAudioCapture::StartCapture] 初始化失败: %s",
          process_tap_->GetErrorMessage().c_str());
    return false;
  }

  // 开始捕获
  if (!process_tap_->Start(callback)) {
    NSLog(@"[MacAudioCapture::StartCapture] 开始捕获失败: %s",
          process_tap_->GetErrorMessage().c_str());
    return false;
  }

  capturing_ = true;
  NSLog(@"[MacAudioCapture::StartCapture] 成功开始捕获");
  return true;
}

/**
 * @brief 停止音频捕获
 * @return 是否成功停止捕获
 */
bool MacAudioCapture::StopCapture() {
  NSLog(@"[MacAudioCapture::StopCapture]");

  if (!capturing_) {
    NSLog(@"[MacAudioCapture::StopCapture] 当前没有在捕获");
    return false;
  }

  capturing_ = false;

  // 停止音频捕获
  if (process_tap_) {
    process_tap_->Stop();
    process_tap_.reset();
  }

  CleanUp();
  NSLog(@"[MacAudioCapture::StopCapture] 成功停止捕获");
  return true;
}

/**
 * @brief 清理资源
 */
void MacAudioCapture::CleanUp() { callback_ = nullptr; }

/**
 * @brief Hello World测试方法
 * @param input 输入字符串
 * @return 处理后的字符串
 */
std::string MacAudioCapture::HelloWorld(const std::string &input) {
  NSLog(@"[MacAudioCapture::HelloWorld] 输入: %s", input.c_str());

  return "Hello from macOS! You said: " + input;
}

/**
 * @brief 工厂函数 - 创建平台特定的实现
 * @return 平台特定的AudioCaptureImpl实例
 */
std::unique_ptr<AudioCaptureImpl> CreatePlatformAudioCapture() {
  NSLog(@"[CreatePlatformAudioCapture] 创建 macOS 音频捕获实例");
  return std::make_unique<MacAudioCapture>();
}

} // namespace audio_capture
#endif // __APPLE__