PROJECT = shell

CONFIGFAT += -DCONFIG_FAT_POOLS
CONFIGFAT += -DCONFIG_FAT_UNICODE
CONFIGFAT += -DCONFIG_FAT_THREADS
CONFIGFAT += -DCONFIG_FAT_TIME
CONFIGFAT += -DCONFIG_FAT_WRITE

CFLAGS = -std=c11 -O0 -Wall -Wextra -Winline -pedantic
CC = gcc
CXXFLAGS = -std=c++11 -O0 -Wall -Wextra -Winline -pedantic
CXX = g++

CFLAGS += $(CONFIGFAT)
CXXFLAGS += $(CONFIGFAT)

ifneq ($(DEBUG), 0)
	CFLAGS += -DDEBUG
	CXXFLAGS += -DDEBUG
endif

PROJECTDIR = ../..
SOURCEDIRS = $(addprefix $(PROJECTDIR)/, common libyaf os examples/$(PROJECT))
INCLUDEPATH = -I./ -I$(PROJECTDIR)/include -I$(PROJECTDIR)/include/common -I$(PROJECTDIR)/include/os

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
