#ifdef _WIN32

#include "../../include/win/win_utils.h"
#include <algorithm>
#include <comdef.h>
#include <gdiplus.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <unordered_map>
#include <windows.h>

// 链接GDI+库
#pragma comment(lib, "gdiplus.lib")

/**
 * @file win_utils.cpp
 * @brief Windows平台工具函数实现
 *
 * 提供Windows平台下的进程管理、字符串转换、图标提取等功能的具体实现
 */

namespace audio_capture {
namespace win_utils {

//=============================================================================
// 内部辅助函数和变量
//=============================================================================

// GDI+ 初始化状态
static bool gdiplus_initialized = false;
static ULONG_PTR gdiplus_token = 0;

/**
 * @brief 内部函数：初始化GDI+库
 * @return 是否成功初始化
 */
static bool InitializeGDIPlus() {
  if (gdiplus_initialized) {
    return true;
  }

  Gdiplus::GdiplusStartupInput startup_input;
  Gdiplus::Status status =
      Gdiplus::GdiplusStartup(&gdiplus_token, &startup_input, NULL);

  if (status == Gdiplus::Ok) {
    gdiplus_initialized = true;
    return true;
  }

  return false;
}

/**
 * @brief 内部函数：清理GDI+库资源
 */
static void CleanupGDIPlus() {
  if (gdiplus_initialized) {
    Gdiplus::GdiplusShutdown(gdiplus_token);
    gdiplus_initialized = false;
  }
}

//=============================================================================
// 字符串转换工具实现
//=============================================================================

std::string WStringToString(const std::wstring &wstr) {
  if (wstr.empty()) {
    return "";
  }

  // 计算转换后的字节数
  int size_needed =
      WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
  if (size_needed <= 0) {
    return "";
  }

  // 执行转换
  std::string result(size_needed - 1, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size_needed,
                      NULL, NULL);
  return result;
}

std::wstring StringToWString(const std::string &str) {
  if (str.empty()) {
    return L"";
  }

  // 计算转换后的字符数
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
  if (size_needed <= 0) {
    return L"";
  }

  // 执行转换
  std::wstring result(size_needed - 1, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size_needed);
  return result;
}

//=============================================================================
// 内部辅助函数和类
//=============================================================================

namespace {
/**
 * @brief RAII风格的Windows句柄管理类
 *
 * 自动管理Windows句柄的生命周期，确保在对象销毁时正确释放句柄资源。
 * 这可以防止句柄泄漏，特别是在异常情况下。
 */
class HandleGuard {
public:
  /**
   * @brief 构造函数，接管句柄的所有权
   * @param handle 要管理的Windows句柄
   */
  explicit HandleGuard(HANDLE handle) : handle_(handle) {}

  /**
   * @brief 析构函数，自动释放句柄
   */
  ~HandleGuard() {
    if (handle_ && handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
    }
  }

  /**
   * @brief 获取原始句柄
   * @return 原始Windows句柄
   */
  HANDLE get() const { return handle_; }

  /**
   * @brief 检查句柄是否有效
   * @return 如果句柄有效返回true，否则返回false
   */
  bool valid() const { return handle_ && handle_ != INVALID_HANDLE_VALUE; }

  // 禁止复制构造和赋值，确保句柄所有权的唯一性
  HandleGuard(const HandleGuard &) = delete;
  HandleGuard &operator=(const HandleGuard &) = delete;

private:
  HANDLE handle_; ///< 被管理的Windows句柄
};

/**
 * @brief 尝试使用多种权限级别打开进程
 *
 * 按照从低到高的权限级别尝试打开进程，以最大化成功率。
 * 这种渐进式权限检查可以处理各种权限受限的进程。
 *
 * @param pid 目标进程ID
 * @param require_vm_access 是否需要虚拟内存访问权限
 * @return 成功时返回进程句柄，失败时返回nullptr
 */
HANDLE TryOpenProcessWithMultipleAccess(uint32_t pid,
                                        bool require_vm_access = false) {
  // 权限级别数组，按从低到高排列
  static const DWORD access_levels[] = {
      PROCESS_QUERY_LIMITED_INFORMATION,          // 最小权限，适用于大多数进程
      PROCESS_QUERY_INFORMATION,                  // 标准查询权限
      PROCESS_QUERY_INFORMATION | PROCESS_VM_READ // 包含虚拟内存读取权限
  };

  // 根据需求确定尝试的权限级别范围
  size_t end_index = require_vm_access ? 3 : 2;

  // 逐级尝试不同的权限级别
  for (size_t i = 0; i < end_index; ++i) {
    HANDLE process_handle = OpenProcess(access_levels[i], FALSE, pid);
    if (process_handle) {
      return process_handle;
    }
  }

  return nullptr;
}

/**
 * @brief 通过进程快照检查进程是否存在
 *
 * 当直接访问进程失败时，使用进程快照作为备用方案。
 * 这种方法不需要特殊权限，只要进程存在就能找到。
 *
 * @param pid 目标进程ID
 * @return 如果进程存在返回true，否则返回false
 */
bool CheckProcessExistsViaSnapshot(uint32_t pid) {
  // 创建系统进程快照
  HandleGuard snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!snapshot.valid()) {
    return false;
  }

  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);

  // 遍历进程快照查找目标进程
  if (Process32First(snapshot.get(), &pe32)) {
    do {
      if (pe32.th32ProcessID == pid) {
        return true;
      }
    } while (Process32Next(snapshot.get(), &pe32));
  }

  return false;
}
} // namespace

//=============================================================================
// 进程信息获取实现
//=============================================================================

std::string GetProcessName(uint32_t pid) {
  // 方案1: 通过进程句柄获取完整路径，然后提取文件名
  // 这种方法可以获得最准确的进程名称
  HandleGuard process_handle(TryOpenProcessWithMultipleAccess(pid, false));
  if (process_handle.valid()) {
    wchar_t process_name[MAX_PATH] = {0};
    DWORD size = MAX_PATH;

    if (QueryFullProcessImageNameW(process_handle.get(), 0, process_name,
                                   &size)) {
      std::wstring full_name(process_name);

      // 提取文件名部分（去除路径）
      size_t last_slash = full_name.find_last_of(L"\\");
      if (last_slash != std::wstring::npos) {
        full_name = full_name.substr(last_slash + 1);
      }

      // 移除.exe扩展名，返回纯净的进程名
      size_t last_dot = full_name.find_last_of(L".");
      if (last_dot != std::wstring::npos &&
          full_name.substr(last_dot) == L".exe") {
        full_name = full_name.substr(0, last_dot);
      }

      return WStringToString(full_name);
    }
  }

  // 方案2: 权限不足时，使用进程快照作为备用方案
  // 虽然信息有限，但不需要特殊权限
  HandleGuard snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (snapshot.valid()) {
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot.get(), &pe32)) {
      do {
        if (pe32.th32ProcessID == pid) {
          std::wstring exe_name = pe32.szExeFile;

          // 移除.exe扩展名
          size_t last_dot = exe_name.find_last_of(L".");
          if (last_dot != std::wstring::npos &&
              exe_name.substr(last_dot) == L".exe") {
            exe_name = exe_name.substr(0, last_dot);
          }

          return WStringToString(exe_name);
        }
      } while (Process32NextW(snapshot.get(), &pe32));
    }
  }

  return "";
}

std::string GetProcessPath(uint32_t pid) {
  // 方案1: 通过进程句柄获取完整的可执行文件路径
  // 这是获取进程路径的首选方法，可以得到完整的文件系统路径
  HandleGuard process_handle(TryOpenProcessWithMultipleAccess(pid, false));
  if (process_handle.valid()) {
    wchar_t process_path[MAX_PATH] = {0};
    DWORD size = MAX_PATH;

    if (QueryFullProcessImageNameW(process_handle.get(), 0, process_path,
                                   &size)) {
      return WStringToString(std::wstring(process_path));
    }
  }

  // 方案2: 权限不足时的降级处理
  // 注意：这里只能获取到进程名，而不是完整路径
  // 但对于某些权限受限的进程，这是唯一可行的方案
  HandleGuard snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (snapshot.valid()) {
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot.get(), &pe32)) {
      do {
        if (pe32.th32ProcessID == pid) {
          std::wstring exe_name = pe32.szExeFile;
          return WStringToString(
              exe_name); // 注意：这里返回的是文件名，不是完整路径
        }
      } while (Process32NextW(snapshot.get(), &pe32));
    }
  }

  return "";
}

std::string GetProcessDescription(uint32_t pid) {
  std::string exe_path = GetProcessPath(pid);
  if (exe_path.empty()) {
    return "";
  }

  std::wstring wexe_path = StringToWString(exe_path);

  // 获取文件版本信息
  DWORD version_info_size = GetFileVersionInfoSizeW(wexe_path.c_str(), NULL);
  if (version_info_size == 0) {
    return "";
  }

  std::vector<BYTE> version_info(version_info_size);
  if (!GetFileVersionInfoW(wexe_path.c_str(), 0, version_info_size,
                           version_info.data())) {
    return "";
  }

  // 尝试获取文件描述（首先尝试英文）
  LPVOID description_ptr = nullptr;
  UINT description_len = 0;

  if (VerQueryValueW(version_info.data(),
                     L"\\StringFileInfo\\040904b0\\FileDescription",
                     &description_ptr, &description_len) &&
      description_ptr) {
    return WStringToString(
        std::wstring(static_cast<wchar_t *>(description_ptr)));
  }

  return "";
}

std::string GetApplicationDisplayName(uint32_t pid) {
  std::string exe_path = GetProcessPath(pid);
  if (exe_path.empty()) {
    return GetProcessName(pid);
  }

  // 如果只获取到进程名（不是完整路径），直接处理
  if (exe_path.find('\\') == std::string::npos &&
      exe_path.find('/') == std::string::npos) {
    // 移除.exe扩展名
    size_t last_dot = exe_path.find_last_of(".");
    if (last_dot != std::string::npos && exe_path.substr(last_dot) == ".exe") {
      return exe_path.substr(0, last_dot);
    }
    return exe_path;
  }

  std::wstring wexe_path = StringToWString(exe_path);

  // 获取文件版本信息
  DWORD version_info_size = GetFileVersionInfoSizeW(wexe_path.c_str(), NULL);
  if (version_info_size == 0) {
    return GetProcessName(pid);
  }

  std::vector<BYTE> version_info(version_info_size);
  if (!GetFileVersionInfoW(wexe_path.c_str(), 0, version_info_size,
                           version_info.data())) {
    return GetProcessName(pid);
  }

  // 查询可用的语言代码页
  LPVOID lang_buffer = nullptr;
  UINT lang_len = 0;
  std::vector<std::wstring> available_lang_codes;

  if (VerQueryValueW(version_info.data(), L"\\VarFileInfo\\Translation",
                     &lang_buffer, &lang_len) &&
      lang_buffer) {
    DWORD *lang_data = static_cast<DWORD *>(lang_buffer);
    UINT lang_count = lang_len / sizeof(DWORD);

    for (UINT i = 0; i < lang_count; i++) {
      WORD lang_id = LOWORD(lang_data[i]);
      WORD code_page = HIWORD(lang_data[i]);

      wchar_t lang_code_str[32];
      swprintf_s(lang_code_str, L"\\StringFileInfo\\%04x%04x\\", lang_id,
                 code_page);
      available_lang_codes.push_back(std::wstring(lang_code_str));
    }
  }

  // 如果没有找到语言代码页，使用默认的（中文优先）
  if (available_lang_codes.empty()) {
    available_lang_codes.push_back(L"\\StringFileInfo\\080404b0\\"); // 简体中文
    available_lang_codes.push_back(L"\\StringFileInfo\\040904b0\\"); // 英文
  }

  std::string result;

  // 按中文优先、英文次之的顺序排列语言代码页
  std::vector<std::wstring> sorted_lang_codes;

  // 1. 优先添加中文相关的代码页
  for (const auto &lang_code : available_lang_codes) {
    if (lang_code.find(L"0804") != std::wstring::npos || // 简体中文
        lang_code.find(L"0404") != std::wstring::npos || // 繁体中文
        lang_code.find(L"0c04") != std::wstring::npos || // 香港中文
        lang_code.find(L"1004") != std::wstring::npos) { // 新加坡中文
      sorted_lang_codes.push_back(lang_code);
    }
  }

  // 2. 添加英文相关的代码页
  for (const auto &lang_code : available_lang_codes) {
    if (lang_code.find(L"0409") != std::wstring::npos || // 美式英文
        lang_code.find(L"0809") != std::wstring::npos || // 英式英文
        lang_code.find(L"0c09") != std::wstring::npos || // 澳大利亚英文
        lang_code.find(L"1009") != std::wstring::npos) { // 加拿大英文
      if (std::find(sorted_lang_codes.begin(), sorted_lang_codes.end(),
                    lang_code) == sorted_lang_codes.end()) {
        sorted_lang_codes.push_back(lang_code);
      }
    }
  }

  // 3. 添加其他语言代码页
  for (const auto &lang_code : available_lang_codes) {
    if (std::find(sorted_lang_codes.begin(), sorted_lang_codes.end(),
                  lang_code) == sorted_lang_codes.end()) {
      sorted_lang_codes.push_back(lang_code);
    }
  }

  // 按优先级尝试获取应用名称
  const wchar_t *field_names[] = {L"ProductName", L"FileDescription",
                                  L"InternalName"};

  for (const wchar_t *field_name : field_names) {
    for (const auto &lang_code : sorted_lang_codes) {
      std::wstring key = lang_code + field_name;

      LPVOID ptr = nullptr;
      UINT len = 0;

      if (VerQueryValueW(version_info.data(), key.c_str(), &ptr, &len) && ptr &&
          len > 1) {
        result = WStringToString(std::wstring(static_cast<wchar_t *>(ptr)));
        if (!result.empty()) {
          return result;
        }
      }
    }
  }

  // 后备方案：使用进程名
  return GetProcessName(pid);
}

//=============================================================================
// 进程状态检查实现
//=============================================================================

bool IsProcessExists(uint32_t pid) {
  // 方案1: 尝试直接访问进程来验证其存在性
  // 使用渐进式权限检查，从最小权限开始尝试
  HandleGuard process_handle(TryOpenProcessWithMultipleAccess(pid, false));
  if (process_handle.valid()) {
    return true;
  }

  // 方案2: 直接访问失败时，使用进程快照作为备用验证方案
  // 这种方法不需要特殊权限，是最后的检查手段
  return CheckProcessExistsViaSnapshot(pid);
}

bool HasProcessAccess(uint32_t pid) {
  // 检查是否能够访问进程（包括虚拟内存读取权限）
  // 这个函数用于确定是否有足够的权限来获取进程的详细信息
  // 注意：这里要求更高的权限级别，包括PROCESS_VM_READ
  HandleGuard process_handle(TryOpenProcessWithMultipleAccess(pid, true));
  return process_handle.valid();
}

//=============================================================================
// GetRealApplicationInfo 辅助函数
// 用于智能识别多进程应用的主进程，类似Windows任务管理器的实现
//=============================================================================

namespace {
/**
 * @brief 检查两个进程路径是否在同一目录
 *
 * @param path1 第一个进程的完整路径
 * @param path2 第二个进程的完整路径
 * @return true 如果在同一目录（不区分大小写）
 */
bool IsSameDirectory(const std::string &path1, const std::string &path2) {
  if (path1.empty() || path2.empty()) {
    return false;
  }

  size_t pos1 = path1.find_last_of("\\/");
  size_t pos2 = path2.find_last_of("\\/");

  if (pos1 == std::string::npos || pos2 == std::string::npos) {
    return false;
  }

  std::string dir1 = path1.substr(0, pos1);
  std::string dir2 = path2.substr(0, pos2);

  // Windows路径不区分大小写
  std::transform(dir1.begin(), dir1.end(), dir1.begin(), ::tolower);
  std::transform(dir2.begin(), dir2.end(), dir2.begin(), ::tolower);

  return dir1 == dir2;
}

/**
 * @brief 检查进程名是否像辅助进程
 *
 * 通过文件名中的关键词判断是否为辅助进程，如：
 * - KwService.exe (service)
 * - chrome.exe --type=renderer (renderer)
 *
 * @param exe_name 可执行文件名
 * @return true 如果是辅助进程
 */
bool IsAuxiliaryProcess(const std::string &exe_name) {
  std::string lower = exe_name;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  // 常见的辅助进程关键词
  static const char *keywords[] = {"service", "helper", "worker",  "renderer",
                                   "gpu",     "plugin", "utility", "crashpad"};

  for (const char *keyword : keywords) {
    if (lower.find(keyword) != std::string::npos) {
      return true;
    }
  }

  return false;
}

/**
 * @brief 一次性枚举所有窗口，建立PID到窗口的映射
 *
 * 性能优化：避免为每个进程重复枚举所有窗口
 * 只记录可见的顶层窗口（有标题栏的主窗口）
 *
 * @return PID到窗口句柄的映射表
 */
std::unordered_map<uint32_t, HWND> BuildProcessWindowMap() {
  std::unordered_map<uint32_t, HWND> pid_to_window;

  struct EnumData {
    std::unordered_map<uint32_t, HWND> *map;
  };

  EnumData data;
  data.map = &pid_to_window;

  EnumWindows(
      [](HWND hwnd, LPARAM lParam) -> BOOL {
        EnumData *data = reinterpret_cast<EnumData *>(lParam);

        // 只处理可见窗口
        if (!IsWindowVisible(hwnd)) {
          return TRUE;
        }

        DWORD window_pid = 0;
        GetWindowThreadProcessId(hwnd, &window_pid);

        // 只记录顶层窗口（没有父窗口）
        if (GetWindow(hwnd, GW_OWNER) == NULL) {
          LONG style = GetWindowLong(hwnd, GWL_STYLE);
          // 必须有标题栏（主窗口特征）
          if (style & WS_CAPTION) {
            // 每个PID只记录第一个找到的主窗口
            if (data->map->find(window_pid) == data->map->end()) {
              (*data->map)[window_pid] = hwnd;
            }
          }
        }

        return TRUE;
      },
      reinterpret_cast<LPARAM>(&data));

  return pid_to_window;
}

/**
 * @brief 查找同目录下最适合代表应用的主进程
 *
 * 对于多进程应用（如酷我音乐），通过以下策略找到主进程：
 * 1. 如果当前进程不是辅助进程，直接返回（快速路径）
 * 2. 在同目录下查找所有进程，按优先级评分：
 *    - 有可见窗口: +1000分
 *    - 非辅助进程: +500分
 *    - PID越小: 分数越高（先启动的通常是主进程）
 * 3. 找到第一个有窗口的非辅助进程立即返回（快速路径）
 *
 * @param pid 当前进程ID
 * @param current_path 当前进程的完整路径
 * @return 主进程的PID，如果找不到则返回原PID
 */
uint32_t FindMainProcessInDirectory(uint32_t pid,
                                    const std::string &current_path) {
  // 提取文件名
  size_t pos = current_path.find_last_of("\\/");
  std::string current_name =
      (pos != std::string::npos) ? current_path.substr(pos + 1) : current_path;

  // 快速路径：如果当前进程不是辅助进程，直接返回
  if (!IsAuxiliaryProcess(current_name)) {
    return pid;
  }

  // 只有辅助进程才需要查找主进程
  // 一次性枚举所有窗口，建立映射表（性能优化）
  auto pid_to_window = BuildProcessWindowMap();

  // 获取进程快照
  HandleGuard snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!snapshot.valid()) {
    return pid;
  }

  // 候选进程信息
  struct Candidate {
    uint32_t pid;
    std::string name;
    bool has_window;
    bool is_auxiliary;

    // 计算优先级分数
    int GetScore() const {
      int score = 0;
      if (has_window)
        score += 1000; // 有窗口最重要
      if (!is_auxiliary)
        score += 500;                        // 非辅助进程优先
      score -= static_cast<int>(pid / 1000); // PID越小越好
      return score;
    }
  };

  std::vector<Candidate> candidates;

  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);

  if (Process32First(snapshot.get(), &pe32)) {
    do {
      uint32_t candidate_pid = static_cast<uint32_t>(pe32.th32ProcessID);

      // 跳过自己
      if (candidate_pid == pid) {
        continue;
      }

      std::string candidate_path = GetProcessPath(candidate_pid);

      // 只考虑同目录的进程
      if (IsSameDirectory(current_path, candidate_path)) {
        Candidate c;
        c.pid = candidate_pid;

        // 提取文件名
        size_t pos = candidate_path.find_last_of("\\/");
        c.name = (pos != std::string::npos) ? candidate_path.substr(pos + 1)
                                            : candidate_path;

        // 使用预先建立的映射，避免重复枚举窗口
        c.has_window =
            (pid_to_window.find(candidate_pid) != pid_to_window.end());
        c.is_auxiliary = IsAuxiliaryProcess(c.name);

        candidates.push_back(c);

        // 快速路径：找到第一个有窗口的非辅助进程就立即返回
        if (c.has_window && !c.is_auxiliary) {
          return candidate_pid;
        }
      }
    } while (Process32Next(snapshot.get(), &pe32));
  }

  if (candidates.empty()) {
    return pid;
  }

  // 按分数排序，选择最佳候选
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate &a, const Candidate &b) {
              return a.GetScore() > b.GetScore();
            });

  return candidates[0].pid;
}
} // namespace

uint32_t GetRealApplicationInfo(uint32_t pid, std::string &out_name,
                                IconData &out_icon, std::string &out_path) {
  /**
   * 获取进程的真实应用信息（类似Windows任务管理器）
   *
   * 处理流程：
   * 1. 获取当前进程路径
   * 2. 查找同目录下的主进程（如果当前是辅助进程）
   * 3. 从主进程获取应用名称和图标
   *
   * 示例：
   * 输入: KwService.exe (PID: 24004)
   * 输出: "酷我音乐" + kwmusic.exe的图标 (PID: 17248)
   */

  // 步骤1: 获取当前进程路径
  std::string current_path = GetProcessPath(pid);
  if (current_path.empty()) {
    out_name = "Unknown Process";
    return pid;
  }

  // 步骤2: 查找主进程（智能识别多进程应用）
  uint32_t main_pid = FindMainProcessInDirectory(pid, current_path);
  std::string main_path = GetProcessPath(main_pid);

  if (main_path.empty()) {
    main_path = current_path;
    main_pid = pid;
  }

  out_path = main_path;
  std::wstring wpath = StringToWString(main_path);

  // 步骤3: 获取应用名称（优先从PE文件版本信息）
  // 读取ProductName字段，支持中文优先
  out_name = GetApplicationDisplayName(main_pid);

  // 步骤4: 获取应用图标
  // 优先使用 ExtractIconW 直接从 exe 提取，以保留完整的 Alpha 通道
  // SHGetFileInfoW 返回的系统缓存图标在某些情况下会丢失透明度信息
  out_icon = ExtractIconFromFile(main_path);

  // 步骤5: 后备方案 - 使用 Shell API
  if (out_icon.data.empty()) {
    SHFILEINFOW file_info = {0};
    DWORD_PTR result =
        SHGetFileInfoW(wpath.c_str(), 0, &file_info, sizeof(file_info),
                       SHGFI_ICON | SHGFI_LARGEICON);

    if (result && file_info.hIcon) {
      out_icon = ConvertIconToPNG(file_info.hIcon);
      DestroyIcon(file_info.hIcon);
    }
  }

  // 步骤6: 后备方案 - 使用进程名作为名称
  if (out_name.empty() || out_name == "Unknown Process") {
    out_name = GetProcessName(main_pid);
    if (out_name.empty()) {
      out_name = "Unknown Process";
    }
  }

  return main_pid;
}

//=============================================================================
// 图标提取实现
//=============================================================================

IconData GetProcessIcon(uint32_t pid) {
  std::string exe_path = GetProcessPath(pid);
  if (exe_path.empty()) {
    return {};
  }

  return ExtractIconFromFile(exe_path);
}

IconData ExtractIconFromFile(const std::string &exe_path) {
  if (!InitializeGDIPlus()) {
    return {};
  }

  std::wstring wexe_path = StringToWString(exe_path);

  // 提取大图标
  HICON hicon = ExtractIconW(GetModuleHandle(NULL), wexe_path.c_str(), 0);
  if (!hicon) {
    // 尝试从Shell获取图标
    SHFILEINFOW file_info = {0};
    if (SHGetFileInfoW(wexe_path.c_str(), 0, &file_info, sizeof(file_info),
                       SHGFI_ICON | SHGFI_LARGEICON)) {
      hicon = file_info.hIcon;
    }
  }

  if (!hicon) {
    return {};
  }

  IconData icon_data = ConvertIconToPNG(hicon);
  DestroyIcon(hicon);

  return icon_data;
}

IconData ConvertIconToPNG(HICON hicon) {
  if (!hicon || !InitializeGDIPlus()) {
    return {};
  }

  // 获取图标信息
  ICONINFO icon_info;
  if (!GetIconInfo(hicon, &icon_info)) {
    return {};
  }

  // 获取位图信息
  BITMAP bitmap;
  if (!GetObject(icon_info.hbmColor, sizeof(BITMAP), &bitmap)) {
    DeleteObject(icon_info.hbmColor);
    DeleteObject(icon_info.hbmMask);
    return {};
  }

  int width = bitmap.bmWidth;
  int height = bitmap.bmHeight;

  // 创建GDI+ Bitmap
  Gdiplus::Bitmap *gdi_bitmap = Gdiplus::Bitmap::FromHICON(hicon);
  if (!gdi_bitmap || gdi_bitmap->GetLastStatus() != Gdiplus::Ok) {
    DeleteObject(icon_info.hbmColor);
    DeleteObject(icon_info.hbmMask);
    if (gdi_bitmap)
      delete gdi_bitmap;
    return {};
  }

  // 创建内存流
  IStream *stream = nullptr;
  if (CreateStreamOnHGlobal(NULL, TRUE, &stream) != S_OK) {
    delete gdi_bitmap;
    DeleteObject(icon_info.hbmColor);
    DeleteObject(icon_info.hbmMask);
    return {};
  }

  // 获取PNG编码器
  CLSID png_clsid;
  if (CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}", &png_clsid) !=
      S_OK) {
    stream->Release();
    delete gdi_bitmap;
    DeleteObject(icon_info.hbmColor);
    DeleteObject(icon_info.hbmMask);
    return {};
  }

  // 保存为PNG
  if (gdi_bitmap->Save(stream, &png_clsid, NULL) != Gdiplus::Ok) {
    stream->Release();
    delete gdi_bitmap;
    DeleteObject(icon_info.hbmColor);
    DeleteObject(icon_info.hbmMask);
    return {};
  }

  // 获取流大小
  STATSTG stat;
  if (stream->Stat(&stat, STATFLAG_NONAME) != S_OK) {
    stream->Release();
    delete gdi_bitmap;
    DeleteObject(icon_info.hbmColor);
    DeleteObject(icon_info.hbmMask);
    return {};
  }

  // 读取数据
  LARGE_INTEGER seek_pos = {0};
  stream->Seek(seek_pos, STREAM_SEEK_SET, NULL);

  IconData icon_data;
  icon_data.format = "png";
  icon_data.width = width;
  icon_data.height = height;
  icon_data.data.resize(static_cast<size_t>(stat.cbSize.QuadPart));

  ULONG bytes_read = 0;
  if (stream->Read(icon_data.data.data(),
                   static_cast<ULONG>(icon_data.data.size()),
                   &bytes_read) != S_OK) {
    icon_data.data.clear();
  }

  // 清理资源
  stream->Release();
  delete gdi_bitmap;
  DeleteObject(icon_info.hbmColor);
  DeleteObject(icon_info.hbmMask);

  return icon_data;
}

//=============================================================================
// 系统功能实现
//=============================================================================

bool InitializeCOM() {
  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  // S_FALSE 表示 COM 已经初始化过了，这也是成功的情况
  return SUCCEEDED(hr) || hr == S_FALSE;
}

void CleanupCOM() { CoUninitialize(); }

} // namespace win_utils
} // namespace audio_capture

#endif // _WIN32
