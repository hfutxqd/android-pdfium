LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := pdfium

ARCH_PATH = $(TARGET_ARCH_ABI)

ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    ARCH_PATH = armeabi
endif

LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/$(ARCH_PATH)/libpdfium.so

include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := pdfiumjni

LOCAL_CFLAGS += -DHAVE_PTHREADS
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES += pdfium
LOCAL_LDLIBS += -llog -landroid -ljnigraphics

LOCAL_SRC_FILES :=  $(LOCAL_PATH)/src/mainJNILib.cpp

include $(BUILD_SHARED_LIBRARY)
