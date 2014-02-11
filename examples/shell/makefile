FATCONFIG+=-DFAT_LFN
FATCONFIG+=-DFAT_POOLS
FATCONFIG+=-DFAT_TIME
FATCONFIG+=-DFAT_WRITE

PROJECT=fatshell
PATH_COMMON=../../common/
PATH_SOURCES=../../sources/
CFLAGS=-c -std=c99 -O0 -Wall -Wextra -Winline $(FATCONFIG)
CXXFLAGS=-c -std=c++11 -O0 -Wall -Wextra -Winline $(FATCONFIG)
CC=gcc
CXX=g++

CFLAGS+=-I./ -I$(PATH_COMMON) -I$(PATH_SOURCES)
CXXFLAGS+=-I./ -I$(PATH_COMMON) -I$(PATH_SOURCES)

ifneq ($(DEBUG), 0)
	CFLAGS+=-DDEBUG
endif

all: project

project: main.o shell.o commands.o entity.o fat32.o unicode.o rtc.o mutex.o mmi.o queue.o
	$(CXX) main.o shell.o commands.o entity.o fat32.o unicode.o rtc.o mutex.o mmi.o queue.o -lrt -lpthread -lcrypto -o $(PROJECT)

entity.o: $(PATH_COMMON)entity.c $(PATH_COMMON)entity.h
	$(CC) $(CFLAGS) $(PATH_COMMON)entity.c

queue.o: $(PATH_COMMON)queue.c $(PATH_COMMON)queue.h
	$(CC) $(CFLAGS) $(PATH_COMMON)queue.c

fat32.o: $(PATH_SOURCES)fat32.c $(PATH_SOURCES)fat32.h $(PATH_SOURCES)fat32_defs.h
	$(CC) $(CFLAGS) $(PATH_SOURCES)fat32.c

unicode.o: $(PATH_SOURCES)unicode.c $(PATH_SOURCES)unicode.h
	$(CC) $(CFLAGS) $(PATH_SOURCES)unicode.c

mutex.o: mutex.c mutex.h
	$(CC) $(CFLAGS) mutex.c

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
