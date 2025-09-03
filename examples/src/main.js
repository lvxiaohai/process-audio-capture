#!/usr/bin/env node

// 这是一个简单的启动脚本，用于直接运行 Electron 应用程序
const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');

// 设置全局错误处理
process.on('uncaughtException', (error) => {
  console.error('未捕获的异常:', error);
  if (app.isReady()) {
    dialog.showErrorBox('未捕获的异常', `${error.message}\n${error.stack}`);
  }
});

// 尝试加载原生模块
let audioCapture = null;
try {
  console.log('尝试加载 electron-audio-capture 模块');
  audioCapture = require('electron-audio-capture');
  console.log('成功加载 electron-audio-capture 模块');
} catch (error) {
  console.error('加载 electron-audio-capture 模块失败:', error);
  if (app.isReady()) {
    dialog.showErrorBox('模块加载错误', `无法加载 electron-audio-capture 模块: ${error.message}`);
  }
}

// 保持对window对象的全局引用
let mainWindow = null;

// 创建浏览器窗口
function createWindow() {
  mainWindow = new BrowserWindow({
    width: 900,
    height: 700,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    }
  });

  // 加载 HTML 文件
  mainWindow.loadFile(path.join(__dirname, '../index.html'));

  // 打开开发者工具
  mainWindow.webContents.openDevTools();

  // 当窗口关闭时触发
  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

// 当 Electron 完成初始化并准备创建浏览器窗口时调用此方法
app.whenReady().then(() => {
  try {
    // 检查是否成功加载了模块
    if (audioCapture) {
      // 检查音频捕获权限
      try {
        const permission = audioCapture.checkPermission();
        console.log('音频捕获权限状态:', permission);
      } catch (error) {
        console.error('检查权限失败:', error);
        dialog.showErrorBox('权限检查错误', `检查音频捕获权限失败: ${error.message}`);
      }
    }
    
    createWindow();

    app.on('activate', () => {
      if (BrowserWindow.getAllWindows().length === 0) {
        createWindow();
      }
    });
  } catch (error) {
    console.error('应用初始化错误:', error);
    dialog.showErrorBox('应用初始化错误', `${error.message}\n${error.stack}`);
  }
});

// 当所有窗口关闭时退出应用
app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

// 处理IPC消息

// 检查权限
ipcMain.handle('check-permission', async () => {
  try {
    if (!audioCapture) {
      return { status: 'unknown', error: '模块未加载' };
    }
    return audioCapture.checkPermission();
  } catch (error) {
    console.error('检查权限失败:', error);
    return { status: 'error', error: error.message };
  }
});

// 请求权限
ipcMain.handle('request-permission', () => {
  return new Promise((resolve, reject) => {
    try {
      if (!audioCapture) {
        reject(new Error('模块未加载'));
        return;
      }
      audioCapture.requestPermission((result) => {
        resolve(result);
      });
    } catch (error) {
      console.error('请求权限失败:', error);
      reject(error);
    }
  });
});

// 获取进程列表
ipcMain.handle('get-process-list', async () => {
  try {
    if (!audioCapture) {
      return [];
    }
    return audioCapture.getProcessList();
  } catch (error) {
    console.error('获取进程列表失败:', error);
    dialog.showErrorBox('获取进程列表失败', error.message);
    return [];
  }
});

// 开始捕获
ipcMain.handle('start-capture', async (event, pid) => {
  try {
    if (!audioCapture) {
      return false;
    }
    
    const result = audioCapture.startCapture(pid, (audioData) => {
      // 将音频数据发送到渲染进程
      if (mainWindow && !mainWindow.isDestroyed()) {
        mainWindow.webContents.send('audio-data', audioData);
      }
    });
    
    return result;
  } catch (error) {
    console.error('开始捕获失败:', error);
    dialog.showErrorBox('开始捕获失败', error.message);
    return false;
  }
});

// 停止捕获
ipcMain.handle('stop-capture', async () => {
  try {
    if (!audioCapture) {
      return false;
    }
    return audioCapture.stopCapture();
  } catch (error) {
    console.error('停止捕获失败:', error);
    dialog.showErrorBox('停止捕获失败', error.message);
    return false;
  }
});

// Hello World 测试
ipcMain.handle('hello-world', async (event, input) => {
  try {
    if (!audioCapture) {
      return `模块未加载: ${input}`;
    }
    return audioCapture.helloWorld(input);
  } catch (error) {
    console.error('Hello World 测试失败:', error);
    return `错误: ${error.message}`;
  }
});