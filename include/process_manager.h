#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * @file process_manager.h
 * @brief 跨平台进程管理
 *
 * 提供获取播放音频的进程列表和进程过滤功能
 */

namespace process_manager {

/**
 * @struct IconData
 * @brief 图标数据结构体
 */
struct IconData {
  std::vector<uint8_t> data;
  std::string format;
  int width;
  int height;
};

/**
 * @struct ProcessInfo
 * @brief 进程信息结构体
 */
struct ProcessInfo {
  uint32_t pid;
  std::string name;
  std::string description;
  std::string path;
  IconData icon;
};

/**
 * @brief 获取正在播放音频的进程列表（已过滤当前应用进程）
 */
std::vector<ProcessInfo> GetProcessList();

} // namespace process_manager
