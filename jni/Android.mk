LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
OPENCV_INSTALL_MODULES:=on
include /Users/linqi/SDKDir/OpenCV-android-sdk/sdk/native/jni/OpenCV.mk


LOCAL_LDFLAGS := -Wl,--build-id -lskia -llog -lcutils
LOCAL_CFLAGS := -DSK_SUPPORT_LEGACY_SETCONFIG -mfloat-abi=softfp -mfpu=neon -march=armv7-a -mtune=cortex-a8

LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += \
	    $(LOCAL_PATH)/../../opencv-sdk/native/jni/include

LOCAL_SRC_FILES := \
	WDRInterface.cpp \
	MyThread.cpp \
	WDRBase.cpp

LOCAL_MODULE:= librkwdr
LOCAL_SHARED_LIBRARIES := libopencv_java3
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
OPENCV_INSTALL_MODULES:=on
include /Users/linqi/SDKDir/OpenCV-android-sdk/sdk/native/jni/OpenCV.mk

LOCAL_LDFLAGS := -Wl,--build-id -lskia -llog -lcutils
LOCAL_CFLAGS := -DSK_SUPPORT_LEGACY_SETCONFIG -mfloat-abi=softfp -mfpu=neon -march=armv7-a -mtune=cortex-a8

LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += \
	    $(LOCAL_PATH)/../../opencv-sdk/native/jni/include

LOCAL_SRC_FILES := \
	main.cpp \
	MyThread.cpp \
	WDRBase.cpp

LOCAL_MODULE:= test-wdr
LOCAL_SHARED_LIBRARIES := libopencv_java3
include $(BUILD_EXECUTABLE)
