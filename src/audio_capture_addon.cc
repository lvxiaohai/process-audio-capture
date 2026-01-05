#include "../include/audio_capture.h"
#include "../include/permission_manager.h"
#include "../include/process_manager.h"
#include <cstring>
#include <memory>
#include <napi.h>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * @file audio_capture_addon.cc
 * @brief Node.js 原生模块的主要实现
 *
 * 本文件实现了 Node.js 原生模块的接口，将 C++ 音频捕获功能暴露给 JavaScript。
 * 使用 Node-API (N-API) 实现跨 Node.js 版本的稳定性。
 */

// 创建平台特定的实现
std::unique_ptr<audio_capture::AudioCapture> g_audio_capture;

// PCM数据回调函数的JavaScript引用
Napi::ThreadSafeFunction g_ts_callback;

// 权限状态回调函数的JavaScript引用
Napi::ThreadSafeFunction g_ts_permission_callback;

// 创建一个将暴露给JavaScript的类
class AudioCaptureAddon : public Napi::ObjectWrap<AudioCaptureAddon> {
public:
  // 这个静态方法为JavaScript定义类
  static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    // 定义带有方法的JavaScript类
    Napi::Function func = DefineClass(
        env, "AudioCaptureAddon",
        {
            InstanceMethod("checkPermission",
                           &AudioCaptureAddon::CheckPermission),
            InstanceMethod("requestPermission",
                           &AudioCaptureAddon::RequestPermission),
            InstanceMethod("getProcessList",
                           &AudioCaptureAddon::GetProcessList),
            InstanceMethod("startCapture", &AudioCaptureAddon::StartCapture),
            InstanceMethod("stopCapture", &AudioCaptureAddon::StopCapture),
            InstanceMethod("isCapturing", &AudioCaptureAddon::IsCapturing),
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
      : Napi::ObjectWrap<AudioCaptureAddon>(info) {
    // 延迟初始化平台特定的实现
    if (!g_audio_capture) {
      g_audio_capture = audio_capture::CreatePlatformAudioCapture();
    }
  }

private:
  // 检查音频捕获权限状态
  Napi::Value CheckPermission(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    permission_manager::PermissionStatus status =
        permission_manager::PermissionManager::GetInstance().CheckPermission();

    Napi::Object result = Napi::Object::New(env);

    switch (status) {
    case permission_manager::PermissionStatus::Authorized:
      result.Set("status", Napi::String::New(env, "authorized"));
      break;
    case permission_manager::PermissionStatus::Denied:
      result.Set("status", Napi::String::New(env, "denied"));
      break;
    case permission_manager::PermissionStatus::Unknown:
    default:
      result.Set("status", Napi::String::New(env, "unknown"));
      break;
    }

    return result;
  }

  // 请求音频捕获权限
  Napi::Value RequestPermission(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    // 验证参数
    if (info.Length() < 1 || !info[0].IsFunction()) {
      Napi::TypeError::New(env, "参数错误: 需要回调函数")
          .ThrowAsJavaScriptException();
      return env.Null();
    }

    Napi::Function callback = info[0].As<Napi::Function>();

    // 创建线程安全的函数回调
    g_ts_permission_callback = Napi::ThreadSafeFunction::New(
        env, callback, "PermissionCallback", 0, 1, [](Napi::Env) {
          // 清理回调
        });

    // 设置C++回调函数，将权限状态传递给JavaScript
    permission_manager::PermissionManager::GetInstance().RequestPermission(
        [](permission_manager::PermissionStatus status) {
          // 在新线程中调用JavaScript回调
          auto callback = [status](Napi::Env env, Napi::Function jsCallback) {
            try {
              // 创建返回对象
              Napi::Object result = Napi::Object::New(env);

              switch (status) {
              case permission_manager::PermissionStatus::Authorized:
                result.Set("status", Napi::String::New(env, "authorized"));
                break;
              case permission_manager::PermissionStatus::Denied:
                result.Set("status", Napi::String::New(env, "denied"));
                break;
              case permission_manager::PermissionStatus::Unknown:
              default:
                result.Set("status", Napi::String::New(env, "unknown"));
                break;
              }

              // 调用JavaScript回调，添加错误处理
              jsCallback.Call({result});
            } catch (const Napi::Error &e) {
              // 捕获N-API异常，避免崩溃
            } catch (const std::exception &e) {
              // 捕获其他C++异常
            } catch (...) {
              // 捕获所有其他异常
            }
          };

          g_ts_permission_callback.BlockingCall(callback);
        });

    return env.Undefined();
  }

  // 获取进程列表（已自动过滤当前应用进程）
  Napi::Value GetProcessList(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    std::vector<process_manager::ProcessInfo> processes =
        process_manager::GetProcessList();

    Napi::Array result = Napi::Array::New(env, processes.size());

    for (size_t i = 0; i < processes.size(); i++) {
      Napi::Object process = Napi::Object::New(env);
      const auto &p = processes[i];

      process.Set("pid", Napi::Number::New(env, p.pid));
      process.Set("name", Napi::String::New(env, p.name));
      process.Set("description", Napi::String::New(env, p.description));
      process.Set("path", Napi::String::New(env, p.path));

      // 处理图标数据
      if (!p.icon.data.empty()) {
        // 创建图标对象
        Napi::Object iconObj = Napi::Object::New(env);

        // 创建ArrayBuffer来存储图标数据
        Napi::ArrayBuffer iconBuffer =
            Napi::ArrayBuffer::New(env, p.icon.data.size());
        memcpy(iconBuffer.Data(), p.icon.data.data(), p.icon.data.size());

        // 设置图标属性
        iconObj.Set("data", Napi::Uint8Array::New(env, p.icon.data.size(),
                                                  iconBuffer, 0));
        iconObj.Set("format", Napi::String::New(env, p.icon.format));
        iconObj.Set("width", Napi::Number::New(env, p.icon.width));
        iconObj.Set("height", Napi::Number::New(env, p.icon.height));

        // 将图标对象添加到进程对象
        process.Set("icon", iconObj);
      }

      result.Set(i, process);
    }

    return result;
  }

  // 开始捕获指定进程的音频
  Napi::Value StartCapture(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    // 验证参数
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsFunction()) {
      Napi::TypeError::New(env, "参数错误: 需要进程ID和回调函数")
          .ThrowAsJavaScriptException();
      return env.Null();
    }

    uint32_t pid = info[0].As<Napi::Number>().Uint32Value();
    Napi::Function callback = info[1].As<Napi::Function>();

    // 创建线程安全的函数回调
    g_ts_callback = Napi::ThreadSafeFunction::New(
        env, callback, "AudioCaptureCallback", 0, 1, [](Napi::Env) {
          // 清理回调
        });

    // 设置C++回调函数，将PCM数据传递给JavaScript
    bool result = g_audio_capture->StartCapture(pid, [](const uint8_t *data,
                                                        size_t length,
                                                        int channels,
                                                        int sampleRate) {
      // 数据有效性检查
      if (!data || length == 0 || length > 16 * 1024 * 1024) { // 限制最大16MB
        return;
      }

      // 参数合理性检查
      if (channels <= 0 || channels > 32 || sampleRate <= 0 ||
          sampleRate > 192000) {
        return;
      }

      // 创建数据副本，避免悬空指针问题
      std::vector<uint8_t> dataCopy;
      try {
        dataCopy.assign(data, data + length);
      } catch (const std::exception &e) {
        // 内存分配失败，跳过这帧数据
        return;
      }

      // 在新线程中调用JavaScript回调
      auto callback = [dataCopy = std::move(dataCopy), length, channels,
                       sampleRate](Napi::Env env, Napi::Function jsCallback) {
        try {
          // 再次验证数据长度
          if (dataCopy.size() != length || length == 0) {
            return;
          }

          // 创建ArrayBuffer来存储PCM数据
          Napi::ArrayBuffer buffer = Napi::ArrayBuffer::New(env, length);
          if (!buffer.Data()) {
            return; // ArrayBuffer创建失败
          }

          // 安全的内存拷贝
          std::memcpy(buffer.Data(), dataCopy.data(), length);

          // 创建返回对象
          Napi::Object result = Napi::Object::New(env);
          size_t sampleCount = length / sizeof(float);
          result.Set("buffer",
                     Napi::Float32Array::New(env, sampleCount, buffer, 0));
          result.Set("channels", Napi::Number::New(env, channels));
          result.Set("sampleRate", Napi::Number::New(env, sampleRate));

          // 调用JavaScript回调，添加错误处理
          jsCallback.Call({result});
        } catch (const Napi::Error &e) {
          // 捕获N-API异常，避免崩溃
          // 在开发环境可以输出错误信息
        } catch (const std::exception &e) {
          // 捕获其他C++异常
        } catch (...) {
          // 捕获所有其他异常
        }
      };

      g_ts_callback.BlockingCall(callback);
    });

    return Napi::Boolean::New(env, result);
  }

  // 停止捕获
  Napi::Value StopCapture(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    bool result = false;
    try {
      result = g_audio_capture->StopCapture();

      // 释放线程安全函数
      if (g_ts_callback) {
        g_ts_callback.Release();
      }
    } catch (const std::exception &e) {
      // 确保即使发生异常也能释放资源
      if (g_ts_callback) {
        try {
          g_ts_callback.Release();
        } catch (...) {
          // 忽略释放时的异常
        }
      }
    }

    return Napi::Boolean::New(env, result);
  }

  // 检查是否正在捕获
  Napi::Value IsCapturing(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    bool result = g_audio_capture->IsCapturing();
    return Napi::Boolean::New(env, result);
  }
};

// 初始化插件
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  return AudioCaptureAddon::Init(env, exports);
}

// 注册初始化函数
NODE_API_MODULE(audio_capture, Init)
