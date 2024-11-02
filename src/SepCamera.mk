#
#SepCamera makefile.
#create by HuJian
#2015/09/15
#

include $(CLEAR_VARS)
LOCAL_PATH:= $(call my-dir)
include $(BUILD_SUB_DIR)

$(warning COMPILE_RECORDER_MP4=$(COMPILE_RECORDER_MP4))
ifeq ($(COMPILE_RECORDER_MP4),)

$(warning "##start compile pes recorder##")
LOCAL_SRC_FILES := sepcam_recorder.c
#LOCAL_SRC_FILES += recorder_task.c
LOCAL_SRC_FILES += av_packet_ps.c
LOCAL_SRC_FILES += recorder_server.c
LOCAL_SRC_FILES += recorder_es2pes.c
LOCAL_SRC_FILES += playback_task.c
LOCAL_SRC_FILES += disk_mgr.c
LOCAL_SRC_FILES += recorder_params.c
LOCAL_SRC_FILES += file_list.c
LOCAL_SRC_FILES += recorder_utils.c
LOCAL_SRC_FILES += $(TOPDIR)/extlib/ini_param_port/ini_param_port.c


#TARGET_CFLAGS +=  -lm -Wall -Wextra -Wno-unused-parameter  -g -O2 -DLINUX -ffunction-sections -Wl,--gc-sections
TARGET_CFLAGS +=  -Werror -Wextra -Wno-unused-parameter  -g -O2 -DLINUX -Wl,-gc-sections -pthread
TARGET_CPPFLAGS:= 


BASE_STATIC_LIBRARIES += libtaskbase
BASE_STATIC_LIBRARIES += libstreamshmgr
BASE_STATIC_LIBRARIES += libvipc
BASE_STATIC_LIBRARIES += libcjson
BASE_STATIC_LIBRARIES += libIniFile
BASE_STATIC_LIBRARIES += libbase_misc

# LOCAL_STATIC_LIBRARIES += libvipc
# LOCAL_STATIC_LIBRARIES += libjson
# LOCAL_STATIC_LIBRARIES += libIniFile

# LOCAL_C_INCLUDES += $(BASIC_DIR)/base/inc
# LOCAL_C_INCLUDES += $(BASIC_DIR)/base/inc/base
# LOCAL_C_INCLUDES += $(BASIC_DIR)/base/inc/streamshmmgr
# LOCAL_C_INCLUDES += $(BASIC_DIR)/hardware/common/inc
# LOCAL_C_INCLUDES += $(BASIC_DIR)/libvipc/include 
# LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include
LOCAL_C_INCLUDES += $(BASIC_DIR)/common/include
LOCAL_C_INCLUDES += $(BASIC_DIR)/include/taskevent
LOCAL_C_INCLUDES += $(BASIC_DIR)/include/streamshm
LOCAL_C_INCLUDES += $(BASIC_DIR)/libvipc/include
LOCAL_C_INCLUDES += $(BASIC_DIR)/libinifile
LOCAL_C_INCLUDES += $(BASIC_DIR)/cjson
LOCAL_C_INCLUDES += $(BASIC_DIR)/libmisc/include
LOCAL_C_INCLUDES += $(EXTLIB_DIR)/ini_param_port

TARGET_CFLAGS +=  -ldl

LOCAL_STRIP_MODULE := y
LOCAL_STRIP_FLAGS := --remove-section=.note --remove-section=.comment --remove-section=.debug

#LOCAL_MODULE := libsepcamera
#include $(BUILD_STATIC_LIBRARY)

LOCAL_MODULE := recorderapp
include $(BUILD_EXECUTABLE)

endif
