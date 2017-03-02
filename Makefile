CC = g++
CFLAGS= -o
CCFLAGS = -lpthread

all:
	$(CC) syssim.cc $(CFLAGS) syssim $(CCFLAGS)
clean:
	rm syssim
