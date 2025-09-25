#ifdef __APPLE__

#include "../../include/mac/mac_permission_manager.h"
#include <AppKit/AppKit.h>
#include <dlfcn.h>
#include <os/log.h>

/**
 * @file mac_permission_manager.mm
 * @brief macOS平台的音频录制权限管理实现
 *
 * 本文件实现了macOS平台特定的音频录制权限管理功能，使用TCC SPI。
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

// TCC SPI静态函数指针
static TCCAccessPreflightFuncType s_tcc_preflight = nullptr;
static TCCAccessRequestFuncType s_tcc_request = nullptr;

// TCC Framework句柄
static void *s_tcc_handle = nullptr;

// 通知观察者对象
static id s_notification_observer = nil;

// 获取单例实例
PermissionManager &PermissionManager::GetInstance() {
  static MacPermissionManager instance;
  return static_cast<PermissionManager &>(instance);
}

MacPermissionManager::MacPermissionManager()
    : current_status_(PermissionStatus::Unknown),
      logger_(os_log_create(kAppSubsystem, kAppCategory)) {

#ifdef ENABLE_TCC_SPI
  // 注册通知和更新状态，不在构造函数中检查SPI初始化
  s_notification_observer = [[NSNotificationCenter defaultCenter]
      addObserverForName:NSApplicationDidBecomeActiveNotification
                  object:nil
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification *notification) {
                UpdateStatus();
              }];

  // 初始化时更新状态
  UpdateStatus();
#else
  // 如果未启用TCC SPI，默认授权
  current_status_ = PermissionStatus::Authorized;
#endif
}

MacPermissionManager::~MacPermissionManager() {
  os_log_debug(logger_, "%s", __FUNCTION__);

  // 移除通知观察者
  if (s_notification_observer) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:s_notification_observer];
    s_notification_observer = nil;
  }

  // 清理TCC SPI资源
  if (s_tcc_handle) {
    dlclose(s_tcc_handle);
    s_tcc_handle = nullptr;
    s_tcc_preflight = nullptr;
    s_tcc_request = nullptr;
  }
}

bool MacPermissionManager::InitializeTCCSPI() {
#ifdef ENABLE_TCC_SPI
  // 检查是否已经初始化过
  if (s_tcc_handle && s_tcc_preflight && s_tcc_request) {
    return true;
  }

  // 清理之前可能的不完整初始化
  if (s_tcc_handle) {
    dlclose(s_tcc_handle);
    s_tcc_handle = nullptr;
    s_tcc_preflight = nullptr;
    s_tcc_request = nullptr;
  }

  // 尝试加载TCC框架
  const char *tcc_path =
      "/System/Library/PrivateFrameworks/TCC.framework/Versions/A/TCC";
  s_tcc_handle = dlopen(tcc_path, RTLD_NOW);

  if (!s_tcc_handle) {
    const char *error = dlerror();
    os_log_fault(logger_, "dlopen failed: %{public}s",
                 error ? error : "unknown error");
    return false;
  }

  // 获取TCCAccessPreflight函数指针
  void *preflight_symbol = dlsym(s_tcc_handle, "TCCAccessPreflight");
  if (!preflight_symbol) {
    const char *error = dlerror();
    os_log_fault(logger_, "Couldn't find symbol: %{public}s",
                 error ? error : "unknown error");
    dlclose(s_tcc_handle);
    s_tcc_handle = nullptr;
    return false;
  }
  s_tcc_preflight = (TCCAccessPreflightFuncType)preflight_symbol;

  // 获取TCCAccessRequest函数指针
  void *request_symbol = dlsym(s_tcc_handle, "TCCAccessRequest");
  if (!request_symbol) {
    const char *error = dlerror();
    os_log_fault(logger_, "Couldn't find symbol: %{public}s",
                 error ? error : "unknown error");
    dlclose(s_tcc_handle);
    s_tcc_handle = nullptr;
    s_tcc_preflight = nullptr;
    return false;
  }
  s_tcc_request = (TCCAccessRequestFuncType)request_symbol;

  return true;
#else
  return false;
#endif
}

PermissionStatus MacPermissionManager::CheckPermission() {
#ifdef ENABLE_TCC_SPI
  // 使用TCC SPI检查权限
  return CheckPermissionTCC();
#else
  // 编译时未启用TCC SPI则默认授权
  return PermissionStatus::Authorized;
#endif
}

PermissionStatus MacPermissionManager::CheckPermissionTCC() {
#ifdef ENABLE_TCC_SPI
  // 使用时才初始化SPI（懒加载）
  if (!s_tcc_preflight) {
    InitializeTCCSPI();
  }

  if (!s_tcc_preflight) {
    os_log_fault(logger_, "Preflight SPI missing");
    return PermissionStatus::Unknown;
  }

  // 使用TCC SPI检查权限
  int result = s_tcc_preflight(kTCCServiceAudioCapture, nullptr);

  // TCC状态映射逻辑：0=授权，1=拒绝，其他=未确定
  if (result == 0) {
    // 0 = authorized
    return PermissionStatus::Authorized;
  } else if (result == 1) {
    // 1 = denied
    return PermissionStatus::Denied;
  } else {
    // 其他值 (通常是2) = unknown/not determined
    return PermissionStatus::Unknown;
  }
#else
  // 编译时未启用TCC SPI
  return PermissionStatus::Authorized;
#endif
}

void MacPermissionManager::UpdateStatus() {
  os_log_debug(logger_, "%s", __FUNCTION__);

  PermissionStatus old_status = current_status_;

#ifdef ENABLE_TCC_SPI
  // 调用CheckPermissionTCC，由其处理SPI初始化
  current_status_ = CheckPermissionTCC();
#else
  // 编译时未启用TCC SPI则保持授权状态
  current_status_ = PermissionStatus::Authorized;
#endif

  // 只在状态发生变化时记录信息级别日志
  if (old_status != current_status_) {
    os_log_info(logger_, "Status changed: %d -> %d", (int)old_status,
                (int)current_status_);
  }
}

void MacPermissionManager::RequestPermission(PermissionCallback callback) {
  permission_callback_ = callback;

#ifdef ENABLE_TCC_SPI
  // 调用TCC请求方法
  RequestPermissionTCC(callback);
#else
  // 编译时未启用TCC SPI则直接返回授权
  if (callback) {
    callback(PermissionStatus::Authorized);
  }
#endif
}

void MacPermissionManager::RequestPermissionTCC(PermissionCallback callback) {
#ifdef ENABLE_TCC_SPI
  os_log_debug(logger_, "%s", __FUNCTION__);

  // 使用时才初始化SPI（懒加载）
  if (!s_tcc_request) {
    InitializeTCCSPI();
  }

  if (!s_tcc_request) {
    os_log_fault(logger_, "Request SPI missing");
    if (callback) {
      callback(PermissionStatus::Unknown);
    }
    return;
  }

  // 记录请求开始
  os_log_debug(logger_, "开始使用 TCC SPI 请求权限");

  // 检查当前权限状态
  if (s_tcc_preflight) {
    int currentStatus = s_tcc_preflight(kTCCServiceAudioCapture, nullptr);
    os_log_debug(logger_, "请求前 TCC 状态: %d", currentStatus);

    // 如果已经有权限，直接返回
    if (currentStatus == 0) {
      os_log_info(logger_, "权限已授权，无需重复请求");
      current_status_ = PermissionStatus::Authorized;
      if (callback) {
        dispatch_async(dispatch_get_main_queue(), ^{
          callback(PermissionStatus::Authorized);
        });
      }
      return;
    }
  }

  // 使用TCC SPI请求权限，不使用options参数
  s_tcc_request(kTCCServiceAudioCapture, nullptr, ^(bool granted) {
    os_log_info(logger_, "TCC 权限请求完成，结果: %{public}d", granted);

    // 确定最终状态
    PermissionStatus final_status;
    if (granted) {
      final_status = PermissionStatus::Authorized;
    } else {
      // 再次检查实际状态，因为用户可能取消了对话框而不是拒绝
      if (s_tcc_preflight) {
        int newStatus = s_tcc_preflight(kTCCServiceAudioCapture, nullptr);
        os_log_debug(logger_, "请求后 TCC 状态: %d", newStatus);

        if (newStatus == 0) {
          final_status = PermissionStatus::Authorized;
        } else if (newStatus == 1) {
          final_status = PermissionStatus::Denied;
        } else {
          final_status = PermissionStatus::Unknown;
        }
      } else {
        final_status = PermissionStatus::Denied;
      }
    }

    // 更新内部状态
    current_status_ = final_status;
    os_log_info(logger_, "权限请求完成，最终状态: %d", (int)final_status);

    // 在主线程中调用回调
    dispatch_async(dispatch_get_main_queue(), ^{
      if (callback) {
        callback(final_status);
      }
    });
  });
#else
  // 编译时未启用TCC SPI
  if (callback) {
    callback(PermissionStatus::Authorized);
  }
#endif
}

} // namespace permission_manager

#endif // __APPLE__
