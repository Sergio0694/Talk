# Directories
SRVDIR = ../UNIX_Server
LISTDIR = ../Tools/Server
SHDIR = ../Tools/Shared

# Compiler
CC = gcc
CFLAGS = -O2 -lpthread
CFDEBUG = -Wall -g

# obj and exe files
OBJ = server_util.o guid.o string_helper.o users_list.o chat_handler.o
EXE = server

# run and clean commands
RM = rm -f
RUN = ./server

server: server.o $(OBJ)
	$(CC) -o $(EXE) server.o $(OBJ) $(CFLAGS)

dbg: server.o $(OBJ)
	$(CC) -o $(EXE) server.o $(OBJ) $(CFDEBUG) -lpthread

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

chat_handler.o: $(SRVDIR)/chat_handler.c $(SRVDIR)/chat_handler.h
	$(CC) -c $(SRVDIR)/chat_handler.c 

run:
	$(RUN)

clean:
	$(RM) server.o $(OBJ) $(EXE)