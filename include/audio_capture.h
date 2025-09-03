#pragma once
#include <string>
#include <memory>
#include <vector>
#include <functional>

/**
 * @file audio_capture.h
 * @brief 音频捕获模块的主要接口定义
 *
 * 本文件定义了音频捕获模块的核心接口和数据结构，包括进程信息、权限状态、
 * 音频数据回调等。该接口设计为跨平台，但目前仅实现了macOS平台。
 */

namespace audio_capture
{
    /**
     * @struct IconData
     * @brief 图标数据结构体
     *
     * 用于存储进程图标的二进制数据和相关属性。
     */
    struct IconData
    {
        std::vector<uint8_t> data; ///< 图标的二进制数据
        std::string format;        ///< 图标格式 (例如 "png", "jpeg", "ico")
        int width;                 ///< 图标宽度
        int height;                ///< 图标高度
    };

    /**
     * @struct ProcessInfo
     * @brief 进程信息结构体
     *
     * 用于存储可捕获音频的进程的相关信息。
     */
    struct ProcessInfo
    {
        uint32_t pid;            ///< 进程ID
        std::string name;        ///< 进程名称
        std::string description; ///< 进程描述（可选）
        std::string path;        ///< 进程可执行文件路径
        IconData icon;           ///< 进程图标
    };

    /**
     * @enum PermissionStatus
     * @brief 权限状态枚举
     *
     * 表示音频捕获权限的当前状态。
     */
    enum class PermissionStatus
    {
        Unknown,   ///< 未知状态，通常表示用户尚未做出选择
        Denied,    ///< 已拒绝，用户明确拒绝了权限请求
        Authorized ///< 已授权，用户已同意授予权限
    };

    /**
     * @typedef AudioDataCallback
     * @brief PCM音频数据回调函数类型
     *
     * 当捕获到新的音频数据时，将通过此回调函数传递给调用者。
     *
     * @param data 指向PCM音频数据的指针
     * @param length 数据长度（字节数）
     * @param channels 音频通道数
     * @param sampleRate 采样率（Hz）
     */
    using AudioDataCallback = std::function<void(const uint8_t *data, size_t length, int channels, int sampleRate)>;

    /**
     * @typedef PermissionCallback
     * @brief 权限状态变化回调函数类型
     *
     * 当权限状态发生变化时（例如用户响应权限请求对话框），将通过此回调函数通知调用者。
     *
     * @param status 新的权限状态
     */
    using PermissionCallback = std::function<void(PermissionStatus status)>;

    /**
     * @class AudioCaptureImpl
     * @brief 音频捕获接口类
     *
     * 定义了音频捕获模块的核心接口，包括权限管理、进程列表获取、音频捕获等功能。
     * 这是一个抽象基类，需要由平台特定的实现类继承并实现所有方法。
     */
    class AudioCaptureImpl
    {
    public:
        /**
         * @brief 虚析构函数
         *
         * 确保派生类的析构函数被正确调用。
         */
        virtual ~AudioCaptureImpl() = default;

        /**
         * @brief 检查音频捕获权限状态
         * @return 当前权限状态
         */
        virtual PermissionStatus CheckPermission() = 0;

        /**
         * @brief 请求音频捕获权限
         * @param callback 权限状态变化回调函数
         *
         * 此方法将触发系统权限请求对话框，用户可以选择允许或拒绝。
         * 用户的选择将通过回调函数通知调用者。
         */
        virtual void RequestPermission(PermissionCallback callback) = 0;

        /**
         * @brief 获取所有可捕获音频的进程列表
         * @return 包含进程信息的向量
         *
         * 返回当前系统中正在播放音频的所有进程的信息。
         */
        virtual std::vector<ProcessInfo> GetProcessList() = 0;

        /**
         * @brief 开始捕获指定进程的音频
         * @param pid 目标进程ID
         * @param callback 接收音频数据的回调函数
         * @return 是否成功启动捕获
         *
         * 开始捕获指定进程的音频，并通过回调函数返回PCM数据。
         */
        virtual bool StartCapture(uint32_t pid, AudioDataCallback callback) = 0;

        /**
         * @brief 停止捕获
         * @return 是否成功停止捕获
         *
         * 停止当前的音频捕获，释放相关资源。
         */
        virtual bool StopCapture() = 0;

        /**
         * @brief 测试方法 - Hello World
         * @param input 输入字符串
         * @return 带有输入的问候语
         *
         * 这是一个简单的测试方法，用于验证模块是否正常工作。
         */
        virtual std::string HelloWorld(const std::string &input) = 0;
    };

    /**
     * @brief 工厂函数 - 创建平台特定的实现
     * @return 平台特定的AudioCaptureImpl实例
     *
     * 根据当前运行平台创建相应的音频捕获实现实例。
     */
    std::unique_ptr<AudioCaptureImpl> CreatePlatformAudioCapture();

} // namespace audio_capture