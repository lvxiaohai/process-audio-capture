const { audioCapture } = require('process-audio-capture');
const fs = require('fs');
const wav = require('wav');

console.log('=== Process Audio Capture Node.js 示例 ===\n');

// 检查平台支持
if (!audioCapture.isPlatformSupported()) {
    console.error('❌ 当前平台不支持音频捕获功能');
    process.exit(1);
}

console.log('✅ 平台支持音频捕获功能');

async function main() {
    try {
        // 检查权限状态
        console.log('\n📋 检查音频捕获权限...');
        let permission = audioCapture.checkPermission();
        console.log(`当前权限状态: ${permission.status}`);

        // 如果没有权限，请求权限
        if (permission.status !== 'authorized') {
            console.log('🔐 请求音频捕获权限...');
            permission = await audioCapture.requestPermission();
            console.log(`权限请求结果: ${permission.status}`);
            
            if (permission.status !== 'authorized') {
                console.error('❌ 未获得音频捕获权限，程序退出');
                process.exit(1);
            }
        }

        console.log('✅ 已获得音频捕获权限');

        // 获取进程列表
        console.log('\n📋 获取可捕获音频的进程列表...');
        const processes = audioCapture.getProcessList();
        
        if (processes.length === 0) {
            console.log('⚠️  没有找到可捕获音频的进程');
            return;
        }

        console.log(`找到 ${processes.length} 个可捕获音频的进程:\n`);
        processes.forEach((process, index) => {
            console.log(`${index + 1}. ${process.name} (PID: ${process.pid})`);
            console.log(`   路径: ${process.path}`);
            if (process.description) {
                console.log(`   描述: ${process.description}`);
            }
            console.log('');
        });

        // 选择第一个进程进行捕获演示
        if (processes.length > 0) {
            const targetProcess = processes[0];
            console.log(`🎯 开始捕获进程 "${targetProcess.name}" (PID: ${targetProcess.pid}) 的音频...\n`);

            let audioDataCount = 0;
            let totalSamples = 0;
            let wavWriter = null;
            let audioParams = null;
            
            // 开始捕获音频
            const success = audioCapture.startCapture(targetProcess.pid, (audioData) => {
                audioDataCount++;
                totalSamples += audioData.buffer.length;
                
                // 第一次接收到音频数据时，创建WAV文件写入器
                if (!wavWriter && audioData.buffer.length > 0) {
                    audioParams = {
                        channels: audioData.channels,
                        sampleRate: audioData.sampleRate,
                        bitDepth: 16  // 16位深度
                    };
                    
                    console.log(`🎵 创建WAV文件: capture.wav`);
                    console.log(`   - 通道数: ${audioParams.channels}`);
                    console.log(`   - 采样率: ${audioParams.sampleRate}Hz`);
                    console.log(`   - 位深度: ${audioParams.bitDepth}位\n`);
                    
                    wavWriter = new wav.Writer(audioParams);
                    globalWavWriter = wavWriter;  // 保存到全局变量供清理函数使用
                    wavWriter.pipe(fs.createWriteStream('capture.wav'));
                }
                
                // 将Float32Array转换为16位PCM数据并写入WAV文件
                if (wavWriter) {
                    const int16Buffer = new Int16Array(audioData.buffer.length);
                    for (let i = 0; i < audioData.buffer.length; i++) {
                        // 将-1到1的浮点数转换为-32768到32767的16位整数
                        int16Buffer[i] = Math.max(-32768, Math.min(32767, Math.round(audioData.buffer[i] * 32767)));
                    }
                    
                    // 创建Buffer并写入WAV文件
                    const buffer = Buffer.from(int16Buffer.buffer);
                    wavWriter.write(buffer);
                }
                
                // 计算音频数据的RMS值（用于显示音量级别）
                let sum = 0;
                for (let i = 0; i < audioData.buffer.length; i++) {
                    sum += audioData.buffer[i] * audioData.buffer[i];
                }
                const rms = Math.sqrt(sum / audioData.buffer.length);
                const volume = Math.round(rms * 100);
                
                // 创建简单的音量条显示
                const maxBars = 20;
                const bars = Math.round((volume / 100) * maxBars);
                const volumeBar = '█'.repeat(bars) + '░'.repeat(maxBars - bars);
                
                process.stdout.write(`\r🎵 音频数据 #${audioDataCount.toString().padStart(4)} | ` +
                    `通道: ${audioData.channels} | 采样率: ${audioData.sampleRate}Hz | ` +
                    `音量: [${volumeBar}] ${volume.toString().padStart(3)}%`);
            });

            if (success) {
                console.log('✅ 音频捕获开始成功');
                console.log('📊 实时音频数据显示 (按 Ctrl+C 停止):');
                console.log('');
                
                // 设置定时器，10秒后自动停止捕获
                setTimeout(() => {
                    console.log('\n\n⏰ 10秒演示时间结束，停止音频捕获...');
                    const stopSuccess = audioCapture.stopCapture();
                    
                    // 关闭WAV文件写入器
                    if (wavWriter) {
                        wavWriter.end();
                        console.log('💾 WAV文件已保存: capture.wav');
                    }
                    
                    if (stopSuccess) {
                        console.log('✅ 音频捕获已停止');
                        console.log(`\n📈 统计信息:`);
                        console.log(`   - 总计接收到 ${audioDataCount} 个音频数据包`);
                        console.log(`   - 总计处理 ${totalSamples.toLocaleString()} 个音频样本`);
                        
                        if (audioParams) {
                            const durationSeconds = totalSamples / audioParams.sampleRate / audioParams.channels;
                            console.log(`   - 录制时长: ${durationSeconds.toFixed(2)} 秒`);
                        }
                    } else {
                        console.log('❌ 停止音频捕获失败');
                    }
                    
                    console.log('\n🎉 演示完成！');
                    console.log('📁 请检查当前目录下的 capture.wav 文件');
                }, 10000);
                
            } else {
                console.log('❌ 音频捕获启动失败');
            }
        }

    } catch (error) {
        console.error('❌ 发生错误:', error.message);
        console.error(error.stack);
    }
}

// 全局变量，用于在信号处理中访问WAV写入器
let globalWavWriter = null;

// 清理函数
function cleanup() {
    audioCapture.stopCapture();
    if (globalWavWriter) {
        globalWavWriter.end();
        console.log('💾 WAV文件已保存: capture.wav');
    }
}

// 处理程序退出信号
process.on('SIGINT', () => {
    console.log('\n\n⚠️  收到中断信号，正在清理...');
    cleanup();
    console.log('👋 程序退出');
    process.exit(0);
});

process.on('SIGTERM', () => {
    console.log('\n\n⚠️  收到终止信号，正在清理...');
    cleanup();
    console.log('👋 程序退出');
    process.exit(0);
});

// 运行主程序
main().catch((error) => {
    console.error('❌ 程序执行失败:', error.message);
    console.error(error.stack);
    process.exit(1);
});
