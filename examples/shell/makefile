PROJECT=fatshell
CFLAGS=-c -Wall -Wextra -Winline
CC=gcc
CXX=g++

CFLAGS+=-DSECTOR_POW=9

ifneq ($(DEBUG), 0)
	CFLAGS+=-DDEBUG
endif

all: project

project: main.o bdev.o fs.o fat32.o io.o rtc.o mutex.o interface.o mmi.o
	$(CXX) main.o bdev.o fs.o fat32.o io.o rtc.o mutex.o interface.o mmi.o -lrt -lcrypto -lboost_regex -o $(PROJECT)

main.o: main.cpp
	$(CXX) $(CFLAGS) main.cpp

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

interface.o: interface.c interface.h
	$(CC) $(CFLAGS) interface.c

mmi.o: mmi.c mmi.h
	$(CC) $(CFLAGS) mmi.c

clean:
	rm -rf *.o *.lst $(PROJECT)
