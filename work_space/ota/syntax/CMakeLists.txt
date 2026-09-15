cmake_minimum_required(VERSION 3.16)
project(humanoid_ota_syntax_examples CXX)

# ---------------------------------------------------------------------------
# 与真实工程 humanoid_ota 的编译设置保持一致：
#   - C++20，不允许 GNU 扩展（CMAKE_CXX_EXTENSIONS OFF）
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# 每个 01_xxx.cpp ... 16_xxx.cpp 都编译成一个独立可执行文件
file(GLOB SYNTAX_EXAMPLE_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/[0-9][0-9]_*.cpp")

if(NOT SYNTAX_EXAMPLE_SOURCES)
  message(FATAL_ERROR "没有找到语法例程源文件 (syntax/[0-9][0-9]_*.cpp)")
endif()

set(SYNTAX_TARGETS "")
foreach(src ${SYNTAX_EXAMPLE_SOURCES})
  get_filename_component(ex_name ${src} NAME_WE)
  add_executable(${ex_name} ${src})
  # 例程之间共享 mini_log.hpp / xmacro_list.hpp，所以要带上本目录
  target_include_directories(${ex_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_options(${ex_name} PRIVATE -Wall -Wextra)
  set_target_properties(${ex_name} PROPERTIES
                        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  list(APPEND SYNTAX_TARGETS ${ex_name})
  message(STATUS "  语法例程: ${ex_name}")
endforeach()

list(LENGTH SYNTAX_TARGETS SYNTAX_TARGET_COUNT)
message(STATUS "语法例程数量: ${SYNTAX_TARGET_COUNT}")

# ---------------------------------------------------------------------------
# run_all_syntax：先编译全部，再依次运行（方便一次性看完所有输出）
# ---------------------------------------------------------------------------
add_custom_target(run_all_syntax
                  COMMAND ${CMAKE_COMMAND} -E echo "== 依次运行全部语法例程 =="
                  COMMAND sh -c "for ex in ${SYNTAX_TARGETS}; do echo; echo \"===== $$ex =====\"; ${CMAKE_BINARY_DIR}/$$ex; done"
                  DEPENDS ${SYNTAX_TARGETS}
                  USES_TERMINAL)
