import type { AudioData } from 'process-audio-capture'

/**
 * 将Uint8Array转换为Base64字符串
 */
export function arrayBufferToBase64(buffer: ArrayBuffer): string {
  let binary = ''
  const bytes = new Uint8Array(buffer)
  const len = bytes.byteLength
  for (let i = 0; i < len; i++) {
    binary += String.fromCharCode(bytes[i])
  }
  return window.btoa(binary)
}

/**
 * 创建WAV文件
 */
export function createWAV(
  audioBuffers: AudioData[],
  channels: number,
  sampleRate: number
): ArrayBuffer {
  if (!audioBuffers || audioBuffers.length === 0) {
    throw new Error('没有音频缓冲区数据')
  }

  // 计算转换后的数据长度（转换为16位）
  let totalFrames = 0 // 音频帧数（一帧包含所有声道的数据）

  audioBuffers.forEach((item) => {
    const buffer = item.buffer // Float32Array
    const samples = buffer.length
    const frames = samples / channels // 帧数 = 样本数 / 声道数
    totalFrames += frames
  })

  // 转换后的数据长度（16位 = 2字节每样本，样本数 = 帧数 * 声道数）
  const totalSamples = totalFrames * channels
  const totalLength = totalSamples * 2

  // WAV文件头大小
  const headerSize = 44

  // 创建WAV文件缓冲区
  const wav = new ArrayBuffer(headerSize + totalLength)
  const view = new DataView(wav)

  // 写入WAV文件头
  // "RIFF" 标识
  view.setUint8(0, 'R'.charCodeAt(0))
  view.setUint8(1, 'I'.charCodeAt(0))
  view.setUint8(2, 'F'.charCodeAt(0))
  view.setUint8(3, 'F'.charCodeAt(0))

  // 文件大小
  view.setUint32(4, 36 + totalLength, true)

  // "WAVE" 标识
  view.setUint8(8, 'W'.charCodeAt(0))
  view.setUint8(9, 'A'.charCodeAt(0))
  view.setUint8(10, 'V'.charCodeAt(0))
  view.setUint8(11, 'E'.charCodeAt(0))

  // "fmt " 子块
  view.setUint8(12, 'f'.charCodeAt(0))
  view.setUint8(13, 'm'.charCodeAt(0))
  view.setUint8(14, 't'.charCodeAt(0))
  view.setUint8(15, ' '.charCodeAt(0))

  // 子块大小
  view.setUint32(16, 16, true)

  // 音频格式 (1 表示 PCM)
  view.setUint16(20, 1, true)

  // 声道数
  view.setUint16(22, channels, true)

  // 采样率
  view.setUint32(24, sampleRate, true)

  // 根据实际数据计算每个样本的字节数
  // CoreAudio给出的格式：32位浮点，但我们要转换为16位整数WAV
  const outputBytesPerSample = 2 // 输出为16位音频
  const outputBitsPerSample = 16

  // 字节率 (采样率 * 声道数 * 每个样本的字节数)
  view.setUint32(28, sampleRate * channels * outputBytesPerSample, true)

  // 块对齐 (声道数 * 每个样本的字节数)
  view.setUint16(32, channels * outputBytesPerSample, true)

  // 每个样本的位数
  view.setUint16(34, outputBitsPerSample, true)

  // "data" 子块
  view.setUint8(36, 'd'.charCodeAt(0))
  view.setUint8(37, 'a'.charCodeAt(0))
  view.setUint8(38, 't'.charCodeAt(0))
  view.setUint8(39, 'a'.charCodeAt(0))

  // 数据大小
  view.setUint32(40, totalLength, true)

  // 写入音频数据 - 转换格式
  let offset = 44

  audioBuffers.forEach((item, bufferIndex) => {
    if (!item || !item.buffer) {
      throw new Error(`缓冲区 ${bufferIndex} 无效或缺少buffer属性`)
    }

    const floatData = item.buffer // 现在直接是Float32Array

    for (let i = 0; i < floatData.length; i++) {
      // 检查边界
      if (offset + 2 > wav.byteLength) {
        throw new Error(`WAV缓冲区空间不足，在样本${i}处超出边界`)
      }

      // 将浮点数 [-1.0, 1.0] 转换为16位整数 [-32768, 32767]
      let sample = Math.max(-1, Math.min(1, floatData[i])) // 限制范围
      sample = sample * 32767 // 缩放到16位范围
      const intSample = Math.round(sample)

      // 以小端序写入16位整数
      view.setInt16(offset, intSample, true)
      offset += 2
    }
  })

  return wav
}
