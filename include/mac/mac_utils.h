#pragma once

#ifdef __APPLE__

#include <CoreAudio/CoreAudio.h>
#include <string>
#include <vector>

namespace audio_capture {
namespace mac_utils {

// Core Audio 进程管理
std::vector<AudioObjectID> GetProcessList();
pid_t GetProcessPID(AudioObjectID processID);
bool IsProcessPlayingAudio(AudioObjectID processID);
AudioObjectID GetAudioObjectIDForPID(uint32_t pid);

// 内部辅助函数
std::string CFStringToStdString(CFStringRef cfString);

} // namespace mac_utils
} // namespace audio_capture

#endif // __APPLE__
