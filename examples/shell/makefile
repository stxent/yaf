PROJECT=fatshell
TOP=../../
PATH_HEADERS=$(TOP)include/
PATH_COMMON=$(TOP)common/
PATH_LIBRARY=$(TOP)libyaf/
PATH_OS=$(TOP)os/
INCLUDEPATH=-I$(PATH_HEADERS) -I$(PATH_HEADERS)common -I$(PATH_HEADERS)os -I./

FATCONFIG+=-DFAT_LFN
FATCONFIG+=-DFAT_POOLS
FATCONFIG+=-DFAT_TIME
FATCONFIG+=-DFAT_WRITE

CFLAGS=-c -std=c99 -O0 $(INCLUDEPATH) -Wall -Wextra -Winline $(FATCONFIG)
CXXFLAGS=-c -std=c++11 -O0 $(INCLUDEPATH) -Wall -Wextra -Winline $(FATCONFIG)
CC=gcc
CXX=g++

ifneq ($(DEBUG), 0)
	CFLAGS+=-DDEBUG
endif

all: project

project: main.o shell.o commands.o entity.o fat32.o unicode.o rtc.o mutex.o mmi.o queue.o
	$(CXX) main.o shell.o commands.o entity.o fat32.o unicode.o rtc.o mutex.o mmi.o queue.o -lrt -lpthread -lcrypto -o $(PROJECT)

entity.o: $(PATH_COMMON)entity.c $(PATH_HEADERS)common/entity.h
	$(CC) $(CFLAGS) $(PATH_COMMON)entity.c

queue.o: $(PATH_COMMON)queue.c $(PATH_HEADERS)common/queue.h
	$(CC) $(CFLAGS) $(PATH_COMMON)queue.c

fat32.o: $(PATH_LIBRARY)fat32.c $(PATH_HEADERS)libyaf/fat32.h $(PATH_HEADERS)libyaf/fat32_defs.h
	$(CC) $(CFLAGS) $(PATH_LIBRARY)fat32.c

unicode.o: $(PATH_LIBRARY)unicode.c $(PATH_HEADERS)libyaf/unicode.h
	$(CC) $(CFLAGS) $(PATH_LIBRARY)unicode.c

mutex.o: $(PATH_OS)mutex.c $(PATH_HEADERS)os/mutex.h
	$(CC) $(CFLAGS) $(PATH_OS)mutex.c

rtc.o: rtc.c rtc.h
	$(CC) $(CFLAGS) rtc.c

mmi.o: mmi.c mmi.h
	$(CC) $(CFLAGS) mmi.c

main.o: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp

shell.o: shell.cpp
	$(CXX) $(CXXFLAGS) shell.cpp

commands.o: commands.cpp
	$(CXX) $(CXXFLAGS) commands.cpp

clean:
	rm -rf *.o *.lst $(PROJECT)
