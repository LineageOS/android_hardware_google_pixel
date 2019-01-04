LOCAL_PATH:= $(call my-dir)

# /vendor/bin/perfstatsd
include $(CLEAR_VARS)
LOCAL_SRC_FILES := main.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_MODULE := perfstatsd
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_OWNER := google
LOCAL_MODULE_TAGS := eng debug
LOCAL_STATIC_LIBRARIES := libperfstatsd
LOCAL_SHARED_LIBRARIES := libbase libbinder libcutils libhwbinder liblog libutils
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := perfstatsd.cpp perfstatsd_service.cpp perfstats_buffer.cpp cpu_usage.cpp io_usage.cpp
LOCAL_SRC_FILES += $(call all-Iaidl-files-under, binder)
LOCAL_SRC_FILES += $(call all-Iaidl-files-under, ../../../../frameworks/native/libs/binder/aidl)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(TOP)/system/core/include
LOCAL_MODULE := libperfstatsd
LOCAL_AIDL_INCLUDES := $(LOCAL_PATH)/binder
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_TAGS := eng debug
LOCAL_SHARED_LIBRARIES := libbinder
include $(BUILD_STATIC_LIBRARY)

# /vendor/etc/init/init-perfstatsd.rc
include $(CLEAR_VARS)
LOCAL_MODULE := init-perfstatsd.rc
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng debug
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_ETC)/init
include $(BUILD_PREBUILT)
