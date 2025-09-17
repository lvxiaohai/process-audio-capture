import { exposeAudioCaptureApi } from 'process-audio-capture/preload'

// 暴露音频捕获 API 给渲染进程
exposeAudioCaptureApi()
