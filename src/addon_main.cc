#include <napi.h>
#include <string>
#include <memory>
#include <vector>
#include "../include/audio_capture.h"

// 创建平台特定的实现
std::unique_ptr<audio_capture::AudioCaptureImpl> g_audio_capture = audio_capture::CreatePlatformAudioCapture();

// PCM数据回调函数的JavaScript引用
Napi::ThreadSafeFunction g_ts_callback;

// 权限状态回调函数的JavaScript引用
Napi::ThreadSafeFunction g_ts_permission_callback;

// 创建一个将暴露给JavaScript的类
class AudioCaptureAddon : public Napi::ObjectWrap<AudioCaptureAddon>
{
public:
    // 这个静态方法为JavaScript定义类
    static Napi::Object Init(Napi::Env env, Napi::Object exports)
    {
        // 定义带有方法的JavaScript类
        Napi::Function func = DefineClass(env, "AudioCaptureAddon", {
                                                                        InstanceMethod("checkPermission", &AudioCaptureAddon::CheckPermission),
                                                                        InstanceMethod("requestPermission", &AudioCaptureAddon::RequestPermission),
                                                                        InstanceMethod("getProcessList", &AudioCaptureAddon::GetProcessList),
                                                                        InstanceMethod("startCapture", &AudioCaptureAddon::StartCapture),
                                                                        InstanceMethod("stopCapture", &AudioCaptureAddon::StopCapture),
                                                                        InstanceMethod("helloWorld", &AudioCaptureAddon::HelloWorld),
                                                                    });

        // 创建构造函数的持久引用
        Napi::FunctionReference *constructor = new Napi::FunctionReference();
        *constructor = Napi::Persistent(func);
        env.SetInstanceData(constructor);

        // 在exports对象上设置构造函数
        exports.Set("AudioCaptureAddon", func);
        return exports;
    }

    // 构造函数
    AudioCaptureAddon(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<AudioCaptureAddon>(info) {}

private:
    // 检查音频捕获权限状态
    Napi::Value CheckPermission(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();
        audio_capture::PermissionStatus status = g_audio_capture->CheckPermission();

        Napi::Object result = Napi::Object::New(env);

        switch (status)
        {
        case audio_capture::PermissionStatus::Authorized:
            result.Set("status", Napi::String::New(env, "authorized"));
            break;
        case audio_capture::PermissionStatus::Denied:
            result.Set("status", Napi::String::New(env, "denied"));
            break;
        case audio_capture::PermissionStatus::Unknown:
        default:
            result.Set("status", Napi::String::New(env, "unknown"));
            break;
        }

        return result;
    }

    // 请求音频捕获权限
    Napi::Value RequestPermission(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        // 验证参数
        if (info.Length() < 1 || !info[0].IsFunction())
        {
            Napi::TypeError::New(env, "参数错误: 需要回调函数").ThrowAsJavaScriptException();
            return env.Null();
        }

        Napi::Function callback = info[0].As<Napi::Function>();

        // 创建线程安全的函数回调
        g_ts_permission_callback = Napi::ThreadSafeFunction::New(
            env,
            callback,
            "PermissionCallback",
            0,
            1,
            [](Napi::Env)
            {
                // 清理回调
            });

        // 设置C++回调函数，将权限状态传递给JavaScript
        g_audio_capture->RequestPermission(
            [](audio_capture::PermissionStatus status)
            {
                // 在新线程中调用JavaScript回调
                auto callback = [status](Napi::Env env, Napi::Function jsCallback)
                {
                    // 创建返回对象
                    Napi::Object result = Napi::Object::New(env);

                    switch (status)
                    {
                    case audio_capture::PermissionStatus::Authorized:
                        result.Set("status", Napi::String::New(env, "authorized"));
                        break;
                    case audio_capture::PermissionStatus::Denied:
                        result.Set("status", Napi::String::New(env, "denied"));
                        break;
                    case audio_capture::PermissionStatus::Unknown:
                    default:
                        result.Set("status", Napi::String::New(env, "unknown"));
                        break;
                    }

                    // 调用JavaScript回调
                    jsCallback.Call({result});
                };

                g_ts_permission_callback.BlockingCall(callback);
            });

        return env.Undefined();
    }

    // 获取进程列表
    Napi::Value GetProcessList(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();
        std::vector<audio_capture::ProcessInfo> processes = g_audio_capture->GetProcessList();

        Napi::Array result = Napi::Array::New(env, processes.size());

        for (size_t i = 0; i < processes.size(); i++)
        {
            Napi::Object process = Napi::Object::New(env);
            process.Set("pid", Napi::Number::New(env, processes[i].pid));
            process.Set("name", Napi::String::New(env, processes[i].name));
            process.Set("description", Napi::String::New(env, processes[i].description));
            process.Set("path", Napi::String::New(env, processes[i].path));

            // 处理图标数据
            if (!processes[i].icon.data.empty())
            {
                // 创建图标对象
                Napi::Object iconObj = Napi::Object::New(env);

                // 创建ArrayBuffer来存储图标数据
                Napi::ArrayBuffer iconBuffer = Napi::ArrayBuffer::New(env, processes[i].icon.data.size());
                memcpy(iconBuffer.Data(), processes[i].icon.data.data(), processes[i].icon.data.size());

                // 设置图标属性
                iconObj.Set("data", Napi::Uint8Array::New(env, processes[i].icon.data.size(), iconBuffer, 0));
                iconObj.Set("format", Napi::String::New(env, processes[i].icon.format));
                iconObj.Set("width", Napi::Number::New(env, processes[i].icon.width));
                iconObj.Set("height", Napi::Number::New(env, processes[i].icon.height));

                // 将图标对象添加到进程对象
                process.Set("icon", iconObj);
            }

            result.Set(i, process);
        }

        return result;
    }

    // 开始捕获指定进程的音频
    Napi::Value StartCapture(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        // 验证参数
        if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsFunction())
        {
            Napi::TypeError::New(env, "参数错误: 需要进程ID和回调函数").ThrowAsJavaScriptException();
            return env.Null();
        }

        uint32_t pid = info[0].As<Napi::Number>().Uint32Value();
        Napi::Function callback = info[1].As<Napi::Function>();

        // 创建线程安全的函数回调
        g_ts_callback = Napi::ThreadSafeFunction::New(
            env,
            callback,
            "AudioCaptureCallback",
            0,
            1,
            [](Napi::Env)
            {
                // 清理回调
            });

        // 设置C++回调函数，将PCM数据传递给JavaScript
        bool result = g_audio_capture->StartCapture(pid,
                                                    [](const uint8_t *data, size_t length, int channels, int sampleRate)
                                                    {
                                                        // 在新线程中调用JavaScript回调
                                                        auto callback = [data, length, channels, sampleRate](Napi::Env env, Napi::Function jsCallback)
                                                        {
                                                            // 创建ArrayBuffer来存储PCM数据
                                                            Napi::ArrayBuffer buffer = Napi::ArrayBuffer::New(env, length);
                                                            memcpy(buffer.Data(), data, length);

                                                            // 创建返回对象
                                                            Napi::Object result = Napi::Object::New(env);
                                                            result.Set("data", Napi::Uint8Array::New(env, length, buffer, 0));
                                                            result.Set("channels", Napi::Number::New(env, channels));
                                                            result.Set("sampleRate", Napi::Number::New(env, sampleRate));

                                                            // 调用JavaScript回调
                                                            jsCallback.Call({result});
                                                        };

                                                        g_ts_callback.BlockingCall(callback);
                                                    });

        return Napi::Boolean::New(env, result);
    }

    // 停止捕获
    Napi::Value StopCapture(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        bool result = g_audio_capture->StopCapture();

        // 释放线程安全函数
        if (g_ts_callback)
        {
            g_ts_callback.Release();
        }

        return Napi::Boolean::New(env, result);
    }

    // Hello World测试方法
    Napi::Value HelloWorld(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        // 验证参数（期望一个字符串）
        if (info.Length() < 1 || !info[0].IsString())
        {
            Napi::TypeError::New(env, "期望字符串参数").ThrowAsJavaScriptException();
            return env.Null();
        }

        // 将JavaScript字符串转换为C++字符串
        std::string input = info[0].As<Napi::String>();

        // 调用我们的平台特定实现
        std::string result = g_audio_capture->HelloWorld(input);

        // 将C++字符串转换回JavaScript字符串并返回
        return Napi::String::New(env, result);
    }
};

// 初始化插件
Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    return AudioCaptureAddon::Init(env, exports);
}

// 注册初始化函数
NODE_API_MODULE(audio_capture, Init)