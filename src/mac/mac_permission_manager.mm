#ifdef __APPLE__

#include "../../include/mac/mac_permission_manager.h"
#include <AVFoundation/AVFoundation.h>
#include <AppKit/AppKit.h>
#include <dlfcn.h>
#include <os/log.h>

/**
 * @file mac_permission_manager.mm
 * @brief macOS平台的音频录制权限管理实现
 *
 * 本文件实现了macOS平台特定的音频录制权限管理功能。
 * 使用TCC SPI和AVFoundation API实现权限检查和请求。
 */

namespace permission_manager {

// TCC服务名称
static const CFStringRef kTCCServiceAudioCapture =
    CFSTR("kTCCServiceAudioCapture");

// 日志子系统和类别名称
static const char *kAppSubsystem = "com.electron.audiocapture";
static const char *kAppCategory = "MacPermissionManager";

// TCC SPI函数类型定义
typedef int (*TCCAccessPreflightFuncType)(CFStringRef service,
                                          CFDictionaryRef options);
typedef void (*TCCAccessRequestFuncType)(CFStringRef service,
                                         CFDictionaryRef options,
                                         void (^callback)(bool granted));

// TCC SPI函数指针
static TCCAccessPreflightFuncType TCCAccessPreflight = nullptr;
static TCCAccessRequestFuncType TCCAccessRequest = nullptr;

// TCC Framework句柄
static void *g_tcc_handle = nullptr;

// 获取单例实例
PermissionManager &PermissionManager::GetInstance() {
  static MacPermissionManager instance;
  return instance;
}

MacPermissionManager::MacPermissionManager()
    : current_status_(PermissionStatus::Unknown), enable_tcc_spi_(false) {

  // 初始化TCC SPI
#ifdef ENABLE_TCC_SPI
  enable_tcc_spi_ = InitializeTCCSPI();

  if (enable_tcc_spi_) {
    // 注册应用程序激活通知，以便更新权限状态
    [[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationDidBecomeActiveNotification
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification *notification) {
                  UpdateStatus();
                }];

    // 初始化时更新状态
    UpdateStatus();
  } else {
    // 如果TCC SPI不可用，则使用AVFoundation API
    current_status_ = CheckPermissionAVFoundation();
  }
#else
  current_status_ = CheckPermissionAVFoundation();
#endif
}

MacPermissionManager::~MacPermissionManager() {
  // 移除通知观察者
#ifdef ENABLE_TCC_SPI
  if (enable_tcc_spi_) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:[NSApp delegate]
                  name:NSApplicationDidBecomeActiveNotification
                object:nil];
  }
#endif
}

bool MacPermissionManager::InitializeTCCSPI() {
#ifdef ENABLE_TCC_SPI
  // 尝试加载TCC框架
  const char *tcc_path =
      "/System/Library/PrivateFrameworks/TCC.framework/Versions/A/TCC";
  g_tcc_handle = dlopen(tcc_path, RTLD_NOW);

  if (!g_tcc_handle) {
    os_log_fault(os_log_create(kAppSubsystem, kAppCategory),
                 "无法加载TCC框架: %s", dlerror());
    return false;
  }

  // 获取TCCAccessPreflight函数指针
  TCCAccessPreflight =
      (TCCAccessPreflightFuncType)dlsym(g_tcc_handle, "TCCAccessPreflight");
  if (!TCCAccessPreflight) {
    os_log_fault(os_log_create(kAppSubsystem, kAppCategory),
                 "无法找到TCCAccessPreflight函数: %s", dlerror());
    dlclose(g_tcc_handle);
    g_tcc_handle = nullptr;
    return false;
  }

  // 获取TCCAccessRequest函数指针
  TCCAccessRequest =
      (TCCAccessRequestFuncType)dlsym(g_tcc_handle, "TCCAccessRequest");
  if (!TCCAccessRequest) {
    os_log_fault(os_log_create(kAppSubsystem, kAppCategory),
                 "无法找到TCCAccessRequest函数: %s", dlerror());
    dlclose(g_tcc_handle);
    g_tcc_handle = nullptr;
    return false;
  }

  return true;
#else
  return false;
#endif
}

PermissionStatus MacPermissionManager::CheckPermission() {
#ifdef ENABLE_TCC_SPI
  if (enable_tcc_spi_) {
    return CheckPermissionTCC();
  }
#endif

  return CheckPermissionAVFoundation();
}

PermissionStatus MacPermissionManager::CheckPermissionAVFoundation() {
  // 使用AVFoundation API检查权限
  AVAuthorizationStatus status =
      [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];

  os_log_info(os_log_create(kAppSubsystem, kAppCategory),
              "CheckPermissionAVFoundation: AVAuthorizationStatus = %d",
              (int)status);

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

PermissionStatus MacPermissionManager::CheckPermissionTCC() {
#ifdef ENABLE_TCC_SPI
  if (!enable_tcc_spi_ || !TCCAccessPreflight) {
    return CheckPermissionAVFoundation();
  }

  // 使用TCC SPI检查权限
  int result = TCCAccessPreflight(kTCCServiceAudioCapture, nullptr);

  if (result == 0) {
    return PermissionStatus::Authorized;
  } else if (result == 1) {
    return PermissionStatus::Denied;
  } else {
    return PermissionStatus::Unknown;
  }
#else
  return CheckPermissionAVFoundation();
#endif
}

void MacPermissionManager::UpdateStatus() {
#ifdef ENABLE_TCC_SPI
  if (!enable_tcc_spi_) {
    current_status_ = CheckPermissionAVFoundation();
    os_log_info(os_log_create(kAppSubsystem, kAppCategory),
                "UpdateStatus: 使用AVFoundation, current_status_ = %d",
                (int)current_status_);
    return;
  }

  current_status_ = CheckPermissionTCC();
  os_log_info(os_log_create(kAppSubsystem, kAppCategory),
              "UpdateStatus: 使用TCC SPI, current_status_ = %d",
              (int)current_status_);
#else
  current_status_ = CheckPermissionAVFoundation();
  os_log_info(os_log_create(kAppSubsystem, kAppCategory),
              "UpdateStatus: 使用AVFoundation, current_status_ = %d",
              (int)current_status_);
#endif
}

void MacPermissionManager::RequestPermission(PermissionCallback callback) {
  permission_callback_ = callback;

#ifdef ENABLE_TCC_SPI
  if (enable_tcc_spi_) {
    RequestPermissionTCC(callback);
    return;
  }
#endif

  RequestPermissionAVFoundation(callback);
}

void MacPermissionManager::RequestPermissionAVFoundation(
    PermissionCallback callback) {
  // 记录请求开始
  os_log_debug(os_log_create(kAppSubsystem, kAppCategory),
               "开始请求AVFoundation权限");

  // 确保Info.plist中有NSMicrophoneUsageDescription
  NSBundle *mainBundle = [NSBundle mainBundle];
  NSString *usageDescription =
      [mainBundle objectForInfoDictionaryKey:@"NSMicrophoneUsageDescription"];
  if (!usageDescription) {
    os_log_error(os_log_create(kAppSubsystem, kAppCategory),
                 "缺少NSMicrophoneUsageDescription，权限请求可能会失败");
  }

  // 请求麦克风权限
  [AVCaptureDevice
      requestAccessForMediaType:AVMediaTypeAudio
              completionHandler:^(BOOL granted) {
                os_log_info(os_log_create(kAppSubsystem, kAppCategory),
                            "AVFoundation权限请求完成，结果: %{public}d",
                            granted);

                // 再次检查权限状态
                AVAuthorizationStatus newStatus = [AVCaptureDevice
                    authorizationStatusForMediaType:AVMediaTypeAudio];
                os_log_info(os_log_create(kAppSubsystem, kAppCategory),
                            "RequestPermissionAVFoundation: 请求后状态 = %d",
                            (int)newStatus);

                PermissionStatus status = granted ? PermissionStatus::Authorized
                                                  : PermissionStatus::Denied;

                current_status_ = status;
                os_log_info(
                    os_log_create(kAppSubsystem, kAppCategory),
                    "RequestPermissionAVFoundation: 设置current_status_ = %d",
                    (int)current_status_);

                // 在主线程中调用回调
                dispatch_async(dispatch_get_main_queue(), ^{
                  if (callback) {
                    callback(status);
                  }
                });
              }];
}

void MacPermissionManager::RequestPermissionTCC(PermissionCallback callback) {
#ifdef ENABLE_TCC_SPI
  if (!enable_tcc_spi_ || !TCCAccessRequest) {
    RequestPermissionAVFoundation(callback);
    return;
  }

  // 记录请求开始
  os_log_debug(os_log_create(kAppSubsystem, kAppCategory), "开始请求TCC权限");

  // 检查当前权限状态
  int currentStatus = -1;
  if (TCCAccessPreflight) {
    currentStatus = TCCAccessPreflight(kTCCServiceAudioCapture, nullptr);
    os_log_info(os_log_create(kAppSubsystem, kAppCategory),
                "RequestPermissionTCC: 请求前状态 = %d", currentStatus);
  }

  // 创建选项字典，指定应用需要访问麦克风
  CFMutableDictionaryRef options = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);

  // 添加Info.plist中的权限描述
  NSBundle *mainBundle = [NSBundle mainBundle];
  NSString *usageDescription =
      [mainBundle objectForInfoDictionaryKey:@"NSMicrophoneUsageDescription"];
  if (usageDescription) {
    CFDictionaryAddValue(options, CFSTR("prompt-description"),
                         (__bridge CFStringRef)usageDescription);
  }

  // 使用TCC SPI请求权限
  TCCAccessRequest(kTCCServiceAudioCapture, options, ^(bool granted) {
    // 释放选项字典
    CFRelease(options);

    os_log_info(os_log_create(kAppSubsystem, kAppCategory),
                "TCC权限请求完成，结果: %{public}d", granted);

    // 再次检查权限状态
    int newStatus = -1;
    if (TCCAccessPreflight) {
      newStatus = TCCAccessPreflight(kTCCServiceAudioCapture, nullptr);
      os_log_info(os_log_create(kAppSubsystem, kAppCategory),
                  "RequestPermissionTCC: 请求后状态 = %d", newStatus);

      // 修复：即使TCCAccessRequest回调granted=false，但如果TCCAccessPreflight仍然返回2（未确定）
      // 这可能是因为用户关闭了权限对话框但没有明确拒绝，或者系统状态尚未完全更新
      if (newStatus == 2 && !granted) {
        current_status_ = PermissionStatus::Unknown;
        os_log_info(os_log_create(kAppSubsystem, kAppCategory),
                    "RequestPermissionTCC: 检测到状态不一致，设置为Unknown");

        // 在主线程中调用回调
        dispatch_async(dispatch_get_main_queue(), ^{
          if (callback) {
            callback(PermissionStatus::Unknown);
          }
        });
        return;
      }
    }

    PermissionStatus status =
        granted ? PermissionStatus::Authorized : PermissionStatus::Denied;
    current_status_ = status;
    os_log_info(os_log_create(kAppSubsystem, kAppCategory),
                "RequestPermissionTCC: 设置current_status_ = %d",
                (int)current_status_);

    // 在主线程中调用回调
    dispatch_async(dispatch_get_main_queue(), ^{
      if (callback) {
        callback(status);
      }
    });
  });
#else
  RequestPermissionAVFoundation(callback);
#endif
}

} // namespace permission_manager

#endif // __APPLE__
