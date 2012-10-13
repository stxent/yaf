PROJECT=fatshell
CFLAGS=-c -Wall -Wextra -Winline
CC=gcc
CXX=g++

ifneq ($(DEBUG), 0)
	CFLAGS+=-DDEBUG
endif

all: project

project: main.o fat.o io.o rtc.o
	$(CXX) main.o fat.o io.o rtc.o -lrt -lcrypto -lboost_regex -o $(PROJECT)

main.o: main.cpp
	$(CXX) $(CFLAGS) main.cpp

fat.o: fat.c fat.h
	$(CC) $(CFLAGS) fat.c

io.o: io.c io.h
	$(CC) $(CFLAGS) io.c

rtc.o: rtc.c rtc.h
	$(CC) $(CFLAGS) rtc.c

clean:
	rm -rf *.o *.lst $(PROJECT)
