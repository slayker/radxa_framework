LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    Client.cpp                              \
    DisplayDevice.cpp                       \
    EventThread.cpp                         \
    Layer.cpp                               \
    LayerBase.cpp                           \
    LayerDim.cpp                            \
    LayerScreenshot.cpp                     \
    DisplayHardware/FramebufferSurface.cpp  \
    DisplayHardware/GraphicBufferAlloc.cpp  \
    DisplayHardware/HWComposer.cpp          \
    DisplayHardware/PowerHAL.cpp            \
    GLExtensions.cpp                        \
    MessageQueue.cpp                        \
    SurfaceFlinger.cpp                      \
    SurfaceTextureLayer.cpp                 \
    Transform.cpp                           \
    

LOCAL_CFLAGS:= -DLOG_TAG=\"SurfaceFlinger\"
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES

ifeq ($(TARGET_BOARD_PLATFORM),omap3)
	LOCAL_CFLAGS += -DNO_RGBX_8888
endif
ifeq ($(TARGET_BOARD_PLATFORM),omap4)
	LOCAL_CFLAGS += -DHAS_CONTEXT_PRIORITY
endif
ifeq ($(TARGET_BOARD_PLATFORM),s5pc110)
	LOCAL_CFLAGS += -DHAS_CONTEXT_PRIORITY
	LOCAL_CFLAGS += -DNEVER_DEFAULT_TO_ASYNC_MODE
endif

ifeq ($(TARGET_DISABLE_TRIPLE_BUFFERING),true)
	LOCAL_CFLAGS += -DTARGET_DISABLE_TRIPLE_BUFFERING
endif

ifneq ($(NUM_FRAMEBUFFER_SURFACE_BUFFERS),)
  LOCAL_CFLAGS += -DNUM_FRAMEBUFFER_SURFACE_BUFFERS=$(NUM_FRAMEBUFFER_SURFACE_BUFFERS)
endif

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libdl \
	libhardware \
	libutils \
	libEGL \
	libGLESv1_CM \
	libbinder \
	libui \
	libgui

ifeq ($(strip $(TARGET_BOARD_HARDWARE)),rk30board)
  LOCAL_C_INCLUDES += hardware/rk29/libgralloc_ump/ump/include
  LOCAL_CFLAGS += -DTARGET_RK30
  ifeq ($(strip $(TARGET_BOARD_PLATFORM)),rk30xxb)
    LOCAL_CFLAGS += -DTARGET_BOARD_PLATFORM_RK30XXB
  endif
else
  ifeq ($(strip $(TARGET_BOARD_HARDWARE)),rk2928board)
    LOCAL_C_INCLUDES += hardware/rk29/libgralloc_ump/ump/include
    LOCAL_CFLAGS += -DTARGET_RK30
  else
    LOCAL_C_INCLUDES += hardware/libhardware/modules/gralloc
  endif
endif

LOCAL_MODULE:= libsurfaceflinger

include $(BUILD_SHARED_LIBRARY)

###############################################################
# uses jni which may not be available in PDK
ifneq ($(wildcard libnativehelper/include),)
include $(CLEAR_VARS)
LOCAL_CFLAGS:= -DLOG_TAG=\"SurfaceFlinger\"

LOCAL_SRC_FILES:= \
    DdmConnection.cpp

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libdl

LOCAL_MODULE:= libsurfaceflinger_ddmconnection

include $(BUILD_SHARED_LIBRARY)
endif # libnativehelper
