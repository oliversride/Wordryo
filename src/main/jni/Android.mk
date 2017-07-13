# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

COMMON_PATH=../jni_common
local_C_INCLUDES+= \
	-I$(LOCAL_PATH)/$(COMMON_PATH) \
	-I$(LOCAL_PATH)/../jni_relay \

local_LDLIBS += -llog

local_DEFINES += \
	$(local_DEBUG) \
	-DXWFEATURE_RELAY \
	-DXWFEATURE_SMS \
	-DXWFEATURE_COMMSACK \
	-DXWFEATURE_TURNCHANGENOTIFY \
	-DXWFEATURE_CHAT \
	-DCOMMS_XPORT_FLAGSPROC \
	-DKEY_SUPPORT \
	-DXWFEATURE_CROSSHAIRS \
	-DPOINTER_SUPPORT \
	-DSCROLL_DRAG_THRESHHOLD=1 \
	-DDROP_BITMAPS \
	-DXWFEATURE_TRAYUNDO_ONE \
	-DDISABLE_TILE_SEL \
	-DXWFEATURE_BOARDWORDS \
	-DXWFEATURE_WALKDICT \
	-DXWFEATURE_WALKDICT_FILTER \
	-DXWFEATURE_DICTSANITY \
	-DFEATURE_TRAY_EDIT \
	-DXWFEATURE_BONUSALL \
	-DMAX_ROWS=32 \
	-DHASH_STREAM \
	-DXWFEATURE_BASE64 \
	-DXWFEATURE_DEVID \
	-DINITIAL_CLIENT_VERS=3 \
	-DRELAY_ROOM_DEFAULT=\"\" \
	-D__LITTLE_ENDIAN \

local_SRC_FILES +=         \
	xwjni.c                \
	utilwrapper.c          \
	drawwrapper.c          \
	xportwrapper.c         \
	anddict.c              \
	andutils.c             \
	jniutlswrapper.c       \


COMMON_PATH=../jni_common
common_SRC_FILES +=        \
	$(COMMON_PATH)/boarddrw.c   \
	$(COMMON_PATH)/scorebdp.c   \
	$(COMMON_PATH)/dragdrpp.c   \
	$(COMMON_PATH)/pool.c       \
	$(COMMON_PATH)/tray.c       \
	$(COMMON_PATH)/dictnry.c    \
	$(COMMON_PATH)/dictiter.c   \
	$(COMMON_PATH)/mscore.c     \
	$(COMMON_PATH)/vtabmgr.c    \
	$(COMMON_PATH)/strutils.c   \
	$(COMMON_PATH)/engine.c     \
	$(COMMON_PATH)/board.c      \
	$(COMMON_PATH)/mempool.c    \
	$(COMMON_PATH)/game.c       \
	$(COMMON_PATH)/server.c     \
	$(COMMON_PATH)/model.c      \
	$(COMMON_PATH)/comms.c      \
	$(COMMON_PATH)/memstream.c  \
	$(COMMON_PATH)/movestak.c   \
	$(COMMON_PATH)/dbgutil.c    \

LOCAL_MODULE    := xwjni
LOCAL_SRC_FILES := $(local_SRC_FILES) $(common_SRC_FILES)
LOCAL_CFLAGS+=$(local_C_INCLUDES) $(local_DEFINES) -Wall
LOCAL_LDLIBS := -L${SYSROOT}/usr/lib -llog -lz

include $(BUILD_SHARED_LIBRARY)