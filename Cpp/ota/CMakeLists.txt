cmake_minimum_required(VERSION 3.16)
project(humanoid_ota_learning CXX)

# =============================================================================
# humanoid_ota 工程学习笔记 —— 一键编译全部例程
#
#   cmake -S . -B build && cmake --build build -j
#
# 产物：
#   build/01_namespace_headers ... build/16_modern_cpp_misc   （语法例程）
#   build/ota_mini                                            （综合例程）
# =============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ① 按语法点拆分的独立小例程
add_subdirectory(syntax)

# ② 综合例程：一个完整可运行的迷你 OTA 状态机
add_subdirectory(examples/ota_mini)

message(STATUS "")
message(STATUS "构建产物:")
message(STATUS "  语法例程 : ${CMAKE_BINARY_DIR}/0X_xxx   (16 个)")
message(STATUS "  综合例程 : ${CMAKE_BINARY_DIR}/ota_mini")
message(STATUS "  运行 demo: ${CMAKE_BINARY_DIR}/ota_mini --demo")
message(STATUS "")
