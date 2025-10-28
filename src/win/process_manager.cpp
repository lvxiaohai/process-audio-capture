#ifdef _WIN32

#include "../../include/process_manager.h"
#include "../../include/win/win_utils.h"
#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <set>
#include <tlhelp32.h>
#include <unordered_set>
#include <windows.h>

using namespace audio_capture;

/**
 * @file process_manager.cpp
 * @brief Windows进程管理实现
 *
 * 通过Windows音频会话管理器获取正在播放音频的进程信息，
 * 并提供进程列表、图标提取等功能
 */

namespace process_manager {

//=============================================================================
// 内部数据结构和类定义
//=============================================================================

/**
 * @brief 音频会话信息结构
 *
 * 存储从Windows音频会话中获取的进程相关信息
 */
struct AudioSessionInfo {
  uint32_t process_id;                   ///< 进程ID
  std::string display_name;              ///< 显示名称
  std::string icon_path;                 ///< 图标路径
  bool is_active;                        ///< 是否活跃
  float volume;                          ///< 音量（0.0-1.0）
  bool is_muted;                         ///< 是否静音
  IAudioSessionControl *session_control; ///< 音频会话控制接口
};

/**
 * @brief Windows音频会话管理器
 *
 * 负责与Windows音频系统交互，获取当前活跃的音频会话信息
 * 支持枚举所有音频输出设备上的会话
 */
class AudioSessionManager {
public:
  AudioSessionManager() {}
  ~AudioSessionManager() { Cleanup(); }

  /**
   * @brief 初始化音频会话管理器
   * @return 是否成功初始化
   */
  bool Initialize() {
    if (initialized_) {
      return true;
    }

    // 创建设备枚举器
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  (void **)&device_enumerator_);
    if (FAILED(hr)) {
      return false;
    }

    initialized_ = true;
    return true;
  }

  /**
   * @brief 清理所有COM接口资源
   */
  void Cleanup() {
    if (device_enumerator_) {
      device_enumerator_->Release();
      device_enumerator_ = nullptr;
    }

    initialized_ = false;
  }

  /**
   * @brief 获取所有活跃的音频会话信息（从所有输出设备）
   * @return 音频会话信息列表
   */
  std::vector<AudioSessionInfo> GetActiveSessions() {
    std::vector<AudioSessionInfo> all_sessions;

    if (!initialized_) {
      return all_sessions;
    }

    // 1. 枚举所有音频输出设备
    IMMDeviceCollection *device_collection = nullptr;
    HRESULT hr = device_enumerator_->EnumAudioEndpoints(
        eRender, DEVICE_STATE_ACTIVE, &device_collection);
    
    if (FAILED(hr) || !device_collection) {
      return all_sessions;
    }

    UINT device_count = 0;
    hr = device_collection->GetCount(&device_count);
    if (FAILED(hr) || device_count == 0) {
      device_collection->Release();
      return all_sessions;
    }

    // 2. 遍历每个音频输出设备
    for (UINT i = 0; i < device_count; i++) {
      IMMDevice *device = nullptr;
      hr = device_collection->Item(i, &device);
      
      if (FAILED(hr) || !device) {
        continue;
      }

      // 3. 获取该设备的会话管理器
      IAudioSessionManager2 *session_manager = nullptr;
      hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                           NULL, (void **)&session_manager);
      
      if (FAILED(hr) || !session_manager) {
        device->Release();
        continue;
      }

      // 4. 获取会话枚举器
      IAudioSessionEnumerator *session_enumerator = nullptr;
      hr = session_manager->GetSessionEnumerator(&session_enumerator);
      
      if (FAILED(hr) || !session_enumerator) {
        session_manager->Release();
        device->Release();
        continue;
      }

      // 5. 枚举该设备上的所有会话
      int session_count = 0;
      hr = session_enumerator->GetCount(&session_count);
      
      if (SUCCEEDED(hr)) {
        for (int j = 0; j < session_count; j++) {
          IAudioSessionControl *session_control = nullptr;
          hr = session_enumerator->GetSession(j, &session_control);

          if (FAILED(hr) || !session_control) {
            continue;
          }

          // 检查会话是否活跃
          if (!IsSessionActive(session_control)) {
            session_control->Release();
            continue;
          }

          AudioSessionInfo info;
          info.process_id = GetSessionProcessId(session_control);
          info.display_name = GetSessionDisplayName(session_control);
          info.icon_path = GetSessionIconPath(session_control);
          info.is_active = true;
          info.session_control = session_control;

          // 获取音量信息
          ISimpleAudioVolume *volume_control = nullptr;
          hr = session_control->QueryInterface(__uuidof(ISimpleAudioVolume),
                                               (void **)&volume_control);
          if (SUCCEEDED(hr) && volume_control) {
            volume_control->GetMasterVolume(&info.volume);
            BOOL muted = FALSE;
            volume_control->GetMute(&muted);
            info.is_muted = (muted != FALSE);
            volume_control->Release();
          } else {
            info.volume = 1.0f;
            info.is_muted = false;
          }

          all_sessions.push_back(info);
        }
      }

      // 清理资源
      session_enumerator->Release();
      session_manager->Release();
      device->Release();
    }

    device_collection->Release();
    return all_sessions;
  }

private:
  bool initialized_{false};
  IMMDeviceEnumerator *device_enumerator_{nullptr};

  uint32_t GetSessionProcessId(IAudioSessionControl *session_control) {
    if (!session_control) {
      return 0;
    }

    IAudioSessionControl2 *session_control2 = nullptr;
    HRESULT hr = session_control->QueryInterface(
        __uuidof(IAudioSessionControl2), (void **)&session_control2);

    if (FAILED(hr) || !session_control2) {
      return 0;
    }

    DWORD process_id = 0;
    hr = session_control2->GetProcessId(&process_id);
    session_control2->Release();

    if (FAILED(hr)) {
      return 0;
    }

    return static_cast<uint32_t>(process_id);
  }

  std::string GetSessionDisplayName(IAudioSessionControl *session_control) {
    if (!session_control) {
      return "";
    }

    LPWSTR display_name = nullptr;
    HRESULT hr = session_control->GetDisplayName(&display_name);

    if (FAILED(hr) || !display_name) {
      return "";
    }

    std::string result = win_utils::WStringToString(display_name);
    CoTaskMemFree(display_name);
    return result;
  }

  std::string GetSessionIconPath(IAudioSessionControl *session_control) {
    if (!session_control) {
      return "";
    }

    LPWSTR icon_path = nullptr;
    HRESULT hr = session_control->GetIconPath(&icon_path);

    if (FAILED(hr) || !icon_path) {
      return "";
    }

    std::string result = win_utils::WStringToString(icon_path);
    CoTaskMemFree(icon_path);
    return result;
  }

  bool IsSessionActive(IAudioSessionControl *session_control) {
    if (!session_control) {
      return false;
    }

    AudioSessionState state;
    HRESULT hr = session_control->GetState(&state);

    if (FAILED(hr)) {
      return false;
    }

    return (state == AudioSessionStateActive);
  }
};

// 私有函数：获取与当前进程相关的所有进程ID
std::vector<uint32_t> GetSelfProcessIds() {
  std::vector<uint32_t> result;
  DWORD current_pid = GetCurrentProcessId();

  // 获取当前进程路径
  HANDLE process_handle = OpenProcess(
      PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, current_pid);
  if (!process_handle) {
    return result;
  }

  char target_buffer[MAX_PATH];
  DWORD target_size = MAX_PATH;

  if (!QueryFullProcessImageNameA(process_handle, 0, target_buffer,
                                  &target_size)) {
    CloseHandle(process_handle);
    return result;
  }
  CloseHandle(process_handle);

  std::string target_path(target_buffer);

  // 遍历所有进程，查找相同路径的进程
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    return result;

  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);

  if (Process32First(snapshot, &pe32)) {
    do {
      HANDLE proc_handle =
          OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE,
                      pe32.th32ProcessID);
      if (proc_handle) {
        char buffer[MAX_PATH];
        DWORD size = MAX_PATH;

        if (QueryFullProcessImageNameA(proc_handle, 0, buffer, &size)) {
          std::string process_path(buffer);
          if (process_path == target_path) {
            result.push_back(static_cast<uint32_t>(pe32.th32ProcessID));
          }
        }
        CloseHandle(proc_handle);
      }
    } while (Process32Next(snapshot, &pe32));
  }

  CloseHandle(snapshot);
  return result;
}

//=============================================================================
// 辅助函数实现
//=============================================================================

//=============================================================================
// 主要接口实现
//=============================================================================

/**
 * @brief 获取正在播放音频的进程列表
 *
 * 通过Windows音频会话管理器获取当前正在播放音频的进程信息，
 * 包括进程名称、路径、描述、图标等详细信息。
 * 
 * 重要特性：
 * - 支持枚举所有音频输出设备（包括默认和非默认设备）
 * - 能够获取使用任意输出设备的应用程序的音频会话
 *
 * 处理流程：
 * 1. 初始化Windows音频会话管理器
 * 2. 枚举系统中所有活跃的音频输出设备
 * 3. 遍历每个设备，获取其上的所有活跃音频会话
 * 4. 获取每个会话对应的进程信息
 * 5. 提取进程图标和友好名称
 * 6. 过滤掉当前应用自身的进程
 *
 * @return 进程信息列表，失败时返回空列表
 */
std::vector<ProcessInfo> GetProcessList() {
  std::vector<ProcessInfo> all_processes;
  std::unordered_set<uint32_t> processed_pids;

  try {
    // 1. 初始化音频会话管理器
    AudioSessionManager session_manager;
    if (!session_manager.Initialize()) {
      return all_processes;
    }

    // 2. 获取所有活跃的音频会话
    std::vector<AudioSessionInfo> sessions =
        session_manager.GetActiveSessions();

    // 3. 遍历每个音频会话，提取进程信息
    for (const auto &session : sessions) {
      uint32_t pid = session.process_id;

      // 预过滤：跳过无效的进程ID和已处理的进程
      // 这是最基本的过滤条件，可以快速排除明显无效的情况
      if (pid == 0 || processed_pids.find(pid) != processed_pids.end()) {
        continue;
      }

      // 权限验证：检查是否有足够权限访问目标进程
      // 注意：这里使用HasProcessAccess而不是IsProcessExists，因为：
      // 1. HasProcessAccess内部已经包含了进程存在性检查
      // 2. 我们需要确保有足够权限获取进程详细信息
      // 3. 避免重复的系统调用，提高性能
      if (!win_utils::HasProcessAccess(pid)) {
        continue;
      }

      ProcessInfo process;
      process.pid = pid;

      // 4. 安全地获取进程信息
      try {
        // 使用新的统一接口获取真实应用信息（类似任务管理器的实现）
        std::string app_name;
        IconData app_icon;
        std::string app_path;
        
        uint32_t representative_pid = win_utils::GetRealApplicationInfo(
          pid, app_name, app_icon, app_path);
        
        // 设置进程信息
        process.path = app_path;
        process.name = app_name;
        process.icon = app_icon;
        
        // 优先使用会话显示名称（如果有的话）
        if (!session.display_name.empty()) {
          process.name = session.display_name;
        }
        
        // 如果没有图标，尝试从会话路径获取
        if (process.icon.data.empty() && !session.icon_path.empty()) {
          process.icon = win_utils::ExtractIconFromFile(session.icon_path);
        }
        
        // 获取进程描述（从代表进程获取）
        process.description = win_utils::GetProcessDescription(representative_pid);
        if (process.description.empty()) {
          process.description = "PID: " + std::to_string(pid);
        }

        // 设置空图标信息
        if (process.icon.data.empty()) {
          process.icon.format = "png";
          process.icon.width = 0;
          process.icon.height = 0;
        }

        all_processes.push_back(process);
        processed_pids.insert(pid);

      } catch (const std::exception &) {
        // 忽略单个进程的异常，继续处理其他进程
      } catch (...) {
        // 忽略单个进程的异常，继续处理其他进程
      }
    }

  } catch (const std::exception &) {
    // 忽略异常，返回已获取的进程列表
  } catch (...) {
    // 忽略异常，返回已获取的进程列表
  }

  // 4. 过滤掉当前应用的相关进程
  try {
    std::vector<uint32_t> self_pids = GetSelfProcessIds();
    std::set<uint32_t> self_pid_set(self_pids.begin(), self_pids.end());

    std::vector<ProcessInfo> filtered_processes;
    for (const auto &process : all_processes) {
      if (self_pid_set.find(process.pid) == self_pid_set.end()) {
        filtered_processes.push_back(process);
      }
    }

    return filtered_processes;

  } catch (const std::exception &) {
    // 过滤失败时返回未过滤的列表
    return all_processes;
  } catch (...) {
    // 过滤失败时返回未过滤的列表
    return all_processes;
  }
}

} // namespace process_manager

#endif // _WIN32
