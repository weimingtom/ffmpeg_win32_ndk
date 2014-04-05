#LOCAL_PATH is one of libavutil, libavcodec, libavformat, or libswscale

#include $(LOCAL_PATH)/../config-$(TARGET_ARCH).mak
include $(LOCAL_PATH)/../config.mak

OBJS :=
OBJS-yes :=
MMX-OBJS-yes :=
include $(LOCAL_PATH)/Makefile

# collect objects
OBJS-$(HAVE_MMX) += $(MMX-OBJS-yes)
OBJS += $(OBJS-yes)

FFNAME := lib$(NAME)
FFLIBS := $(foreach,NAME,$(FFLIBS),lib$(NAME))
FFCFLAGS := -DHAVE_AV_CONFIG_H -Wno-sign-compare 
#if it is not include sysroot path first, libavutil/time.h will be included(see parseutils.c).
#FFCFLAGS += -I$(SYSROOT_INC)/usr/include
#FFCFLAGS += -DTARGET_CONFIG=/"config-$(TARGET_ARCH).h/"
FFCFLAGS += -D_ISOC99_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DPIC -std=c99 
FFCFLAGS += -Wall 
FFCFLAGS += -Wdeclaration-after-statement 
FFCFLAGS += -Wno-parentheses 
FFCFLAGS += -Wno-switch 
FFCFLAGS += -Wno-pointer-sign
FFCFLAGS += -Wno-format-zero-length -Wdisabled-optimization -Wpointer-arith 
#FFCFLAGS += -Wredundant-decls 
FFCFLAGS += -Wno-pointer-sign -Wwrite-strings -Wtype-limits 
#FFCFLAGS += -Wundef 
FFCFLAGS += -Wmissing-prototypes -Wno-pointer-to-int-cast -Wstrict-prototypes 
FFCFLAGS += -fno-math-errno -fno-signed-zeros -fno-tree-vectorize 
#FFCFLAGS += -Werror=implicit-function-declaration 
FFCFLAGS += -Werror=missing-prototypes 
FFCFLAGS += -Werror=return-type -Werror=vla

ALL_S_FILES := $(wildcard $(LOCAL_PATH)/$(TARGET_ARCH)/*.S)
ALL_S_FILES := $(addprefix $(TARGET_ARCH)/, $(notdir $(ALL_S_FILES)))

ifneq ($(ALL_S_FILES),)
ALL_S_OBJS := $(patsubst %.S,%.o,$(ALL_S_FILES))
C_OBJS := $(filter-out $(ALL_S_OBJS),$(OBJS))
S_OBJS := $(filter $(ALL_S_OBJS),$(OBJS))
else
C_OBJS := $(OBJS)
S_OBJS :=
endif

C_FILES := $(patsubst %.o,%.c,$(C_OBJS))
S_FILES := $(patsubst %.o,%.S,$(S_OBJS))

FFFILES := $(sort $(S_FILES)) $(sort $(C_FILES))
