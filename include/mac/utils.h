#pragma once

#ifdef __APPLE__
#include <string>
#include <vector>
#include <CoreAudio/CoreAudio.h>

namespace audio_capture
{
    namespace utils
    {
        // 辅助函数：将CFString转换为std::string
        std::string CFStringToStdString(CFStringRef cfString);

        // 辅助函数：读取AudioObjectID属性
        template <typename T>
        bool GetAudioObjectProperty(AudioObjectID objectID, AudioObjectPropertySelector selector, T &value);

        // 辅助函数：读取进程列表
        std::vector<AudioObjectID> GetProcessList();

        // 辅助函数：检查进程是否正在播放音频
        bool IsProcessPlayingAudio(AudioObjectID processID);

        // 辅助函数：获取进程的Bundle ID
        std::string GetProcessBundleID(AudioObjectID processID);

        // 辅助函数：获取进程ID
        bool GetProcessPID(AudioObjectID processID, pid_t &pid);

        // 辅助函数：获取进程信息
        bool GetProcessInfo(uint32_t pid, std::string &name, std::string &path);

        // 辅助函数：根据PID获取AudioObjectID
        AudioObjectID GetAudioObjectIDForPID(uint32_t pid);
    }
}
#endif // __APPLE__