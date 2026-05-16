cmake_minimum_required(VERSION 3.14)
project(GalGameEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── nlohmann/json（本地单头文件，位于 third_party/nlohmann/json.hpp）──────
# 若文件不存在，可运行：
#   Invoke-WebRequest -Uri "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp" `
#     -OutFile "third_party/nlohmann/json.hpp" -UseBasicParsing

# ── SFML 3.0.0（图形界面库，通过 FetchContent 自动下载编译）──────────────
include(FetchContent)
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://gh-proxy.org/https://github.com/SFML/SFML.git
    GIT_TAG        3.0.0
    GIT_SHALLOW    TRUE
)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
# SFML 3.x 不再有 MSVC CRT 兼容问题，MinGW 可直接编译

# 加速配置：跳过各依赖库的测试和文档
set(FLAC_BUILD_TESTING    OFF CACHE BOOL "" FORCE)
set(FLAC_BUILD_DOCS       OFF CACHE BOOL "" FORCE)
set(FLAC_BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING         OFF CACHE BOOL "" FORCE)
set(OGG_BUILD_TESTING     OFF CACHE BOOL "" FORCE)
set(VORBIS_BUILD_TESTING  OFF CACHE BOOL "" FORCE)
# 若依赖已下载，跳过网络更新
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SFML)

# ── 可执行文件 ────────────────────────────────────────────────────────────
add_executable(galgame
    src/main.cpp
    src/engine.cpp
)

target_include_directories(galgame PRIVATE src third_party)
target_link_libraries(galgame PRIVATE sfml-graphics sfml-window sfml-system sfml-audio)
# SFML 3.x 在 Windows 上仍需 sfml-main 提供 WinMain 入口
if(WIN32)
    target_link_libraries(galgame PRIVATE sfml-main)
endif()

# Windows：GUI 子系统（无黑色控制台窗口）
if(WIN32)
    set_target_properties(galgame PROPERTIES WIN32_EXECUTABLE TRUE)
    target_compile_definitions(galgame PRIVATE UNICODE _UNICODE)
    # SFML 3.x 默认静态链接，无需复制 DLL
endif()

# ── 构建后将游戏数据复制到可执行文件目录 ─────────────────────────────────
# Story/ 目录包含所有故事子目录（如 G0/、AnyStory/ 等），整体复制。
add_custom_command(TARGET galgame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Story"
        "$<TARGET_FILE_DIR:galgame>/Story"
    COMMENT "Copying story data to output directory..."
)
