cmake_minimum_required(VERSION 3.20)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Convenience paths (relative to THIS file)
set(SRC_DIR   ${CMAKE_CURRENT_LIST_DIR})
set(ROOT_DIR  ${CMAKE_CURRENT_LIST_DIR}/..)
set(INC_DIR   ${ROOT_DIR}/include)

# ---- Files (explicit, matching your screenshot) ----
set(MP_SOURCES
    ${SRC_DIR}/CentralCache.cpp
    ${SRC_DIR}/PageCache.cpp
    ${SRC_DIR}/ThreadCache.cpp
    ${ROOT_DIR}/MemoryPool.cpp        # <— note: this is one level up
)

set(MP_PUBLIC_HEADERS
    ${INC_DIR}/CentralCache.h
    ${INC_DIR}/PageCache.h
    ${INC_DIR}/ThreadCache.h
)

set(MP_PRIVATE_HEADERS
    ${ROOT_DIR}/MemoryPool.h
)

# Optional: nice folder view in Visual Studio
source_group(TREE ${ROOT_DIR} FILES
    ${MP_SOURCES} ${MP_PUBLIC_HEADERS} ${MP_PRIVATE_HEADERS}
)

# ---- Library target ----
add_library(memorypool STATIC
    ${MP_SOURCES}
    ${MP_PUBLIC_HEADERS}
    ${MP_PRIVATE_HEADERS}
)

# Consumers include headers from MemoryPool/include
target_include_directories(memorypool
    PUBLIC
        $<BUILD_INTERFACE:${INC_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(memorypool PUBLIC cxx_std_17)
