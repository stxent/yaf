PROJECT=fatshell
FATCONFIG=-DFAT_WRITE -DFAT_TIME
CFLAGS=-c -Wall -Wextra -Winline $(FATCONFIG)
CC=gcc
CXX=g++

ifneq ($(DEBUG), 0)
	CFLAGS+=-DDEBUG
endif

all: project

project: main.o entity.o bdev.o fs.o fat32.o io.o rtc.o mutex.o interface.o mmi.o
	$(CXX) main.o entity.o interface.o bdev.o fs.o fat32.o io.o rtc.o mutex.o mmi.o -lrt -lcrypto -lboost_regex -o $(PROJECT)

main.o: main.cpp
	$(CXX) $(CFLAGS) main.cpp

entity.o: entity.c entity.h
	$(CC) $(CFLAGS) entity.c

interface.o: interface.c interface.h
	$(CC) $(CFLAGS) interface.c

bdev.o: bdev.c bdev.h
	$(CC) $(CFLAGS) bdev.c

fs.o: fs.c fs.h
	$(CC) $(CFLAGS) fs.c

fat32.o: fat32.c fat32.h
	$(CC) $(CFLAGS) fat32.c

io.o: io.c io.h
	$(CC) $(CFLAGS) io.c

rtc.o: rtc.c rtc.h
	$(CC) $(CFLAGS) rtc.c

mutex.o: mutex.c mutex.h
	$(CC) $(CFLAGS) mutex.c

mmi.o: mmi.c mmi.h
	$(CC) $(CFLAGS) mmi.c

clean:
	rm -rf *.o *.lst $(PROJECT)
