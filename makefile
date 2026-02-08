CC=g++
CFLAGS=-g -Wall -Wextra
SERVER_SRC=server2.c
OBJDIR=obj

# List of object files (prefixed with the object directory)
OBJS = $(OBJDIR)/server.o $(OBJDIR)/parse.o $(OBJDIR)/resparse.o

all: $(OBJDIR) proxy

# Create the object directory if it doesn't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

# 1. Compile parser
$(OBJDIR)/parse.o: parse.c parse.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c parse.c -o $@

# 2. Compile response parser
$(OBJDIR)/resparse.o: response_parse.c response_parse.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c response_parse.c -o $@

# 3. Compile the specific server file
$(OBJDIR)/server.o: $(SERVER_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $(SERVER_SRC) -o $@

# 4. Link everything together into the current directory
proxy: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o proxy -lpthread

clean:
	rm -rf $(OBJDIR)
	rm -f proxy proxy2

tar:
	tar -cvzf ass1.tgz $(SERVER_SRC) README Makefile parse.c parse.h response_parse.c