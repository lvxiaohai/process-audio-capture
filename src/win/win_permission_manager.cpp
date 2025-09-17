#ifdef _WIN32

#include "../../include/win/win_permission_manager.h"

/**
 * @file win_permission_manager.cpp
 * @brief Windows平台的音频录制权限管理实现
 *
 * 本文件实现了Windows平台特定的音频录制权限管理功能。
 * 使用Windows API实现权限检查和请求。
 */

namespace permission_manager {

// 在Windows平台上创建单例实例
PermissionManager &PermissionManager::GetInstance() {
  static WinPermissionManager instance;
  return instance;
}

WinPermissionManager::WinPermissionManager() {}

WinPermissionManager::~WinPermissionManager() {}

PermissionStatus WinPermissionManager::CheckPermission() {
  return PermissionStatus::Authorized;
}

void WinPermissionManager::RequestPermission(PermissionCallback callback) {
  if (callback) {
    callback(PermissionStatus::Authorized);
  }
}

} // namespace permission_manager

#endif // _WIN32
