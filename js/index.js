const EventEmitter = require('events')

// 使用'bindings'模块加载原生插件
const bindings = require('bindings')
const native = bindings('audio_capture')

// 创建一个友好的JavaScript包装器
class AudioCapture extends EventEmitter {
  constructor () {
    super()

    // 创建我们C++类的实例
    this.addon = new native.AudioCaptureAddon()
    this._capturing = false
    
    console.log('[AudioCapture::constructor] AudioCapture 实例已创建')
  }

  // 检查音频捕获权限状态
  checkPermission() {
    console.log('[AudioCapture::checkPermission] 检查音频捕获权限')
    const result = this.addon.checkPermission()
    console.log('[AudioCapture::checkPermission] 音频捕获权限状态:', result)
    return result
  }
  
  // 请求音频捕获权限
  requestPermission() {
    console.log('[AudioCapture::requestPermission] 请求音频捕获权限')
    return new Promise((resolve) => {
      this.addon.requestPermission((result) => {
        console.log('[AudioCapture::requestPermission] 音频捕获权限请求结果:', result)
        resolve(result)
        this.emit('permission-changed', result)
      })
    })
  }

  // 获取可捕获音频的进程列表
  getProcessList() {
    console.log('[AudioCapture::getProcessList] 获取进程列表')
    // 检查权限
    const permission = this.checkPermission()
    if (permission.status !== 'authorized') {
      console.error('[AudioCapture::getProcessList] 没有音频捕获权限')
      throw new Error('没有音频捕获权限')
    }
    
    const processes = this.addon.getProcessList()
    console.log(`[AudioCapture::getProcessList] 获取到 ${processes.length} 个进程`)
    return processes
  }

  // 开始捕获指定进程的音频
  startCapture(pid, callback) {
    console.log(`[AudioCapture::startCapture] 开始捕获进程 ${pid} 的音频`)
    // 检查权限
    const permission = this.checkPermission()
    if (permission.status !== 'authorized') {
      console.error('[AudioCapture::startCapture] 没有音频捕获权限')
      throw new Error('没有音频捕获权限')
    }
    
    if (this._capturing) {
      console.error('[AudioCapture::startCapture] 已经在捕获音频，请先停止')
      throw new Error('已经在捕获音频，请先停止')
    }
    
    if (typeof pid !== 'number' || pid <= 0) {
      console.error('[AudioCapture::startCapture] 进程ID必须是正整数:', pid)
      throw new TypeError('进程ID必须是正整数')
    }
    
    if (typeof callback !== 'function') {
      console.error('[AudioCapture::startCapture] 回调必须是函数')
      throw new TypeError('回调必须是函数')
    }
    
    try {
      console.log('[AudioCapture::startCapture] 调用原生模块的startCapture方法')
      const result = this.addon.startCapture(pid, (audioData) => {
        // 调用用户的回调函数
        callback(audioData)
        
        // 同时触发事件
        this.emit('audio-data', audioData)
      })
      
      console.log('[AudioCapture::startCapture] startCapture结果:', result)
      
      if (result) {
        this._capturing = true
        this.emit('capture-started', { pid })
      } else {
        console.error('[AudioCapture::startCapture] 开始捕获失败')
      }
      
      return result
    } catch (error) {
      console.error('[AudioCapture::startCapture] startCapture出错:', error)
      throw error
    }
  }

  // 停止捕获
  stopCapture() {
    console.log('[AudioCapture::stopCapture] 停止捕获')
    if (!this._capturing) {
      console.log('[AudioCapture::stopCapture] 当前没有在捕获，无需停止')
      return false
    }
    
    const result = this.addon.stopCapture()
    console.log('[AudioCapture::stopCapture] stopCapture结果:', result)
    
    if (result) {
      this._capturing = false
      this.emit('capture-stopped')
    } else {
      console.error('[AudioCapture::stopCapture] 停止捕获失败')
    }
    
    return result
  }

  // Hello World测试方法
  helloWorld(input = '') {
    console.log('[AudioCapture::helloWorld] 调用helloWorld:', input)
    if (typeof input !== 'string') {
      console.error('[AudioCapture::helloWorld] 输入必须是字符串')
      throw new TypeError('输入必须是字符串')
    }
    const result = this.addon.helloWorld(input)
    console.log('[AudioCapture::helloWorld] helloWorld结果:', result)
    return result
  }
}

// 导出一个单例实例
if (process.platform === 'darwin') {
  console.log('[module] 在macOS平台上创建AudioCapture实例')
  module.exports = new AudioCapture()
} else {
  // 为不支持的平台提供回退方案
  console.warn('[module] 此插件仅支持macOS平台')

  // 模拟接口，但不提供实际功能
  module.exports = {
    checkPermission: () => ({ status: 'unknown' }),
    requestPermission: () => Promise.resolve({ status: 'denied' }),
    getProcessList: () => [],
    startCapture: () => false,
    stopCapture: () => false,
    helloWorld: (input) => `不支持的平台，无法处理: ${input}`,
    on: () => {},
    once: () => {},
    emit: () => {}
  }
}