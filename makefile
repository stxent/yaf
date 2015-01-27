#Copyright (C) 2015 xent
#Project is distributed under the terms of the GNU General Public License v3.0

PROJECT = yaf
PROJECTDIR = $(shell pwd)

CONFIG_FILE ?= .config
CROSS_COMPILE ?= arm-none-eabi-

#External libraries
XCORE_PATH ?= $(PROJECTDIR)/../xcore

-include $(CONFIG_FILE)

#Select build type
ifneq ($(findstring X86,$(CONFIG_TARGET)),)
  AR = ar
  CC = gcc
  CXX = g++
  LDLIBS += -lcrypto -lpthread -lrt
  ifeq ($(CONFIG_TARGET),"X86")
    PLATFORM := x86
    CPU_FLAGS := -m32
  else
    PLATFORM := x86-64
    CPU_FLAGS := -m64
  endif
else ifneq ($(findstring CORTEX-M,$(CONFIG_TARGET)),)
  AR = $(CROSS_COMPILE)ar
  CC = $(CROSS_COMPILE)gcc
  CXX = $(CROSS_COMPILE)g++
  PLATFORM := cortex-m$(CONFIG_TARGET:"CORTEX-M%"=%)
  CPU_FLAGS += -fmessage-length=0 -fno-builtin -ffunction-sections -fdata-sections -mthumb -mcpu=$(PLATFORM)
else
  ifneq ($(MAKECMDGOALS),menuconfig)
    $(error Target architecture is undefined)
  endif
endif

ifeq ($(CONFIG_OPT_LEVEL),"full")
  OPT_FLAGS := -O3 -flto -DNDEBUG
else ifeq ($(CONFIG_OPT_LEVEL),"size")
  OPT_FLAGS := -Os -flto -DNDEBUG
else ifeq ($(CONFIG_OPT_LEVEL),"none")
  OPT_FLAGS := -O0 -g3
else
  OPT_FLAGS := $(CONFIG_OPT_LEVEL)
endif

#Setup build flags
FLAG_NAMES += CONFIG_FAT_SECTOR CONFIG_FAT_POOLS CONFIG_FAT_THREADS
FLAG_NAMES += CONFIG_FAT_TIME CONFIG_FAT_UNICODE CONFIG_FAT_WRITE
FLAG_NAMES += CONFIG_FAT_DEBUG CONFIG_LIBSHELL
FLAG_NAMES += CONFIG_MMI_DEBUG CONFIG_MMI_STATUS CONFIG_SHELL CONFIG_SHELL_BUFFER
ifeq ($(CONFIG_OVERRIDE_LENGTH),y)
  FLAG_NAMES += CONFIG_FILENAME_LENGTH
endif

define append-flag
  ifeq ($$($(1)),y)
    CONFIG_FLAGS += -D$(1)
  else ifneq ($$($(1)),)
    CONFIG_FLAGS += -D$(1)=$$($(1))
  endif
endef

$(foreach entry,$(FLAG_NAMES),$(eval $(call append-flag,$(entry))))

#Configure common paths and libraries
INCLUDEPATH += -Iinclude -I"$(XCORE_PATH)/include"
OUTPUTDIR = build_$(PLATFORM)
LDFLAGS += -L$(OUTPUTDIR) -L"$(XCORE_PATH)/build_$(PLATFORM)"
LDLIBS += -l$(PROJECT) -lxcore

#Configure compiler options
CFLAGS += -std=c11 -Wall -Wcast-qual -Wextra -Winline -pedantic -Wshadow
CFLAGS += $(OPT_FLAGS) $(CPU_FLAGS) $(CONFIG_FLAGS)
CXXFLAGS += -std=c++11 -Wall -Wcast-qual -Wextra -Winline -pedantic -Wshadow -Wold-style-cast
CXXFLAGS += $(OPT_FLAGS) $(CPU_FLAGS) $(CONFIG_FLAGS)

#Search for project modules
LIBRARY_FILE = $(OUTPUTDIR)/lib$(PROJECT).a
LIBRARY_SOURCES := $(shell find libyaf -name *.c)
LIBRARY_OBJECTS = $(LIBRARY_SOURCES:%.c=$(OUTPUTDIR)/%.o)

TARGETS += $(LIBRARY_FILE)
CSOURCES += $(LIBRARY_SOURCES)
COBJECTS = $(CSOURCES:%.c=$(OUTPUTDIR)/%.o)
CXXOBJECTS = $(CXXSOURCES:%.cpp=$(OUTPUTDIR)/%.o)

include examples/makefile

.PHONY: all clean menuconfig
.SUFFIXES:
.DEFAULT_GOAL = all

all: $(TARGETS)

$(LIBRARY_FILE): $(LIBRARY_OBJECTS)
	$(AR) -r $@ $^

$(OUTPUTDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $(INCLUDEPATH) -MMD -MF $(@:%.o=%.d) -MT $@ $< -o $@

$(OUTPUTDIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) -c $(CXXFLAGS) $(INCLUDEPATH) -MMD -MF $(@:%.o=%.d) -MT $@ $< -o $@

clean:
	rm -f $(COBJECTS:%.o=%.d) $(COBJECTS) $(CXXOBJECTS:%.o=%.d) $(CXXOBJECTS)
	rm -f $(TARGETS)

menuconfig:
	kconfig-mconf kconfig

ifneq ($(MAKECMDGOALS),clean)
  -include $(COBJECTS:%.o=%.d) $(CXXOBJECTS:%.o=%.d)
endif
