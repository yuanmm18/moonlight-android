LOCAL_PATH := $(call my-dir)

# 先包含原有子目录
include $(call all-subdir-makefiles)

# 再包含我们的 Leia 模块
include $(LOCAL_PATH)/leia_jni.mk
