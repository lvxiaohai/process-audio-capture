#pragma once
#include "../audio_capture.h"

#ifdef __APPLE__
#include <vector>

/**
 * @file process_list.h
 * @brief 声明获取进程列表相关的函数
 *
 * 本文件声明了用于获取系统中正在播放音频的进程列表的函数。
 */

namespace audio_capture
{
    namespace process_list
    {

        /**
         * @brief 获取当前系统中正在播放音频的进程列表
         *
         * 此函数使用CoreAudio API和AppKit获取所有正在播放音频的进程信息，
         * 包括进程ID、名称、路径、描述和图标数据。
         *
         * @return 包含进程信息的向量
         */
        std::vector<ProcessInfo> GetProcessList();

    } // namespace process_list
} // namespace audio_capture
#endif // __APPLE__
