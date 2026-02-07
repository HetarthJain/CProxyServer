CC=g++
CFlags= -g -Wall

all:proxy
# i want to use diff server files for testing, so i will not specify the server file in the makefile, but instead i will compile it separately and link it with the parse.o file to create the proxy executable.

proxy: server.c
	$(CC) $(CFLAGS) -o proxy.o -c server.c -lpthread
	$(CC) $(CFLAGS) -o parse.o -c parse.c -lpthread
	$(CC) $(CFLAGS) -o proxy parse.o proxy.o -lpthread
clean:
	rm -f proxy*.o
tar:
	tar -cvzf ass1.tgz server.c README Makefile parse.c parse.h
