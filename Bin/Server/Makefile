# Directories
SRVDIR = ../../Sources/UNIX_Server
LISTDIR = ../../Sources/UNIX_Server/UsersList
SHDIR = ../../Sources/Shared

# Compiler
CC = gcc
CFLAGS = -O2 -lpthread
CFDEBUG = -Wall -g -D_FORTIFY_SOURCE=1

# obj and exe files
OBJ = server_util.o guid.o string_helper.o users_list.o queue.o
EXE = server.out

# run and clean commands
RM = rm -f
RUN = ./server.out

server: server.o $(OBJ)
	$(CC) -o $(EXE) server.o $(OBJ) $(CFLAGS)

dbg: server.o $(OBJ)
	$(CC) -o $(EXE) server.o $(OBJ) $(CFDEBUG) $(CFLAGS)

server.o: $(SRVDIR)/server.c $(SRVDIR)/server.h $(OBJ)
	$(CC) -c $(SRVDIR)/server.c

server_util.o: $(SRVDIR)/server_util.c $(SRVDIR)/server.h $(SRVDIR)/server_util.h
	$(CC) -c $(SRVDIR)/server_util.c

guid.o: $(SHDIR)/guid.c $(SHDIR)/types.h $(SHDIR)/guid.h
	$(CC) -c $(SHDIR)/guid.c

string_helper.o: $(SHDIR)/string_helper.h $(SHDIR)/types.h
	$(CC) -c $(SHDIR)/string_helper.c

users_list.o: $(LISTDIR)/users_list.c $(LISTDIR)/users_list.h
	$(CC) -c $(LISTDIR)/users_list.c

queue.o: $(SHDIR)/Queue/Queue.c $(SHDIR)/Queue/Queue.h
	$(CC) -c $(SHDIR)/Queue/Queue.c

run:
	$(RUN)

clean:
	$(RM) server.o $(OBJ) $(EXE)
