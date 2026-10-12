# Standalone production-wrapper test. Never packaged in the application.
LOCAL_PATH := $(call my-dir)
NTV_TEST_REPO := $(LOCAL_PATH)/../..
NTV_TEST_IJK := $(NTV_TEST_REPO)/.codex-tmp/ijkplayer-0.8.8
ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
NTV_TEST_FF_ARCH := arm64
else
NTV_TEST_FF_ARCH := armv7a
endif
include $(CLEAR_VARS)
LOCAL_MODULE := ijkffmpeg
LOCAL_SRC_FILES := ../../app/src/main/libs/$(TARGET_ARCH_ABI)/libijkffmpeg.so
include $(PREBUILT_SHARED_LIBRARY)
include $(CLEAR_VARS)
LOCAL_MODULE := deinterlace-test
LOCAL_SRC_FILES := test-deinterlace.c
LOCAL_C_INCLUDES := $(LOCAL_PATH) $(NTV_TEST_IJK)/ijkmedia \
    $(NTV_TEST_IJK)/android/contrib/build/ffmpeg-$(NTV_TEST_FF_ARCH)/output/include
LOCAL_CFLAGS := -std=c99 -O2
LOCAL_SHARED_LIBRARIES := ijkffmpeg
include $(BUILD_EXECUTABLE)
