LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := ffmpegutils
LOCAL_SRC_FILES := native.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/ffmpeg-1.2.4

LOCAL_LDLIBS := -llog -ljnigraphics -landroid -lz -ldl -lgcc

LOCAL_WHOLE_STATIC_LIBRARIES := ffmpeg

include $(BUILD_SHARED_LIBRARY)
include $(call all-makefiles-under,$(LOCAL_PATH))
