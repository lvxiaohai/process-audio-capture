/**
 * IPC 通道前缀
 *
 * 用于避免与其他库的 IPC 通道名冲突
 * 所有音频捕获相关的 IPC 通道都使用此前缀
 */
export const AUDIO_CAPTURE_IPC_PREFIX = "process-audio-capture";
