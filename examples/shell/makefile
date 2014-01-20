FATCONFIG+=-DFAT_WRITE
FATCONFIG+=-DFAT_LFN
# FATCONFIG+=-DFAT_TIME

PROJECT=fatshell
PATH_COMMON=../../common/
PATH_SOURCES=../../sources/
CFLAGS=-c -O3 -I$(PATH_COMMON) -I$(PATH_SOURCES) -Wall -Wextra -Winline $(FATCONFIG)
CC=gcc
CXX=g++

ifneq ($(DEBUG), 0)
	CFLAGS+=-DDEBUG
endif

all: project

project: main.o entity.o fat32.o unicode.o rtc.o mutex.o mmi.o
	$(CXX) main.o entity.o fat32.o unicode.o rtc.o mutex.o mmi.o -lrt -lpthread -lcrypto -lboost_regex -o $(PROJECT)

entity.o: $(PATH_COMMON)entity.c $(PATH_COMMON)entity.h
	$(CC) $(CFLAGS) $(PATH_COMMON)entity.c

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
	$(CXX) $(CFLAGS) main.cpp

clean:
	rm -rf *.o *.lst $(PROJECT)
