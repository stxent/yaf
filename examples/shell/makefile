PROJECT=fatshell
PATH_COMMON=../../common/
# FATCONFIG=-DFAT_WRITE -DFAT_TIME
FATCONFIG=-DFAT_WRITE -DFAT_LFN
# FATCONFIG=-DFAT_WRITE
CFLAGS=-c -Os -I$(PATH_COMMON) -Wall -Wextra -Winline $(FATCONFIG)
CC=gcc
CXX=g++

ifneq ($(DEBUG), 0)
	CFLAGS+=-DDEBUG
endif

all: project

project: main.o entity.o fs.o fat32.o unicode.o rtc.o mutex.o interface.o mmi.o
	$(CXX) main.o entity.o interface.o fs.o fat32.o unicode.o rtc.o mutex.o mmi.o -lrt -lpthread -lcrypto -lboost_regex -o $(PROJECT)

entity.o: $(PATH_COMMON)entity.c $(PATH_COMMON)entity.h
	$(CC) $(CFLAGS) $(PATH_COMMON)entity.c

interface.o: $(PATH_COMMON)interface.c $(PATH_COMMON)interface.h
	$(CC) $(CFLAGS) $(PATH_COMMON)interface.c

fs.o: $(PATH_COMMON)fs.c $(PATH_COMMON)fs.h
	$(CC) $(CFLAGS) $(PATH_COMMON)fs.c

fat32.o: $(PATH_COMMON)fat32.c $(PATH_COMMON)fat32.h $(PATH_COMMON)fat32_defs.h
	$(CC) $(CFLAGS) $(PATH_COMMON)fat32.c

unicode.o: $(PATH_COMMON)unicode.c $(PATH_COMMON)unicode.h
	$(CC) $(CFLAGS) $(PATH_COMMON)unicode.c

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
