cmake_minimum_required(VERSION 3.16)
project(ota_mini CXX)

# ---------------------------------------------------------------------------
# 与真实工程 humanoid_ota 保持一致的编译设置
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

set(OTA_MINI_SOURCES
    src/main.cpp
    src/config.cpp
    src/fs_utils.cpp
    src/ota_node.cpp
    src/shell_command.cpp
    src/state_machine.cpp
    src/states.cpp)

add_executable(ota_mini ${OTA_MINI_SOURCES})

target_include_directories(ota_mini PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 默认配置文件路径：既跑得动 build 目录，也跑得动源码目录
target_compile_definitions(
  ota_mini
  PRIVATE OTA_MINI_DEFAULT_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/configures/ota_mini.yaml")

# 线程库（GCC 上 std::thread 需要显式链接 pthread）
find_package(Threads REQUIRED)
target_link_libraries(ota_mini PRIVATE Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ota_mini PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# 便捷目标：直接以 demo 模式跑一遍完整流程
# ---------------------------------------------------------------------------
add_custom_target(run_demo
                  COMMAND ota_mini --demo
                  DEPENDS ota_mini
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                  USES_TERMINAL)
