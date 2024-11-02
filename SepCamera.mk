#
#SepCamera makefile
#create by vincent.yeh
#2015/09/15
#
ifeq ($(TARGET_CHIP_LIB_VER), ver-r0.0.1)

include $(CLEAR_VARS)

LOCAL_PATH := $(shell pwd)
#BUILD_TARGET_DIR := $(shell ls $(LOCAL_PATH))
BUILD_TARGET_DIR := src

all:build_sub_dir

build_sub_dir:
	@for i in $(BUILD_TARGET_DIR) ; \
	do \
        if [ -e $(LOCAL_PATH)/$$i/SepCamera.mk ] ;\
        then \
        $(MAKE) $$i LOCAL_PATH_SUBFIX=$$i; \
                if [ $$? -ne 0 ] ; \
                then \
                exit 1;\
                fi ; \
        fi ; \
        done 

endif
