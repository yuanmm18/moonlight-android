cmake_minimum_required(VERSION 3.18.1)
project(leia_jni)

set(CMAKE_CXX_STANDARD 11)

# 源文件
add_library(leia_jni SHARED
    leia_jni.cpp
    leia_stub.cpp
)

# 链接日志库
find_library(log-lib log)
target_link_libraries(leia_jni ${log-lib})

# arm64 才链接系统 libleia.so
if(${ANDROID_ABI} STREQUAL "arm64-v8a")
    target_link_libraries(leia_jni libleia)
endif()
