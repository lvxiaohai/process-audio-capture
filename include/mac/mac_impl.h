#pragma once
#include "../audio_capture.h"

#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>

/**
 * @file mac_impl.h
 * @brief macOS平台的音频捕获实现
 *
 * 本文件定义了macOS平台特定的音频捕获实现类MacAudioCapture，
 * 该类继承自通用接口AudioCaptureImpl，并实现了所有必要的方法。
 */

namespace audio_capture
{
    // 前向声明
    namespace audio_tap
    {
        class ProcessTap;
    }

    /**
     * @class MacAudioCapture
     * @brief macOS平台的音频捕获实现类
     *
     * 该类使用CoreAudio API实现了在macOS平台上捕获指定进程的音频数据的功能。
     * 它提供了获取进程列表、开始/停止捕获等功能，并通过回调函数将捕获的音频数据传递给JavaScript。
     */
    class MacAudioCapture : public AudioCaptureImpl
    {
    public:
        /**
         * @brief 构造函数
         */
        MacAudioCapture();

        /**
         * @brief 析构函数
         *
         * 确保在对象销毁前停止所有捕获活动并清理资源。
         */
        ~MacAudioCapture() override;

        /**
         * @brief 检查音频捕获权限状态
         * @return 当前权限状态
         */
        PermissionStatus CheckPermission() override;

        /**
         * @brief 请求音频捕获权限
         * @param callback 权限状态变化回调函数
         */
        void RequestPermission(PermissionCallback callback) override;

        /**
         * @brief 获取当前系统中正在播放音频的进程列表
         * @return 包含进程信息的向量
         */
        std::vector<ProcessInfo> GetProcessList() override;

        /**
         * @brief 开始捕获指定进程的音频
         * @param pid 目标进程ID
         * @param callback 接收音频数据的回调函数
         * @return 是否成功启动捕获
         */
        bool StartCapture(uint32_t pid, AudioDataCallback callback) override;

        /**
         * @brief 停止音频捕获
         * @return 是否成功停止捕获
         */
        bool StopCapture() override;

        /**
         * @brief 测试函数，返回带有输入字符串的问候语
         * @param input 输入字符串
         * @return 带有输入的问候语
         */
        std::string HelloWorld(const std::string &input) override;

    private:
        // macOS特定的实现细节
        std::atomic<bool> capturing_{false};     ///< 是否正在捕获音频
        AudioDataCallback callback_;             ///< 音频数据回调函数
        uint32_t target_pid_ = 0;                ///< 目标进程ID
        PermissionCallback permission_callback_; ///< 权限状态回调函数

        // 音频捕获对象
        std::unique_ptr<audio_tap::ProcessTap> process_tap_; ///< 进程音频捕获对象

        /**
         * @brief 清理捕获相关的资源
         *
         * 在停止捕获后调用，用于释放所有分配的资源。
         */
        void CleanUp();
    };

} // namespace audio_capture
#endif // __APPLE__