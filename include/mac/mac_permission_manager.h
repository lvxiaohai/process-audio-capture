#pragma once

#ifdef __APPLE__

#include "../permission_manager.h"

/**
 * @file mac_permission_manager.h
 * @brief macOS平台的音频录制权限管理
 *
 * 本文件定义了macOS平台上音频录制权限的管理类，
 * 使用TCC (Transparency, Consent, and Control)
 * SPI来检查和请求系统音频录制权限。
 */

namespace permission_manager {

/**
 * @class MacPermissionManager
 * @brief macOS平台的音频录制权限管理类
 *
 * 该类提供了检查和请求macOS系统音频录制权限的功能。
 * 它使用TCC
 * SPI来实现更精确的权限管理，同时提供标准AVFoundation权限API作为备选。
 */
class MacPermissionManager : public PermissionManager {
public:
  /**
   * @brief 构造函数
   */
  MacPermissionManager();

  /**
   * @brief 析构函数
   */
  ~MacPermissionManager() override;

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

private:
  /**
   * @brief 更新权限状态
   */
  void UpdateStatus();

  // 当前权限状态
  PermissionStatus current_status_;

  // 权限回调函数
  PermissionCallback permission_callback_;

  // 是否启用TCC SPI
  bool enable_tcc_spi_;

  // 使用AVFoundation API检查权限
  PermissionStatus CheckPermissionAVFoundation();

  // 使用TCC SPI检查权限
  PermissionStatus CheckPermissionTCC();

  // 使用AVFoundation API请求权限
  void RequestPermissionAVFoundation(PermissionCallback callback);

  // 使用TCC SPI请求权限
  void RequestPermissionTCC(PermissionCallback callback);

  // 初始化TCC SPI
  bool InitializeTCCSPI();
};
} // namespace permission_manager

#endif // __APPLE__
