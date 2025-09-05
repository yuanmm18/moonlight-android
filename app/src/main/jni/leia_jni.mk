# Android.mk for Leia JNI library

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := leia_jni
LOCAL_SRC_FILES := leia_jni.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_CFLAGS := -fvisibility=hidden -DHAVE_NEON=1
LOCAL_CPPFLAGS := -std=c++11 -fno-exceptions -fno-rtti

LOCAL_LDLIBS := -llog -ldl

# Only build for arm64-v8a
ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    # Link against system libleia.so
    LOCAL_LDFLAGS := -L$(TARGET_OUT)/lib64 -llibleia
    include $(BUILD_SHARED_LIBRARY)
else
    # Build a stub for other architectures
    LOCAL_SRC_FILES += leia_stub.cpp
    include $(BUILD_SHARED_LIBRARY)
endif
