cmake_minimum_required(VERSION 3.22)

# 静态链接 MSVC 运行库：别人的电脑上不用装 VC++ 运行库也能打开
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(GGitar VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE ------------------------------------------------------------------
# 查找顺序：
#   1. -DGGITAR_JUCE_DIR=<路径>
#   2. third_party/JUCE（build.bat 会从 third_party/JUCE-9.0.4.zip 解压到这里）
#   3. 联网从 GitHub 下载
set(GGITAR_JUCE_DIR "" CACHE PATH "Path to a JUCE checkout")
set(GGITAR_JUCE_TAG "9.0.4")

if(GGITAR_JUCE_DIR)
    add_subdirectory("${GGITAR_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${GGITAR_JUCE_TAG}
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- NAM（Neural Amp Modeler）核心库 -----------------------------------------
# build.bat 会把 third_party/NAM-core.zip 解压到 third_party/nam
set(NAM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nam")
if(NOT EXISTS "${NAM_ROOT}/NAM/dsp.h")
    message(FATAL_ERROR "找不到 NAM 源码：请先解压 third_party/NAM-core.zip 到 third_party/nam")
endif()

file(GLOB NAM_SOURCES "${NAM_ROOT}/NAM/*.cpp" "${NAM_ROOT}/NAM/*/*.cpp")
# OBJECT 库：NAM 靠静态初始化注册模型类型，静态库会被链接器丢掉这些目标文件
add_library(nam_core OBJECT ${NAM_SOURCES})
target_include_directories(nam_core SYSTEM PUBLIC
    "${NAM_ROOT}"
    "${NAM_ROOT}/eigen"
    "${NAM_ROOT}/nlohmann")
target_compile_definitions(nam_core PUBLIC NAM_SAMPLE_FLOAT NAM_ENABLE_A2_FAST)
target_compile_features(nam_core PUBLIC cxx_std_20)
set_target_properties(nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_definitions(nam_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(nam_core PRIVATE /bigobj /utf-8 "$<$<CONFIG:Release>:/O2>")
else()
    target_compile_options(nam_core PRIVATE -w "$<$<CONFIG:Release>:-O3>")
endif()

# ---- App -------------------------------------------------------------------
juce_add_gui_app(GGitar
    PRODUCT_NAME "GGitar"
    COMPANY_NAME "Gx-20hh"
    VERSION      ${PROJECT_VERSION})

target_sources(GGitar PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/AudioEngine.cpp
    Source/fx/BuiltinEffects.cpp
    Source/fx/MoreEffects.cpp
    Source/fx/GervanaEffects.cpp
    Source/fx/NamProcessor.cpp
    Source/fx/EffectChain.cpp
    Source/drums/DrumMachine.cpp
    Source/ui/PedalBoard.cpp
    Source/ui/PresetBar.cpp
    Source/ui/DrumPanel.cpp
    Source/ui/PedalEditor.cpp
    Source/ui/TabHighway.cpp
    Source/ui/PracticePanel.cpp
    Source/ui/TabSearch.cpp
    Source/ui/StatsView.cpp
    Source/song/SongBuilder.cpp
    Source/song/GpImporter.cpp
    Source/practice/TabPlayer.cpp
    Source/practice/Judge.cpp
    Source/practice/Recorder.cpp
    Source/practice/PracticeLog.cpp)

target_include_directories(GGitar PRIVATE Source)

target_compile_definitions(GGitar PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GGitar,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GGitar,JUCE_VERSION>"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    # Windows 音频驱动：ASIO（低延迟首选）、WASAPI、DirectSound
    JUCE_ASIO=1
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    # 加载 VST3 插件
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_LV2=0)

# 只用于在没有 ALSA 开发包的 Linux 机器上做编译检查
option(GGITAR_COMPILE_CHECK_ONLY "Disable ALSA/JACK for header-less compile checks" OFF)
if(GGITAR_COMPILE_CHECK_ONLY)
    target_compile_definitions(GGitar PRIVATE JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XINPUT=0)
endif()

if(MSVC)
    # 源码是 UTF-8，里面有中文字符串
    target_compile_options(GGitar PRIVATE /utf-8 /bigobj)
endif()

target_link_libraries(GGitar
    PRIVATE
        nam_core
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- 离线测试（不需要声卡）---------------------------------------------------
option(GGITAR_BUILD_TESTS "Build offline DSP tests" OFF)
if(GGITAR_BUILD_TESTS)
    juce_add_console_app(GGitarTests PRODUCT_NAME "GGitarTests")
    target_sources(GGitarTests PRIVATE
        tests/OfflineTests.cpp
        tests/PromoRender.cpp
        Source/ui/PedalBoard.cpp
        Source/ui/DrumPanel.cpp
        Source/MainComponent.cpp
        Source/ui/PresetBar.cpp
        Source/AudioEngine.cpp
        Source/fx/BuiltinEffects.cpp
    Source/fx/MoreEffects.cpp
    Source/fx/GervanaEffects.cpp
        Source/fx/NamProcessor.cpp
        Source/fx/EffectChain.cpp
        Source/drums/DrumMachine.cpp
        Source/ui/PedalEditor.cpp
        Source/ui/TabHighway.cpp
        Source/ui/PracticePanel.cpp
    Source/ui/TabSearch.cpp
    Source/ui/StatsView.cpp
        Source/song/SongBuilder.cpp
        Source/song/GpImporter.cpp
    Source/practice/TabPlayer.cpp
    Source/practice/Judge.cpp
    Source/practice/Recorder.cpp
    Source/practice/PracticeLog.cpp)
    target_include_directories(GGitarTests PRIVATE Source)
    target_compile_definitions(GGitarTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=0 JUCE_PLUGINHOST_LV2=0
        JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XINPUT=0)
    if(MSVC)
        target_compile_options(GGitarTests PRIVATE /utf-8 /bigobj)
    endif()
    target_link_libraries(GGitarTests PRIVATE nam_core juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
