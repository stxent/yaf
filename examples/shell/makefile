PROJECT = shell
PROJECTDIR = ../..

PROJECTFLAGS += -DCONFIG_CRYPTO
PROJECTFLAGS += -DCONFIG_FILENAME_LENGTH=128
PROJECTFLAGS += -DCONFIG_SHELL_BUFFER=4096
PROJECTFLAGS += -DCONFIG_FAT_POOLS
PROJECTFLAGS += -DCONFIG_FAT_UNICODE
PROJECTFLAGS += -DCONFIG_FAT_THREADS
PROJECTFLAGS += -DCONFIG_FAT_TIME
PROJECTFLAGS += -DCONFIG_FAT_WRITE

ifneq ($(DEBUG), 0)
#	PROJECTFLAGS += -DCONFIG_MMI_DEBUG
	PROJECTFLAGS += -DCONFIG_MMI_STATUS
	PROJECTFLAGS += -DCONFIG_FAT_DEBUG
endif

CC = gcc
CFLAGS += -std=c11 -O0 -Wall -Wextra
CFLAGS += -Winline -Wcast-qual -pedantic -Wshadow
CFLAGS += $(PROJECTFLAGS)
CXX = g++
CXXFLAGS += -std=c++11 -O0 -Wall -Wextra
CXXFLAGS += -Winline -Wcast-qual -pedantic -Wshadow -Wold-style-cast
CXXFLAGS += $(PROJECTFLAGS)

SOURCEDIRS += $(addprefix $(PROJECTDIR)/, common libyaf os examples/$(PROJECT))
INCLUDEPATH += -I./
INCLUDEPATH += $(addprefix -I$(PROJECTDIR)/, include include/common)

DEPENDENCIES := $(shell find -name ".depend")
CSOURCES := $(shell find $(SOURCEDIRS) -name "*.c")
CXXSOURCES := $(shell find $(SOURCEDIRS) -name "*.cpp")
COBJECTS = $(CSOURCES:%.c=%.o)
CXXOBJECTS = $(CXXSOURCES:%.cpp=%.o)
LIBS = -lpthread -lrt -lcrypto

.PHONY: all clean depend
.SUFFIXES:

all: $(PROJECT)

$(PROJECT): $(COBJECTS) $(CXXOBJECTS)
	$(CXX) $(CXXFLAGS) $(LIBS) $(COBJECTS) $(CXXOBJECTS) -o $(PROJECT)

%.o: %.c
	$(CC) -c $(CFLAGS) $(INCLUDEPATH) $< -o $@

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDEPATH) $< -o $@

.depend:
	for src in $(COBJECTS) ; do \
		$(CC) -MM $(CFLAGS) $(INCLUDEPATH) -MT "$$src" "$$(echo $$src | sed s/o$$/c/)" ; \
	done > $@ ; \
	for src in $(CXXOBJECTS) ; do \
		$(CXX) -MM $(CXXFLAGS) $(INCLUDEPATH) -MT "$$src" "$$(echo $$src | sed s/o$$/cpp/)" ; \
	done >> $@

depend: .depend
	@touch .depend

clean:
	rm -f $(DEPENDENCIES)
	rm -f $(COBJECTS) $(CXXOBJECTS)
	rm -f $(PROJECT)

-include $(DEPENDENCIES)
