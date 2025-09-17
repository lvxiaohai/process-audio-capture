#pragma once

#ifdef _WIN32

#include "../process_manager.h"
#include <string>
#include <windows.h>

/**
 * @file win_utils.h
 * @brief Windows平台工具函数
 *
 * 提供Windows平台下的进程管理、字符串转换、图标提取等功能
 */

namespace audio_capture {
namespace win_utils {

// 类型别名，简化代码
using IconData = process_manager::IconData;

//=============================================================================
// 字符串转换工具
//=============================================================================

/**
 * @brief 将宽字符串转换为UTF-8字符串
 * @param wstr 宽字符串
 * @return UTF-8字符串，转换失败时返回空字符串
 */
std::string WStringToString(const std::wstring &wstr);

/**
 * @brief 将UTF-8字符串转换为宽字符串
 * @param str UTF-8字符串
 * @return 宽字符串，转换失败时返回空字符串
 */
std::wstring StringToWString(const std::string &str);

//=============================================================================
// 进程信息获取
//=============================================================================

/**
 * @brief 获取进程的可执行文件名（不含路径和扩展名）
 * @param pid 进程ID
 * @return 进程名称，失败时返回空字符串
 */
std::string GetProcessName(uint32_t pid);

/**
 * @brief 获取进程的完整可执行文件路径
 * @param pid 进程ID
 * @return 进程路径，失败时返回空字符串
 */
std::string GetProcessPath(uint32_t pid);

/**
 * @brief 获取进程的文件描述信息
 * @param pid 进程ID
 * @return 进程描述，失败时返回空字符串
 */
std::string GetProcessDescription(uint32_t pid);

/**
 * @brief 获取应用程序的友好显示名称
 *
 * 按优先级尝试获取：
 * 1. 产品名称 (ProductName) - 中文优先，英文次之
 * 2. 文件描述 (FileDescription)
 * 3. 内部名称 (InternalName)
 * 4. 进程名称 (后备方案)
 *
 * @param pid 进程ID
 * @return 友好的应用名称
 */
std::string GetApplicationDisplayName(uint32_t pid);

//=============================================================================
// 进程状态检查
//=============================================================================

/**
 * @brief 检查指定进程是否存在
 * @param pid 进程ID
 * @return 进程是否存在
 */
bool IsProcessExists(uint32_t pid);

/**
 * @brief 检查是否有权限访问指定进程
 * @param pid 进程ID
 * @return 是否有访问权限
 */
bool HasProcessAccess(uint32_t pid);

//=============================================================================
// 图标提取
//=============================================================================

/**
 * @brief 根据进程ID获取进程图标
 * @param pid 进程ID
 * @return PNG格式的图标数据，失败时返回空的IconData
 */
IconData GetProcessIcon(uint32_t pid);

/**
 * @brief 从指定文件路径提取图标
 * @param exe_path 可执行文件路径
 * @return PNG格式的图标数据，失败时返回空的IconData
 */
IconData ExtractIconFromFile(const std::string &exe_path);

/**
 * @brief 将Windows图标句柄转换为PNG数据
 * @param hicon 图标句柄
 * @return PNG格式的图标数据，失败时返回空的IconData
 */
IconData ConvertIconToPNG(HICON hicon);

//=============================================================================
// 系统功能
//=============================================================================

/**
 * @brief 初始化COM库（用于某些Windows API）
 * @return 是否成功初始化
 */
bool InitializeCOM();

/**
 * @brief 清理COM库资源
 */
void CleanupCOM();

} // namespace win_utils
} // namespace audio_capture

#endif // _WIN32
