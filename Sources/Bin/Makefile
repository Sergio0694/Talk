# Directories
SRVDIR = ../UNIX_Server
LISTDIR = ../Tools/Server
SHDIR = ../Tools/Shared

# Compiler
CC = gcc
CFLAGS = -O2 -lpthread
CFDEBUG = -Wall -g

# obj and exe files
OBJ = server_util.o guid.o string_helper.o users_list.o
EXE = server

# Command for clean
RM = rm -f

all: server.o $(OBJ)
	$(CC) -o $(EXE) server.o $(OBJ) $(CFLAGS)

server.o: $(SRVDIR)/server.c $(SRVDIR)/server.h $(OBJ)
	$(CC) -c $(CFLAGS) $(SRVDIR)/server.c

server_util.o: $(SRVDIR)/server_util.c $(SRVDIR)/server.h $(SRVDIR)/server_util.h
	$(CC) -c $(CFLAGS) $(SRVDIR)/server_util.c

guid.o: $(SHDIR)/guid.c $(SHDIR)/types.h $(SHDIR)/guid.h
	$(CC) -c $(CFLAGS) $(SHDIR)/guid.c

string_helper.o: $(SHDIR)/string_helper.h $(SHDIR)/types.h
	$(CC) -c $(CFLAGS) $(SHDIR)/string_helper.c

users_list.o: $(LISTDIR)/users_list.c $(LISTDIR)/users_list.h
	$(CC) -c $(CFLAGS) $(LISTDIR)/users_list.c

run:
	./server

clean:
	$(RM) server.o $(OBJ) $(EXE)