#pragma once

#include "audio_capture.h"

/**
 * @file permission_manager.h
 * @brief 跨平台音频录制权限管理接口
 *
 * 本文件定义了跨平台的音频录制权限管理接口，
 * 提供了检查和请求系统音频录制权限的功能。
 */

namespace permission_manager {

/**
 * @enum PermissionStatus
 * @brief 权限状态枚举
 *
 * 表示音频捕获权限的当前状态。
 */
enum class PermissionStatus {
  Unknown,   ///< 未知状态，通常表示用户尚未做出选择
  Denied,    ///< 已拒绝，用户明确拒绝了权限请求
  Authorized ///< 已授权，用户已同意授予权限
};

using PermissionCallback = std::function<void(PermissionStatus status)>;

/**
 * @class PermissionManager
 * @brief 音频录制权限管理接口类
 *
 * 该类提供了检查和请求系统音频录制权限的功能。
 * 它是一个接口类，具体实现由平台特定的子类提供。
 */
class PermissionManager {
public:
  /**
   * @brief 获取PermissionManager单例实例
   * @return PermissionManager单例实例的引用
   */
  static PermissionManager &GetInstance();

  /**
   * @brief 虚析构函数
   */
  virtual ~PermissionManager() = default;

  /**
   * @brief 检查音频录制权限状态
   * @return 当前权限状态
   */
  virtual PermissionStatus CheckPermission() = 0;

  /**
   * @brief 请求音频录制权限
   * @param callback 权限状态变化回调函数
   */

  virtual void RequestPermission(PermissionCallback callback) = 0;

protected:
  /**
   * @brief 构造函数 (受保护)
   */
  PermissionManager() = default;

private:
  // 禁止拷贝构造和赋值操作
  PermissionManager(const PermissionManager &) = delete;
  PermissionManager &operator=(const PermissionManager &) = delete;
};

} // namespace permission_manager