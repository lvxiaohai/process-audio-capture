@echo off
echo ===============================================
echo   Process Audio Capture - Windows EXE Builder
echo ===============================================
echo.

REM 检查 Node.js 是否安装
where node >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [错误] 未找到 Node.js，请先安装 Node.js
    echo 下载地址: https://nodejs.org/
    pause
    exit /b 1
)

echo [1/3] 检查依赖...
if not exist "node_modules\" (
    echo [2/3] 安装依赖...
    call npm install
    if %ERRORLEVEL% NEQ 0 (
        echo [错误] 依赖安装失败
        pause
        exit /b 1
    )
) else (
    echo [信息] 依赖已安装
)

echo [3/3] 构建 EXE 文件...
call npm run build
if %ERRORLEVEL% NEQ 0 (
    echo [错误] 构建失败
    pause
    exit /b 1
)

echo.
echo ===============================================
echo   构建完成！
echo ===============================================
echo.
echo EXE 文件位置: dist\AudioCapture.exe
echo.
echo 运行方式: dist\AudioCapture.exe
echo.

pause

