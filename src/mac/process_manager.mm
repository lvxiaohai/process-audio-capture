#ifdef __APPLE__

#include "../../include/process_manager.h"
#include "../../include/mac/mac_audio_capture.h"
#include "../../include/mac/mac_utils.h"
#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <libproc.h>
#include <set>
#include <sys/sysctl.h>
#include <vector>

using namespace audio_capture;

namespace process_manager {

// 前向声明
std::vector<uint32_t> GetSelfProcessIds();

// 辅助函数声明
void populateProcessFromApp(ProcessInfo *process, NSRunningApplication *app);
void populateProcessFromSystemAPI(ProcessInfo *process, pid_t pid,
                                  AudioObjectID objectID);
void setIconForProcess(ProcessInfo *process, NSImage *icon);
void setIconForProcessFromPath(ProcessInfo *process, const char *processPath);
void setEmptyIconForProcess(ProcessInfo *process);
void optimizeProcessInfo(ProcessInfo *process);
bool updateProcessFromInfoPlist(ProcessInfo *process,
                                const std::string &appBundlePath);

/**
 * @brief 获取系统中有音频活动的进程列表
 *
 * 基于 AudioCap 参考实现，优化了进程信息获取和图标处理逻辑。
 * 支持将 Helper 进程名称替换为主应用名称。
 *
 * @return 进程信息列表，包含名称、路径、图标等详细信息
 */
std::vector<ProcessInfo> GetProcessList() {
  std::vector<ProcessInfo> processes;

  @try {
    // 获取所有音频进程的 AudioObjectID
    std::vector<AudioObjectID> processIDs = mac_utils::GetProcessList();
    if (processIDs.empty()) {
      return processes;
    }

    // 获取所有运行中的应用程序，排除当前进程
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    if (!workspace) {
      return processes;
    }

    NSArray<NSRunningApplication *> *allApps = [workspace runningApplications];
    if (!allApps) {
      return processes;
    }

    // 过滤掉当前进程
    pid_t currentPID = getpid();
    NSMutableArray<NSRunningApplication *> *runningApps =
        [[NSMutableArray alloc] init];
    for (NSRunningApplication *app in allApps) {
      if (app.processIdentifier != currentPID) {
        [runningApps addObject:app];
      }
    }

    // 遍历音频进程并获取详细信息
    for (AudioObjectID objectID : processIDs) {
      @try {
        ProcessInfo process = {};

        // 获取进程 PID
        pid_t pid = mac_utils::GetProcessPID(objectID);
        if (pid <= 0) {
          continue;
        }

        process.pid = static_cast<uint32_t>(pid);

        // 查找对应的 NSRunningApplication
        NSRunningApplication *app = nil;
        for (NSRunningApplication *runningApp in runningApps) {
          if (runningApp.processIdentifier == pid) {
            app = runningApp;
            break;
          }
        }

        // 获取进程信息
        if (app) {
          // 从 NSRunningApplication 获取信息
          populateProcessFromApp(&process, app);
        } else {
          // 从系统 API 获取信息
          populateProcessFromSystemAPI(&process, pid, objectID);
        }

        // 检查是否有音频活动（放在最后以减少不必要的检查）
        bool audioActive = mac_utils::IsProcessPlayingAudio(objectID);
        if (!audioActive) {
          continue;
        }

        // 优化进程显示信息（从 Info.plist 获取更准确的名称和图标）
        optimizeProcessInfo(&process);

        processes.push_back(process);

      } @catch (NSException *exception) {
        // 处理单个进程失败，继续处理其他进程
        continue;
      }
    }

    // 过滤当前应用相关进程
    std::vector<uint32_t> self_pids = GetSelfProcessIds();
    std::set<uint32_t> self_pid_set(self_pids.begin(), self_pids.end());

    std::vector<ProcessInfo> filtered_processes;
    for (const auto &process : processes) {
      if (self_pid_set.find(process.pid) == self_pid_set.end()) {
        filtered_processes.push_back(process);
      }
    }

    // 按名称排序，音频活跃的进程优先
    std::sort(filtered_processes.begin(), filtered_processes.end(),
              [](const ProcessInfo &a, const ProcessInfo &b) {
                // 首先按名称排序
                return a.name < b.name;
              });

    return filtered_processes;

  } @catch (NSException *exception) {
    return processes; // 返回空列表
  }
}

/**
 * @brief 从 NSRunningApplication 获取进程信息
 */
void populateProcessFromApp(ProcessInfo *process, NSRunningApplication *app) {
  // 设置名称 - 优先使用 localizedName
  NSString *name = app.localizedName;
  if (!name || name.length == 0) {
    name =
        app.bundleURL
            ? [[app.bundleURL.lastPathComponent stringByDeletingPathExtension]
                  copy]
            : nil;
  }
  if (!name || name.length == 0) {
    name = app.bundleIdentifier ? [app.bundleIdentifier.lastPathComponent copy]
                                : nil;
  }
  if (!name || name.length == 0) {
    name = [NSString stringWithFormat:@"Unknown %d", app.processIdentifier];
  }

  process->name = std::string([name UTF8String]);

  // 设置路径
  if (app.bundleURL && app.bundleURL.path) {
    process->path = std::string([app.bundleURL.path UTF8String]);
  }

  // 设置描述
  if (app.bundleIdentifier) {
    process->description = std::string([app.bundleIdentifier UTF8String]);
  } else {
    process->description = process->name;
  }

  // 获取图标
  setIconForProcess(process, app.icon);
}

/**
 * @brief 从系统 API 获取进程信息
 */
void populateProcessFromSystemAPI(ProcessInfo *process, pid_t pid,
                                  AudioObjectID objectID) {
  char processPath[MAXPATHLEN];
  char processName[MAXPATHLEN];

  // 获取进程路径和名称
  std::string name, path;
  bool hasPath = false, hasName = false;

  if (proc_pidpath(pid, processPath, sizeof(processPath)) > 0) {
    path = std::string(processPath);
    process->path = path;
    hasPath = true;

    // 从路径提取名称
    size_t lastSlash = path.find_last_of('/');
    if (lastSlash != std::string::npos) {
      name = path.substr(lastSlash + 1);
    } else {
      name = path;
    }
  }

  if (proc_name(pid, processName, sizeof(processName)) > 0) {
    std::string procNameStr(processName);
    if (!procNameStr.empty()) {
      name = procNameStr;
      hasName = true;
    }
  }

  // 设置名称和描述
  if (hasName || hasPath) {
    process->name = name;
    process->description = name;
  } else {
    process->name = "Unknown Process";
    process->description = "PID: " + std::to_string(pid);
  }

  // 尝试获取图标
  if (hasPath) {
    setIconForProcessFromPath(process, processPath);
  }
}

/**
 * @brief 设置进程图标（从 NSImage）
 */
void setIconForProcess(ProcessInfo *process, NSImage *icon) {
  if (!icon) {
    setEmptyIconForProcess(process);
    return;
  }

  // 设置图标尺寸
  NSSize iconSize = NSMakeSize(128, 128);
  [icon setSize:iconSize];

  NSRect imageRect = NSMakeRect(0, 0, iconSize.width, iconSize.height);
  CGImageRef cgImage = [icon CGImageForProposedRect:&imageRect
                                            context:nil
                                              hints:nil];

  if (cgImage) {
    NSBitmapImageRep *bitmapRep =
        [[NSBitmapImageRep alloc] initWithCGImage:cgImage];
    NSData *pngData =
        [bitmapRep representationUsingType:NSBitmapImageFileTypePNG
                                properties:@{}];

    if (pngData) {
      process->icon.format = "png";
      process->icon.width = static_cast<int>(iconSize.width);
      process->icon.height = static_cast<int>(iconSize.height);
      const uint8_t *bytes = static_cast<const uint8_t *>([pngData bytes]);
      process->icon.data.assign(bytes, bytes + [pngData length]);
      return;
    }
  }

  setEmptyIconForProcess(process);
}

/**
 * @brief 设置进程图标（从文件路径）
 */
void setIconForProcessFromPath(ProcessInfo *process, const char *processPath) {
  NSString *pathString = [NSString stringWithUTF8String:processPath];
  NSString *iconPath = pathString;

  // 检查是否在 .app 包中
  if ([pathString containsString:@".app/Contents/MacOS/"]) {
    NSRange range = [pathString rangeOfString:@".app/Contents/MacOS/"];
    if (range.location != NSNotFound) {
      iconPath = [pathString substringToIndex:range.location + 4]; // 包含 .app
    }
  }

  NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:iconPath];
  setIconForProcess(process, icon);
}

/**
 * @brief 设置空图标
 */
void setEmptyIconForProcess(ProcessInfo *process) {
  process->icon.format = "png";
  process->icon.width = 0;
  process->icon.height = 0;
  process->icon.data.clear();
}

/**
 * @brief 优化进程信息显示
 *
 * 通过读取 Info.plist 获取准确的应用名称和图标信息，
 * 比从路径推断更可靠。适用于所有类型的进程。
 */
void optimizeProcessInfo(ProcessInfo *process) {
  std::string &path = process->path;

  // 只要路径不为空就尝试优化
  if (!path.empty()) {
    // 查找 .app 包路径
    size_t appPos = path.find(".app/Contents/");
    if (appPos != std::string::npos) {
      // 构造 .app 包路径
      std::string appBundlePath = path.substr(0, appPos + 4);

      // 尝试从 Info.plist 获取应用信息
      if (updateProcessFromInfoPlist(process, appBundlePath)) {
        // 成功从 Info.plist 获取信息
        return;
      }

      // 如果 Info.plist 方法失败，回退到原方法
      size_t appStart = path.rfind('/', appPos);
      if (appStart != std::string::npos) {
        std::string appName = path.substr(appStart + 1, appPos - appStart - 1);
        if (!appName.empty()) {
          // 移除 .app 后缀
          size_t dotAppPos = appName.find(".app");
          if (dotAppPos == appName.length() - 4) {
            appName = appName.substr(0, dotAppPos);
          }

          // 更新名称和描述
          process->name = appName;
          process->description = appName;

          // 尝试获取主应用的图标
          NSString *appPathString =
              [NSString stringWithUTF8String:appBundlePath.c_str()];
          NSImage *appIcon =
              [[NSWorkspace sharedWorkspace] iconForFile:appPathString];
          if (appIcon) {
            setIconForProcess(process, appIcon);
          }
        }
      }
    }
  }
}

/**
 * @brief 从 Info.plist 更新进程信息
 *
 * @param process 要更新的进程信息
 * @param appBundlePath .app 包的完整路径
 * @return 是否成功更新
 */
bool updateProcessFromInfoPlist(ProcessInfo *process,
                                const std::string &appBundlePath) {
  @try {
    // 构造 Info.plist 路径
    std::string infoPlistPath = appBundlePath + "/Contents/Info.plist";
    NSString *plistPathString =
        [NSString stringWithUTF8String:infoPlistPath.c_str()];

    // 读取 Info.plist
    NSDictionary *infoPlist =
        [NSDictionary dictionaryWithContentsOfFile:plistPathString];
    if (!infoPlist) {
      return false;
    }

    // 获取应用显示名称（优先顺序）
    NSString *displayName = nil;

    // 1. CFBundleDisplayName (本地化显示名称)
    displayName = infoPlist[@"CFBundleDisplayName"];
    if (!displayName || displayName.length == 0) {
      // 2. CFBundleName (Bundle名称)
      displayName = infoPlist[@"CFBundleName"];
    }
    if (!displayName || displayName.length == 0) {
      // 3. CFBundleExecutable (可执行文件名)
      displayName = infoPlist[@"CFBundleExecutable"];
    }

    if (displayName && displayName.length > 0) {
      process->name = std::string([displayName UTF8String]);
      process->description = process->name;
    }

    // 获取应用图标
    NSString *iconFileName = infoPlist[@"CFBundleIconFile"];
    if (iconFileName && iconFileName.length > 0) {
      // 构造图标文件路径
      std::string iconPath = appBundlePath + "/Contents/Resources/";
      iconPath += [iconFileName UTF8String];

      // 如果没有扩展名，尝试添加 .icns
      if (iconPath.find('.') == std::string::npos) {
        iconPath += ".icns";
      }

      NSString *iconPathString =
          [NSString stringWithUTF8String:iconPath.c_str()];
      NSImage *appIcon =
          [[NSImage alloc] initWithContentsOfFile:iconPathString];

      if (appIcon) {
        setIconForProcess(process, appIcon);
      } else {
        // 如果图标文件不存在，使用系统的文件图标方法
        NSString *appPathString =
            [NSString stringWithUTF8String:appBundlePath.c_str()];
        NSImage *systemIcon =
            [[NSWorkspace sharedWorkspace] iconForFile:appPathString];
        if (systemIcon) {
          setIconForProcess(process, systemIcon);
        }
      }
    } else {
      // 没有指定图标文件，使用系统方法
      NSString *appPathString =
          [NSString stringWithUTF8String:appBundlePath.c_str()];
      NSImage *systemIcon =
          [[NSWorkspace sharedWorkspace] iconForFile:appPathString];
      if (systemIcon) {
        setIconForProcess(process, systemIcon);
      }
    }

    return true;

  } @catch (NSException *exception) {
    // 出现异常时返回 false，让调用者使用备用方法
    return false;
  }
}

/**
 * @brief 获取当前应用相关的进程 ID 列表
 *
 * 在 Electron 应用中，包括主进程、渲染进程、GPU进程等所有相关进程。
 * 通过进程名称和层级关系来识别同一应用的所有进程。
 *
 * @return 当前应用相关的进程 ID 列表
 */
std::vector<uint32_t> GetSelfProcessIds() {
  std::vector<uint32_t> pids;
  std::set<uint32_t> foundPids;

  // 获取当前进程ID
  uint32_t currentPid = static_cast<uint32_t>(getpid());
  foundPids.insert(currentPid);

  // 获取当前进程的可执行文件路径
  char currentPath[MAXPATHLEN];
  if (proc_pidpath(currentPid, currentPath, sizeof(currentPath)) <= 0) {
    // 如果无法获取路径，只返回当前进程
    pids.push_back(currentPid);
    return pids;
  }

  std::string currentExePath(currentPath);

  // 提取应用程序包名（去掉 .app/Contents/MacOS/xxx 部分）
  std::string appBundlePath;
  size_t appPos = currentExePath.find(".app/Contents/MacOS/");
  if (appPos != std::string::npos) {
    appBundlePath = currentExePath.substr(0, appPos + 4); // 包含 .app
  }

  // 获取所有进程
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
  size_t size;
  if (sysctl(mib, 4, nullptr, &size, nullptr, 0) != 0) {
    pids.push_back(currentPid);
    return pids;
  }

  struct kinfo_proc *processes = (struct kinfo_proc *)malloc(size);
  if (!processes) {
    pids.push_back(currentPid);
    return pids;
  }

  if (sysctl(mib, 4, processes, &size, nullptr, 0) != 0) {
    free(processes);
    pids.push_back(currentPid);
    return pids;
  }

  int processCount = size / sizeof(struct kinfo_proc);

  // 遍历所有进程，查找同一应用的进程
  for (int i = 0; i < processCount; i++) {
    pid_t pid = processes[i].kp_proc.p_pid;
    if (pid <= 0)
      continue;

    char processPath[MAXPATHLEN];
    if (proc_pidpath(pid, processPath, sizeof(processPath)) <= 0) {
      continue;
    }

    std::string procPath(processPath);

    // 检查是否是同一个应用程序包中的进程
    if (!appBundlePath.empty() && procPath.find(appBundlePath) == 0) {
      foundPids.insert(static_cast<uint32_t>(pid));
    }
    // 如果没有应用程序包，检查是否是相同的可执行文件
    else if (appBundlePath.empty() && procPath == currentExePath) {
      foundPids.insert(static_cast<uint32_t>(pid));
    }
  }

  // 额外检查：添加直接的父子进程关系
  // 这可以捕获一些可能被遗漏的 Electron 辅助进程
  std::vector<uint32_t> additionalPids;
  for (uint32_t pid : foundPids) {
    // 检查此进程的所有子进程
    for (int i = 0; i < processCount; i++) {
      if (processes[i].kp_eproc.e_ppid == static_cast<pid_t>(pid)) {
        uint32_t childPid = static_cast<uint32_t>(processes[i].kp_proc.p_pid);
        if (foundPids.find(childPid) == foundPids.end()) {
          char childPath[MAXPATHLEN];
          if (proc_pidpath(childPid, childPath, sizeof(childPath)) > 0) {
            std::string childPathStr(childPath);
            // 检查子进程是否也在同一应用包中或具有相似的名称
            if ((!appBundlePath.empty() &&
                 childPathStr.find(appBundlePath) == 0) ||
                childPathStr.find("Electron") != std::string::npos ||
                childPathStr.find("Helper") != std::string::npos) {
              additionalPids.push_back(childPid);
            }
          }
        }
      }
    }
  }

  // 添加额外发现的进程
  for (uint32_t pid : additionalPids) {
    foundPids.insert(pid);
  }

  free(processes);

  // 转换为向量
  for (uint32_t pid : foundPids) {
    pids.push_back(pid);
  }

  return pids;
}

} // namespace process_manager

#endif // __APPLE__