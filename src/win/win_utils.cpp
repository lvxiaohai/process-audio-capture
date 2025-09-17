#ifdef _WIN32

#include "../../include/win/win_utils.h"
#include <algorithm>
#include <comdef.h>
#include <gdiplus.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
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
// 进程信息获取实现
//=============================================================================

std::string GetProcessName(uint32_t pid) {
  // 打开进程句柄
  HANDLE process_handle =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (!process_handle) {
    return "";
  }

  // 获取完整路径
  wchar_t process_name[MAX_PATH] = {0};
  DWORD size = MAX_PATH;

  if (QueryFullProcessImageNameW(process_handle, 0, process_name, &size)) {
    CloseHandle(process_handle);

    // 提取文件名（不包含路径）
    std::wstring full_name(process_name);
    size_t last_slash = full_name.find_last_of(L"\\");
    if (last_slash != std::wstring::npos) {
      full_name = full_name.substr(last_slash + 1);
    }

    // 移除.exe扩展名
    size_t last_dot = full_name.find_last_of(L".");
    if (last_dot != std::wstring::npos &&
        full_name.substr(last_dot) == L".exe") {
      full_name = full_name.substr(0, last_dot);
    }

    return WStringToString(full_name);
  }

  CloseHandle(process_handle);
  return "";
}

std::string GetProcessPath(uint32_t pid) {
  // 打开进程句柄
  HANDLE process_handle =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (!process_handle) {
    return "";
  }

  // 获取完整路径
  wchar_t process_path[MAX_PATH] = {0};
  DWORD size = MAX_PATH;

  if (QueryFullProcessImageNameW(process_handle, 0, process_path, &size)) {
    CloseHandle(process_handle);
    return WStringToString(std::wstring(process_path));
  }

  CloseHandle(process_handle);
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
  HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (process_handle) {
    CloseHandle(process_handle);
    return true;
  }
  return false;
}

bool HasProcessAccess(uint32_t pid) {
  HANDLE process_handle =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (process_handle) {
    CloseHandle(process_handle);
    return true;
  }
  return false;
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
