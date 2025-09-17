#pragma once

#ifdef _WIN32

#include "../permission_manager.h"

/**
 * @file win_permission_manager.h
 * @brief Windows平台的音频录制权限管理
 *
 * 本文件定义了Windows平台上音频录制权限的管理类，
 * 使用Windows API来检查和请求系统音频录制权限。
 */

namespace permission_manager {

/**
 * @class WinPermissionManager
 * @brief Windows平台的音频录制权限管理类
 *
 * 该类提供了检查和请求Windows系统音频录制权限的功能。
 */
class WinPermissionManager : public PermissionManager {
public:
  /**
   * @brief 构造函数
   */
  WinPermissionManager();

  /**
   * @brief 析构函数
   */
  ~WinPermissionManager() override;

  /**
   * @brief 检查音频录制权限状态
   * @return 当前权限状态
   */
  PermissionStatus CheckPermission() override;

  /**
   * @brief 请求音频录制权限
   * @param callback 权限状态变化回调函数
   */
  void RequestPermission(PermissionCallback callback) override;
};
} // namespace permission_manager

#endif // _WIN32
