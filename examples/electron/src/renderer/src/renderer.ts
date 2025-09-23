import type { AudioData } from 'process-audio-capture'
import { arrayBufferToBase64, createWAV } from './utils'

// 应用程序状态接口
export interface AppState {
  selectedPid: number | null
  isCapturing: boolean
  audioDataCount: number
  isRecording: boolean
  audioBuffers: AudioData[]
  recordingStartTime: number
}

// 应用程序状态
const appState: AppState = {
  selectedPid: null,
  isCapturing: false,
  audioDataCount: 0,
  isRecording: false,
  audioBuffers: [],
  recordingStartTime: 0
}

// 更新权限状态显示
function updatePermissionStatus(status: { status: string }): void {
  const permissionStatusElement = document.getElementById('permissionStatus')
  if (!permissionStatusElement) return

  permissionStatusElement.innerHTML = `
    权限状态: <span class="status-${status.status}">${status.status}</span>
  `

  // 根据权限状态更新UI
  const getProcessesButton = document.getElementById('getProcessesButton') as HTMLButtonElement
  if (getProcessesButton) {
    getProcessesButton.disabled = status.status !== 'authorized'
  }

  if (status.status !== 'authorized') {
    const processTableBody = document.getElementById('processTableBody')
    if (processTableBody) {
      processTableBody.innerHTML = '<tr><td colspan="5">需要音频捕获权限才能获取进程列表</td></tr>'
    }
  }
}

// 计算当前录制的实际时长
function calculateRecordingDuration(): number {
  if (appState.audioBuffers.length === 0) return 0

  const firstAudio = appState.audioBuffers[0]
  const sampleRate = firstAudio.sampleRate || 44100
  const channels = firstAudio.channels || 2

  // 计算总帧数
  let totalFrames = 0
  appState.audioBuffers.forEach((audioData) => {
    const samples = audioData.buffer.length
    const frames = samples / channels
    totalFrames += frames
  })

  // 根据帧数和采样率计算实际时长
  return totalFrames / sampleRate
}

// 停止录制函数
function stopRecording(): void {
  appState.isRecording = false

  // 更新UI
  const startRecordButton = document.getElementById('startRecordButton') as HTMLButtonElement
  const stopRecordButton = document.getElementById('stopRecordButton') as HTMLButtonElement
  const playAudioButton = document.getElementById('playAudioButton') as HTMLButtonElement
  const recordingStatus = document.getElementById('recordingStatus')
  const audioPlayer = document.getElementById('audioPlayer') as HTMLAudioElement

  if (startRecordButton) startRecordButton.disabled = false
  if (stopRecordButton) stopRecordButton.disabled = true

  // 基于音频数据计算实际录制时长
  const actualDuration = calculateRecordingDuration()
  if (recordingStatus) {
    if (actualDuration > 0) {
      recordingStatus.textContent = `录制完成，时长: ${actualDuration.toFixed(2)} 秒`
    } else {
      recordingStatus.textContent = '录制完成，但未能计算时长'
    }
  }

  // 如果有录制的音频数据
  if (appState.audioBuffers.length > 0) {
    // 使用实际的音频格式参数
    const firstAudio = appState.audioBuffers[0]
    const channels = firstAudio.channels || 2
    const sampleRate = firstAudio.sampleRate || 44100

    // 检查是否所有缓冲区的格式都一致
    let allSameFormat = true
    appState.audioBuffers.forEach((item) => {
      if (item.channels !== channels || item.sampleRate !== sampleRate) {
        allSameFormat = false
      }
    })

    if (!allSameFormat) {
      console.warn('音频格式不一致，可能导致播放问题')
    }

    // 创建WAV文件（传递完整的audioBuffers，不是提取的buffers）
    let wavBuffer: ArrayBuffer
    try {
      wavBuffer = createWAV(appState.audioBuffers, channels, sampleRate)
    } catch (error) {
      console.error('创建WAV文件时出错:', error)
      if (recordingStatus) {
        recordingStatus.textContent = `录制失败: ${(error as Error).message}`
      }
      return
    }

    // 创建Blob对象
    const blob = new Blob([wavBuffer], { type: 'audio/wav' })

    // 创建URL并设置给audio元素
    const url = URL.createObjectURL(blob)
    if (audioPlayer) {
      audioPlayer.src = url
    }

    // 启用播放按钮
    if (playAudioButton) {
      playAudioButton.disabled = false
    }
  } else {
    if (recordingStatus) {
      recordingStatus.textContent = '录制失败：没有捕获到音频数据'
    }
    if (playAudioButton) {
      playAudioButton.disabled = true
    }
  }
}

// 文档加载完成后执行
document.addEventListener('DOMContentLoaded', () => {
  // 设置音频数据监听器（只设置一次）
  const unsubscribe = window.processAudioCapture.on('audio-data', (data: AudioData) => {
    // 只有在捕获状态下才处理音频数据
    if (!appState.isCapturing) return

    appState.audioDataCount++

    // 检查音频数据是否有效
    if (data.buffer && data.buffer.length > 0) {
      const floatData = data.buffer
      const nonZeroCount = floatData.filter((v) => v !== 0).length

      // 只在数据异常时输出警告
      if (nonZeroCount === 0 && appState.audioDataCount % 100 === 1) {
        console.warn('接收到全零音频数据')
      }
    }

    // 添加新的日志条目（限制日志数量以提高性能）
    const audioLog = document.getElementById('audioLog')
    if (audioLog && appState.audioDataCount % 10 === 1) {
      const entry = document.createElement('div')
      const sampleCount = data.buffer ? data.buffer.length : 0
      const frameCount = sampleCount / data.channels
      const duration = frameCount / data.sampleRate

      entry.textContent = `音频数据: ${data.channels}通道, ${data.sampleRate}Hz, ${duration.toFixed(2)}秒`

      // 限制日志条目数量
      if (audioLog.children.length > 50) {
        audioLog.removeChild(audioLog.firstChild!)
      }

      audioLog.appendChild(entry)
      audioLog.scrollTop = audioLog.scrollHeight
    }

    // 如果正在录制，保存音频数据和格式信息
    if (appState.isRecording && data.buffer) {
      // 保存Float32Array数据
      appState.audioBuffers.push({
        buffer: data.buffer, // 现在是Float32Array
        channels: data.channels,
        sampleRate: data.sampleRate
      })
    }
  })

  window.addEventListener('beforeunload', () => {
    // 不使用时取消订阅
    unsubscribe()
  })

  // 检查权限
  const checkPermissionButton = document.getElementById('checkPermissionButton')
  checkPermissionButton?.addEventListener('click', async () => {
    try {
      const permission = await window.processAudioCapture.checkPermission()
      updatePermissionStatus(permission)
    } catch (error) {
      const permissionStatus = document.getElementById('permissionStatus')
      if (permissionStatus) {
        permissionStatus.textContent = `检查权限出错: ${error}`
      }
    }
  })

  // 请求权限
  const requestPermissionButton = document.getElementById('requestPermissionButton')
  requestPermissionButton?.addEventListener('click', async () => {
    try {
      const permissionStatus = document.getElementById('permissionStatus')
      if (permissionStatus) {
        permissionStatus.textContent = '正在请求权限...'
      }
      const permission = await window.processAudioCapture.requestPermission()
      updatePermissionStatus(permission)
    } catch (error) {
      const permissionStatus = document.getElementById('permissionStatus')
      if (permissionStatus) {
        permissionStatus.textContent = `请求权限出错: ${error}`
      }
    }
  })

  // 初始检查权限
  window.processAudioCapture.checkPermission().then(updatePermissionStatus)

  // 获取进程列表
  const getProcessesButton = document.getElementById('getProcessesButton')
  const processTableBody = document.getElementById('processTableBody')
  const captureStatus = document.getElementById('captureStatus')

  getProcessesButton?.addEventListener('click', async () => {
    try {
      // 先检查权限
      const permission = await window.processAudioCapture.checkPermission()
      if (permission.status !== 'authorized') {
        if (processTableBody) {
          processTableBody.innerHTML =
            '<tr><td colspan="5">需要音频捕获权限才能获取进程列表</td></tr>'
        }
        return
      }

      const processes = await window.processAudioCapture.getProcessList()

      if (processes.length === 0) {
        if (processTableBody) {
          processTableBody.innerHTML = '<tr><td colspan="5">没有找到可捕获音频的进程</td></tr>'
        }
        return
      }

      if (processTableBody) {
        processTableBody.innerHTML = ''
        processes.forEach((process) => {
          const row = document.createElement('tr')

          // 创建图标单元格
          const iconCell = document.createElement('td')
          if (process.icon && process.icon.data) {
            // 将图标数据转换为Base64并创建图像元素
            const base64Icon = arrayBufferToBase64(process.icon.data.buffer as ArrayBuffer)
            const imgSrc = `data:image/${process.icon.format};base64,${base64Icon}`

            const iconImg = document.createElement('img')
            iconImg.src = imgSrc
            iconImg.className = 'app-icon'
            iconImg.alt = process.name

            iconCell.appendChild(iconImg)
          } else {
            iconCell.textContent = '无图标'
          }

          // 创建其他单元格
          const pidCell = document.createElement('td')
          pidCell.textContent = process.pid.toString()

          const nameCell = document.createElement('td')
          nameCell.textContent = process.name

          const descCell = document.createElement('td')
          descCell.textContent = process.description

          const actionCell = document.createElement('td')
          const selectButton = document.createElement('button')
          selectButton.textContent = '选择'
          selectButton.className = 'selectProcessButton'
          actionCell.appendChild(selectButton)

          // 添加所有单元格到行
          row.appendChild(iconCell)
          row.appendChild(pidCell)
          row.appendChild(nameCell)
          row.appendChild(descCell)
          row.appendChild(actionCell)

          // 添加选择按钮事件监听器
          selectButton.addEventListener('click', () => {
            // 移除之前选中的行的高亮
            document.querySelectorAll('#processTable tr.selected').forEach((tr) => {
              tr.classList.remove('selected')
            })

            // 高亮当前选中的行
            row.classList.add('selected')

            // 保存选中的进程ID
            appState.selectedPid = process.pid

            // 更新UI状态
            const startCaptureButton = document.getElementById(
              'startCaptureButton'
            ) as HTMLButtonElement
            if (startCaptureButton) {
              startCaptureButton.disabled = false
            }
            if (captureStatus) {
              captureStatus.textContent = `已选择进程: ${process.name} (PID: ${process.pid})`
            }
          })

          processTableBody.appendChild(row)
        })
      }
    } catch (error) {
      if (processTableBody) {
        processTableBody.innerHTML = `<tr><td colspan="5">获取进程列表时出错: ${error}</td></tr>`
      }
    }
  })

  // 开始捕获
  const startCaptureButton = document.getElementById('startCaptureButton')
  const stopCaptureButton = document.getElementById('stopCaptureButton')
  const audioLog = document.getElementById('audioLog')
  const startRecordButton = document.getElementById('startRecordButton')
  const stopRecordButton = document.getElementById('stopRecordButton')
  const playAudioButton = document.getElementById('playAudioButton')
  const recordingStatus = document.getElementById('recordingStatus')
  const audioPlayer = document.getElementById('audioPlayer') as HTMLAudioElement

  startCaptureButton?.addEventListener('click', async () => {
    if (!appState.selectedPid) {
      alert('请先选择一个进程')
      return
    }

    try {
      // 先检查权限
      const permission = await window.processAudioCapture.checkPermission()
      if (permission.status !== 'authorized') {
        if (captureStatus) {
          captureStatus.textContent = '需要音频捕获权限才能开始捕获'
        }
        return
      }

      const result = await window.processAudioCapture.startCapture(appState.selectedPid)

      if (result) {
        appState.isCapturing = true
        if (startCaptureButton) (startCaptureButton as HTMLButtonElement).disabled = true
        if (stopCaptureButton) (stopCaptureButton as HTMLButtonElement).disabled = false
        if (startRecordButton) (startRecordButton as HTMLButtonElement).disabled = false
        if (captureStatus) {
          captureStatus.textContent = `正在捕获PID为${appState.selectedPid}的进程音频...`
        }

        // 清空音频日志
        if (audioLog) {
          audioLog.textContent = ''
        }
        appState.audioDataCount = 0
      } else {
        if (captureStatus) {
          captureStatus.textContent = '开始捕获失败'
        }
      }
    } catch (error) {
      if (captureStatus) {
        captureStatus.textContent = `开始捕获时出错: ${error}`
      }
    }
  })

  // 停止捕获
  stopCaptureButton?.addEventListener('click', async () => {
    try {
      const result = await window.processAudioCapture.stopCapture()

      if (result) {
        appState.isCapturing = false
        if (startCaptureButton) (startCaptureButton as HTMLButtonElement).disabled = false
        if (stopCaptureButton) (stopCaptureButton as HTMLButtonElement).disabled = true
        if (startRecordButton) (startRecordButton as HTMLButtonElement).disabled = true
        if (stopRecordButton) (stopRecordButton as HTMLButtonElement).disabled = true
        if (captureStatus) {
          captureStatus.textContent = '已停止捕获'
        }

        // 如果正在录制，也停止录制
        if (appState.isRecording) {
          stopRecording()
        }
      } else {
        if (captureStatus) {
          captureStatus.textContent = '停止捕获失败'
        }
      }
    } catch (error) {
      if (captureStatus) {
        captureStatus.textContent = `停止捕获时出错: ${error}`
      }
    }
  })

  // 开始录制
  startRecordButton?.addEventListener('click', () => {
    if (!appState.isCapturing) {
      alert('请先开始捕获音频')
      return
    }

    // 清空之前的录制
    appState.audioBuffers = []
    appState.recordingStartTime = Date.now()
    appState.isRecording = true

    // 更新UI
    if (startRecordButton) (startRecordButton as HTMLButtonElement).disabled = true
    if (stopRecordButton) (stopRecordButton as HTMLButtonElement).disabled = false
    if (playAudioButton) (playAudioButton as HTMLButtonElement).disabled = true
    if (recordingStatus) {
      recordingStatus.textContent = '正在录制...'
    }
  })

  // 停止录制
  stopRecordButton?.addEventListener('click', () => {
    if (!appState.isRecording) {
      return
    }

    stopRecording()
  })

  // 播放录制的音频
  playAudioButton?.addEventListener('click', () => {
    if (audioPlayer && audioPlayer.src) {
      audioPlayer.play()
    }
  })
})

document.addEventListener('DOMContentLoaded', () => {
  const unsubscribe = window.processAudioCapture.on('capturing', (capturing) => {
    console.log('capturing', capturing)
  })
  window.addEventListener('beforeunload', () => {
    // 不使用时取消订阅
    unsubscribe()
  })
})
