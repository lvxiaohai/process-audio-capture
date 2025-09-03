#include "../../include/mac/process_list.h"
#include "../../include/mac/utils.h"

#ifdef __APPLE__
#include <AppKit/AppKit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libproc.h>
#include <string>
#include <sys/param.h>
#include <vector>

/**
 * @file process_list.mm
 * @brief 实现获取进程列表的功能
 *
 * 本文件实现了获取系统中正在播放音频的进程列表的功能。
 * 使用Objective-C++以便能够访问AppKit API获取应用图标。
 */

namespace audio_capture {
namespace process_list {

std::vector<ProcessInfo> GetProcessList() {
  std::vector<ProcessInfo> processes;

  // 获取所有进程的AudioObjectID
  // AudioObjectID是CoreAudio API中用于标识音频对象的唯一标识符
  std::vector<AudioObjectID> processIDs = utils::GetProcessList();

  // 获取所有运行中的应用程序
  // 使用NSWorkspace获取系统中所有正在运行的应用程序信息
  NSArray<NSRunningApplication *> *runningApps =
      [[NSWorkspace sharedWorkspace] runningApplications];

  // 遍历所有具有音频活动的进程
  for (AudioObjectID processID : processIDs) {
    // 获取进程ID
    pid_t pid;
    if (!utils::GetProcessPID(processID, pid)) {
      continue;
    }

    ProcessInfo process;
    process.pid = pid;

    // 检查是否有音频活动
    // 只关注正在播放音频的进程
    bool audioActive = utils::IsProcessPlayingAudio(processID);

    // 如果没有音频活动，跳过这个进程
    if (!audioActive) {
      continue;
    }

    // 获取Bundle ID
    // Bundle ID是macOS应用程序的唯一标识符，如com.apple.Music
    std::string bundleID = utils::GetProcessBundleID(processID);

    // 尝试从NSRunningApplication获取更多信息
    // 将CoreAudio的进程ID与NSWorkspace的应用程序匹配
    NSRunningApplication *app = nil;
    for (NSRunningApplication *runningApp in runningApps) {
      if ([runningApp processIdentifier] == pid) {
        app = runningApp;
        break;
      }
    }

    if (app) {
      // 从应用程序获取信息
      // 优先使用本地化名称，然后是Bundle ID，最后是PID
      process.name =
          app.localizedName
              ? std::string([app.localizedName UTF8String])
              : (app.bundleIdentifier
                     ? std::string([[app.bundleIdentifier lastPathComponent]
                           UTF8String])
                     : "Unknown " + std::to_string(pid));
      process.path =
          app.bundleURL ? std::string([app.bundleURL.path UTF8String]) : "";
      process.description = app.bundleIdentifier
                                ? std::string([app.bundleIdentifier UTF8String])
                                : "PID: " + std::to_string(pid);

      // 获取应用图标
      // 将NSImage转换为PNG格式以便在跨平台环境中使用
      NSImage *icon = [app icon];
      if (icon) {
        // 将NSImage转换为PNG数据
        NSData *tiffData = [icon TIFFRepresentation];
        if (tiffData) {
          NSBitmapImageRep *imageRep =
              [NSBitmapImageRep imageRepWithData:tiffData];
          NSData *pngData =
              [imageRep representationUsingType:NSBitmapImageFileTypePNG
                                     properties:@{}];

          if (pngData) {
            process.icon.format = "png";
            process.icon.width = icon.size.width;
            process.icon.height = icon.size.height;
            const uint8_t *bytes =
                static_cast<const uint8_t *>([pngData bytes]);
            process.icon.data.assign(bytes, bytes + [pngData length]);
          }
        }
      }
    } else {
      // 从proc_*函数获取进程信息
      // 对于非应用程序类型的进程，使用底层系统API获取信息
      std::string name, path;
      if (utils::GetProcessInfo(pid, name, path)) {
        process.name = name;
        process.path = path;
        process.description =
            bundleID.empty() ? "PID: " + std::to_string(pid) : bundleID;

        // 为进程获取默认图标
        // 对于系统进程，使用终端图标作为默认图标
        NSImage *icon = [[NSWorkspace sharedWorkspace]
            iconForFile:@"/bin/bash"]; // 默认使用终端图标
        if (icon) {
          NSData *tiffData = [icon TIFFRepresentation];
          if (tiffData) {
            NSBitmapImageRep *imageRep =
                [NSBitmapImageRep imageRepWithData:tiffData];
            NSData *pngData =
                [imageRep representationUsingType:NSBitmapImageFileTypePNG
                                       properties:@{}];

            if (pngData) {
              process.icon.format = "png";
              process.icon.width = icon.size.width;
              process.icon.height = icon.size.height;
              const uint8_t *bytes =
                  static_cast<const uint8_t *>([pngData bytes]);
              process.icon.data.assign(bytes, bytes + [pngData length]);
            }
          }
        }
      } else {
        // 如果获取进程信息失败，使用默认值
        process.name = "Unknown Process";
        process.description = "PID: " + std::to_string(pid);
      }
    }

    // 如果没有图标数据，使用默认图标
    // 提供一个1x1像素的透明PNG作为默认图标
    if (process.icon.data.empty()) {
      process.icon.format = "png";
      process.icon.width = 1;
      process.icon.height = 1;
      static const uint8_t pngData[] = {
          0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00,
          0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01,
          0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90,
          0x77, 0x53, 0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41,
          0x54, 0x08, 0xD7, 0x63, 0xF8, 0xCF, 0xC0, 0x00, 0x00, 0x03,
          0x01, 0x01, 0x00, 0x18, 0xDD, 0x8D, 0xB0, 0x00, 0x00, 0x00,
          0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};
      process.icon.data.assign(pngData, pngData + sizeof(pngData));
    }

    processes.push_back(process);
  }

  return processes;
}

} // namespace process_list
} // namespace audio_capture
#endif // __APPLE__