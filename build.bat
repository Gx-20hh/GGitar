@echo off
setlocal EnableExtensions EnableDelayedExpansion
chcp 65001 >nul
cd /d "%~dp0"

echo ==================================================
echo   GGitar 一键编译
echo ==================================================
echo.

rem ---- 1. 找 Visual Studio（需要“使用 C++ 的桌面开发”） ----
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [错误] 没有找到 Visual Studio。
    echo 请安装 Visual Studio 2022 或更新版本（免费的 Community 版即可），
    echo 安装时勾选“使用 C++ 的桌面开发”。
    goto :fail
)

set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo [错误] Visual Studio 里没有装 C++ 编译工具。
    echo 打开 Visual Studio Installer -^> 修改 -^> 勾选“使用 C++ 的桌面开发”。
    goto :fail
)
echo 使用 Visual Studio: %VSDIR%

rem ---- 2. 找 CMake：优先用 Visual Studio 自带的 ----
set "CMAKE=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" (
    where cmake >nul 2>nul
    if errorlevel 1 (
        echo [错误] 没有找到 CMake。
        echo 在 Visual Studio Installer 里勾选“适用于 Windows 的 C++ CMake 工具”，
        echo 或者从 https://cmake.org/download/ 安装 CMake。
        goto :fail
    )
    set "CMAKE=cmake"
)

rem ---- 2b. 检查 Windows SDK（链接需要 kernel32.lib 等系统库） ----
set "HAS_SDK="
set "KITS=%ProgramFiles(x86)%\Windows Kits\10\"
for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots" /v KitsRoot10 2^>nul ^| findstr KitsRoot10') do set "KITS=%%b"
for /d %%v in ("%KITS%Lib\10.*") do (
    if exist "%%~v\um\x64\kernel32.lib" set "HAS_SDK=1"
)
if not defined HAS_SDK (
    echo [错误] 没有找到 Windows SDK。
    echo 打开 Visual Studio Installer -^> 修改 -^> 单个组件，
    echo 搜索 “Windows 11 SDK”，勾选最新的那个版本，点“修改”安装后再运行本脚本。
    goto :fail
)

rem ---- 3. 把项目目录映射成一个临时盘符，避开中文路径带来的工具兼容问题 ----
set "DRV="
for %%D in (L K J Q R S T U V W X Y Z) do (
    if not defined DRV if not exist %%D:\ set "DRV=%%D"
)
if not defined DRV (
    echo [错误] 找不到空闲盘符用于临时映射。
    goto :fail
)
subst %DRV%: "%~dp0."
if errorlevel 1 (
    echo [错误] 映射盘符 %DRV%: 失败。
    goto :fail
)
pushd %DRV%:\

rem ---- 4. 解压 JUCE 和 NAM ----
if not exist "third_party\JUCE\CMakeLists.txt" (
    echo 正在解压 JUCE ...
    tar -xf "third_party\JUCE-9.0.4.zip" -C "third_party"
    if errorlevel 1 (
        echo [错误] 解压 third_party\JUCE-9.0.4.zip 失败。
        goto :cleanup_fail
    )
)

if not exist "third_party\nam\NAM\dsp.h" (
    echo 正在解压 NAM 箱头模型库 ...
    tar -xf "third_party\NAM-core.zip" -C "third_party"
    if errorlevel 1 (
        echo [错误] 解压 third_party\NAM-core.zip 失败。
        goto :cleanup_fail
    )
)

rem ---- 5. 上次配置没完成，或者盘符变了，旧的 CMake 缓存要清掉 ----
if exist "build\CMakeCache.txt" if not exist "build\cmake_install.cmake" (
    echo 清理上次未完成的配置 ...
    del /q "build\CMakeCache.txt"
    if exist "build\CMakeFiles" rmdir /s /q "build\CMakeFiles"
)
if exist "build\CMakeCache.txt" (
    findstr /c:"CMAKE_HOME_DIRECTORY:INTERNAL=%DRV%:/" "build\CMakeCache.txt" >nul
    if errorlevel 1 (
        echo 清理旧的编译缓存 ...
        del /q "build\CMakeCache.txt"
        if exist "build\CMakeFiles" rmdir /s /q "build\CMakeFiles"
    )
)

rem ---- 6. 配置 + 编译 ----
echo.
echo [1/2] 配置项目（第一次比较慢）...
"%CMAKE%" -S . -B build -A x64
if errorlevel 1 (
    rem 配置失败时清掉缓存，修好环境后重新运行即可，不用手动删 build 文件夹
    if exist "build\CMakeCache.txt" del /q "build\CMakeCache.txt"
    if exist "build\CMakeFiles" rmdir /s /q "build\CMakeFiles"
    goto :cleanup_fail
)

echo.
echo [2/2] 编译 Release 版本（第一次约 3-10 分钟）...
"%CMAKE%" --build build --config Release --parallel
if errorlevel 1 goto :cleanup_fail

copy /y "build\GGitar_artefacts\Release\GGitar.exe" "GGitar.exe" >nul

popd
subst %DRV%: /d >nul

echo.
echo ==================================================
echo   编译成功！双击 GGitar.exe 运行。
echo ==================================================
pause
exit /b 0

:cleanup_fail
popd
subst %DRV%: /d >nul

:fail
echo.
echo 编译没有完成，请把上面的错误信息发给我。
pause
exit /b 1
