#!/bin/bash

# 设置音频捕获权限的脚本

echo "设置 Electron 应用程序的音频捕获权限..."

# 获取 Electron 可执行文件路径
ELECTRON_PATH="$(pwd)/node_modules/electron/dist/Electron.app"

if [ ! -d "$ELECTRON_PATH" ]; then
    echo "错误: 找不到 Electron 应用程序路径: $ELECTRON_PATH"
    exit 1
fi

echo "找到 Electron 路径: $ELECTRON_PATH"

# 复制我们的 Info.plist 到 Electron.app
ELECTRON_INFO_PLIST="$ELECTRON_PATH/Contents/Info.plist"
OUR_INFO_PLIST="$(pwd)/Info.plist"

if [ -f "$OUR_INFO_PLIST" ]; then
    echo "备份原始 Info.plist..."
    cp "$ELECTRON_INFO_PLIST" "$ELECTRON_INFO_PLIST.backup"
    
    echo "复制我们的 Info.plist..."
    cp "$OUR_INFO_PLIST" "$ELECTRON_INFO_PLIST"
    
    echo "更新 Info.plist 完成"
else
    echo "错误: 找不到我们的 Info.plist 文件: $OUR_INFO_PLIST"
    exit 1
fi

# 检查是否需要代码签名
echo "检查代码签名状态..."
codesign -dv "$ELECTRON_PATH" 2>&1 | grep -q "code object is not signed"
if [ $? -eq 0 ]; then
    echo "Electron 应用程序未签名，这可能会导致音频捕获权限问题"
    echo "对于开发测试，可以尝试在系统偏好设置中手动授予权限"
else
    echo "Electron 应用程序已签名"
fi

echo "权限设置完成！"
echo ""
echo "请注意："
echo "1. 首次运行时，系统可能会弹出权限请求对话框"
echo "2. 如果权限被拒绝，请到 系统偏好设置 > 安全性与隐私 > 隐私 > 麦克风 中手动授予权限"
echo "3. 对于进程音频捕获，可能还需要额外的系统级权限"
echo ""
