const { audioCapture } = require('process-audio-capture');
const fs = require('fs');
const wav = require('wav');
const readline = require('readline');

console.log('=== Process Audio Capture Windows EXE 示例 ===\n');

// 检查平台支持
if (!audioCapture.isPlatformSupported()) {
    console.error('❌ 当前平台不支持音频捕获功能');
    console.log('\n按任意键退出...');
    process.stdin.once('data', () => process.exit(1));
    return;
}

console.log('✅ 平台支持音频捕获功能');

// 全局变量
let globalWavWriter = null;
let isCapturing = false;

// 创建readline接口用于用户交互
const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

// 清理函数
function cleanup() {
    if (isCapturing) {
        audioCapture.stopCapture();
        isCapturing = false;
    }
    if (globalWavWriter) {
        globalWavWriter.end();
        console.log('\n💾 WAV文件已保存');
        globalWavWriter = null;
    }
}

// 处理程序退出信号
process.on('SIGINT', () => {
    console.log('\n\n⚠️  收到中断信号，正在清理...');
    cleanup();
    rl.close();
    console.log('👋 程序退出');
    process.exit(0);
});

process.on('SIGTERM', () => {
    console.log('\n\n⚠️  收到终止信号，正在清理...');
    cleanup();
    rl.close();
    console.log('👋 程序退出');
    process.exit(0);
});

// 等待用户输入
function waitForInput(prompt) {
    return new Promise((resolve) => {
        rl.question(prompt, (answer) => {
            resolve(answer);
        });
    });
}

// 开始录制音频
function startRecording(targetProcess) {
    console.log(`\n🎯 开始捕获进程 "${targetProcess.name}" (PID: ${targetProcess.pid}) 的音频...\n`);

    let audioDataCount = 0;
    let totalSamples = 0;
    let audioParams = null;
    const outputFile = `capture_${targetProcess.pid}_${Date.now()}.wav`;
    
    // 开始捕获音频
    const success = audioCapture.startCapture(targetProcess.pid, (audioData) => {
        audioDataCount++;
        totalSamples += audioData.buffer.length;
        
        // 第一次接收到音频数据时，创建WAV文件写入器
        if (!globalWavWriter && audioData.buffer.length > 0) {
            audioParams = {
                channels: audioData.channels,
                sampleRate: audioData.sampleRate,
                bitDepth: 16
            };
            
            console.log(`🎵 创建WAV文件: ${outputFile}`);
            console.log(`   - 通道数: ${audioParams.channels}`);
            console.log(`   - 采样率: ${audioParams.sampleRate}Hz`);
            console.log(`   - 位深度: ${audioParams.bitDepth}位\n`);
            
            globalWavWriter = new wav.Writer(audioParams);
            globalWavWriter.pipe(fs.createWriteStream(outputFile));
        }
        
        // 将Float32Array转换为16位PCM数据并写入WAV文件
        if (globalWavWriter) {
            const int16Buffer = new Int16Array(audioData.buffer.length);
            for (let i = 0; i < audioData.buffer.length; i++) {
                int16Buffer[i] = Math.max(-32768, Math.min(32767, Math.round(audioData.buffer[i] * 32767)));
            }
            
            const buffer = Buffer.from(int16Buffer.buffer);
            globalWavWriter.write(buffer);
        }
        
        // 计算音量级别
        let sum = 0;
        for (let i = 0; i < audioData.buffer.length; i++) {
            sum += audioData.buffer[i] * audioData.buffer[i];
        }
        const rms = Math.sqrt(sum / audioData.buffer.length);
        const volume = Math.round(rms * 100);
        
        // 显示音量条
        const maxBars = 20;
        const bars = Math.round((volume / 100) * maxBars);
        const volumeBar = '█'.repeat(bars) + '░'.repeat(maxBars - bars);
        
        process.stdout.write(`\r🎵 录制中... | 音量: [${volumeBar}] ${volume.toString().padStart(3)}% | 数据包: ${audioDataCount}`);
    });

    if (success) {
        isCapturing = true;
        console.log('✅ 音频捕获开始成功');
        console.log('📊 实时音频数据显示:');
        console.log('💡 按 Ctrl+C 停止录制\n');
        
        return {
            audioDataCount,
            totalSamples,
            audioParams,
            outputFile
        };
    } else {
        console.log('❌ 音频捕获启动失败');
        return null;
    }
}

// 主程序
async function main() {
    try {
        // 检查权限状态
        console.log('\n📋 检查音频捕获权限...');
        let permission = audioCapture.checkPermission();
        console.log(`当前权限状态: ${permission.status}`);

        // 如果没有权限，请求权限
        if (permission.status !== 'authorized') {
            console.log('🔐 请求音频捕获权限...');
            console.log('💡 请在系统弹出的权限对话框中允许访问\n');
            
            permission = await audioCapture.requestPermission();
            console.log(`权限请求结果: ${permission.status}`);
            
            if (permission.status !== 'authorized') {
                console.error('\n❌ 未获得音频捕获权限，程序退出');
                console.log('💡 请在Windows设置中手动授予录音权限：');
                console.log('   设置 → 隐私 → 麦克风 → 允许桌面应用访问麦克风');
                await waitForInput('\n按回车键退出...');
                rl.close();
                process.exit(1);
            }
        }

        console.log('✅ 已获得音频捕获权限');

        // 获取进程列表
        console.log('\n📋 获取可捕获音频的进程列表...');
        const processes = audioCapture.getProcessList();
        
        if (processes.length === 0) {
            console.log('⚠️  没有找到可捕获音频的进程');
            console.log('💡 请先打开一些正在播放音频的应用程序（如浏览器、音乐播放器等）');
            await waitForInput('\n按回车键退出...');
            rl.close();
            return;
        }

        console.log(`\n找到 ${processes.length} 个可捕获音频的进程:\n`);
        processes.forEach((process, index) => {
            console.log(`${index + 1}. ${process.name} (PID: ${process.pid})`);
            console.log(`   路径: ${process.path}`);
            if (process.description) {
                console.log(`   描述: ${process.description}`);
            }
            console.log('');
        });

        // 让用户选择要捕获的进程
        const choice = await waitForInput('请输入要捕获的进程编号 (1-' + processes.length + '): ');
        const selectedIndex = parseInt(choice) - 1;

        if (isNaN(selectedIndex) || selectedIndex < 0 || selectedIndex >= processes.length) {
            console.log('\n❌ 无效的选择');
            await waitForInput('按回车键退出...');
            rl.close();
            return;
        }

        const targetProcess = processes[selectedIndex];
        startRecording(targetProcess);

    } catch (error) {
        console.error('\n❌ 发生错误:', error.message);
        console.error(error.stack);
        await waitForInput('\n按回车键退出...');
        rl.close();
    }
}

// 运行主程序
main().catch((error) => {
    console.error('❌ 程序执行失败:', error.message);
    console.error(error.stack);
    waitForInput('\n按回车键退出...').then(() => {
        rl.close();
        process.exit(1);
    });
});

