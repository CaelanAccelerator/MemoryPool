cmake_minimum_required(VERSION 3.20)
project(MemoryPool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJ_DIR ${CMAKE_CURRENT_SOURCE_DIR}/MemoryPool)
set(INC_DIR  ${PROJ_DIR}/include)
set(SRC_DIR  ${PROJ_DIR}/source)
set(TEST_DIR ${PROJ_DIR}/test)

set(MP_SOURCES
  ${SRC_DIR}/ThreadCache.cpp
  ${SRC_DIR}/CentralCache.cpp
  ${SRC_DIR}/PageCache.cpp
)

foreach(f IN LISTS MP_SOURCES)
  if(NOT EXISTS "${f}")
    message(FATAL_ERROR "Source file not found: ${f}")
  endif()
endforeach()

add_library(mpool STATIC ${MP_SOURCES} "MemoryPool/include/SpinLockGuard.h")
target_include_directories(mpool PUBLIC ${INC_DIR})

if(EXISTS "${TEST_DIR}/unitTests.cpp")
  add_executable(mp_tests ${TEST_DIR}/unitTests.cpp "MemoryPool/include/SpinLockGuard.h")
  target_link_libraries(mp_tests PRIVATE mpool)
endif()

if(EXISTS "${TEST_DIR}/performanceTests.cpp")
  add_executable(mp_perf ${TEST_DIR}/performanceTests.cpp "MemoryPool/include/SpinLockGuard.h")
  target_link_libraries(mp_perf PRIVATE mpool)

  if(NOT WIN32)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(TCMALLOC QUIET libtcmalloc)
      if(TCMALLOC_FOUND)
        target_compile_definitions(mp_perf PRIVATE HAVE_TCMALLOC)
        target_include_directories(mp_perf PRIVATE ${TCMALLOC_INCLUDE_DIRS})
        target_link_libraries(mp_perf PRIVATE ${TCMALLOC_LINK_LIBRARIES})
        message(STATUS "tcmalloc found — benchmark will include tcmalloc comparison")
      else()
        message(STATUS "tcmalloc not found — run: sudo apt install libgoogle-perftools-dev")
      endif()
    endif()
  endif()
endif()

if (WIN32)
  target_compile_definitions(mpool PRIVATE _WIN32_WINNT=0x0601)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(mpool PRIVATE Threads::Threads)
endif()

# --- isolated single-allocator benchmarks ---
if(NOT WIN32)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(TCMALLOC QUIET libtcmalloc)
  endif()

  add_executable(bench_mempool   ${TEST_DIR}/bench_mempool.cpp)
  target_link_libraries(bench_mempool   PRIVATE mpool Threads::Threads)

  add_executable(bench_newdelete ${TEST_DIR}/bench_newdelete.cpp)
  target_link_libraries(bench_newdelete PRIVATE Threads::Threads)

  add_executable(bench_tcmalloc  ${TEST_DIR}/bench_tcmalloc.cpp)
  target_link_libraries(bench_tcmalloc  PRIVATE Threads::Threads)
  if(TCMALLOC_FOUND)
    target_compile_definitions(bench_tcmalloc PRIVATE HAVE_TCMALLOC)
    target_include_directories(bench_tcmalloc PRIVATE ${TCMALLOC_INCLUDE_DIRS})
    target_link_libraries(bench_tcmalloc PRIVATE ${TCMALLOC_LINK_LIBRARIES})
  endif()
endif()
