cmake_minimum_required(VERSION 3.20)
project(MemoryPool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 指向“内层”工程文件夹
set(PROJ_DIR ${CMAKE_CURRENT_SOURCE_DIR}/MemoryPool)
set(INC_DIR  ${PROJ_DIR}/include)
set(SRC_DIR  ${PROJ_DIR}/source)
set(TEST_DIR ${PROJ_DIR}/test)

# ---- 库源码（只放实现文件，不放测试文件） ----
set(MP_SOURCES
  ${SRC_DIR}/ThreadCache.cpp
  ${SRC_DIR}/CentralCache.cpp
  ${SRC_DIR}/PageCache.cpp
)

# 友好检查
foreach(f IN LISTS MP_SOURCES)
  if(NOT EXISTS "${f}")
    message(FATAL_ERROR "Source file not found: ${f}")
  endif()
endforeach()

# ---- 构建静态库 ----
add_library(mpool STATIC ${MP_SOURCES})
target_include_directories(mpool PUBLIC ${INC_DIR})

# ---- 可执行文件：单测（可选）----
if(EXISTS "${TEST_DIR}/unitTests.cpp")
  add_executable(mp_tests ${TEST_DIR}/unitTests.cpp)
  target_link_libraries(mp_tests PRIVATE mpool)
endif()

# ---- 可执行文件：性能测试（可选）----
# 注意文件名是 performanceTests.cpp（和右侧目录一致）
if(EXISTS "${TEST_DIR}/performanceTests.cpp")
  add_executable(mp_perf ${TEST_DIR}/performanceTests.cpp)
  target_link_libraries(mp_perf PRIVATE mpool)
endif()

# ---- 平台差异 ----
if (WIN32)
  target_compile_definitions(mpool PRIVATE _WIN32_WINNT=0x0601)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(mpool PRIVATE Threads::Threads)
endif()
