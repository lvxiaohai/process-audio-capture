const { contextBridge, ipcRenderer } = require('electron');

// 音频数据消息接口
/**
 * @typedef {Object} AudioDataMessage
 * @property {number} timestamp - 时间戳
 * @property {number} dataSize - 数据大小（字节数）
 * @property {number} channels - 音频通道数
 * @property {number} sampleRate - 采样率（Hz）
 */

/**
 * @typedef {Object} ProcessInfo
 * @property {number} pid - 进程ID
 * @property {string} name - 进程名称
 * @property {string} description - 进程描述
 * @property {string} path - 进程路径
 * @property {Object} icon - 进程图标
 * @property {Uint8Array} icon.data - 图标数据
 * @property {string} icon.format - 图标格式
 * @property {number} icon.width - 图标宽度
 * @property {number} icon.height - 图标高度
 */

/**
 * @typedef {Object} PermissionStatus
 * @property {string} status - 权限状态: 'authorized', 'denied', 'unknown'
 */

// 定义暴露给渲染进程的API
const electronAPI = {
  // 检查音频捕获权限
  checkPermission: () => 
    ipcRenderer.invoke("check-permission"),
  
  // 请求音频捕获权限
  requestPermission: () => 
    ipcRenderer.invoke("request-permission"),
    
  // Hello World测试方法
  helloWorld: (message) => 
    ipcRenderer.invoke("hello-world", message),

  // 获取进程列表
  getProcessList: () => 
    ipcRenderer.invoke("get-process-list"),

  // 开始捕获指定进程的音频
  startCapture: (pid) => 
    ipcRenderer.invoke("start-capture", pid),

  // 停止捕获
  stopCapture: () => ipcRenderer.invoke("stop-capture"),

  // 监听音频数据
  onAudioData: (callback) => {
    ipcRenderer.on(
      "audio-data",
      (_event, data) => callback(data)
    );
  },

  // 移除音频数据监听器
  removeAudioDataListener: () => {
    ipcRenderer.removeAllListeners("audio-data");
  },
};

// 暴露安全的API给渲染进程
contextBridge.exposeInMainWorld("electronAPI", electronAPI);