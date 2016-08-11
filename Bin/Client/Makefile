# compiler
CC = gcc
CFLAGS = -O2 -lws2_32
CFDEBUG = -Wall

# objs
OBJ = client_util.o client_list.o guid.o string_helper.o
EXE = client.exe

# directories
CLDIR = ..\..\Sources\WIN_Client
SHDIR = ..\..\Sources\Shared
LISTDIR = $(CLDIR)\ClientList

# headers
SHHEADER = $(SHDIR)\guid.c $(SHDIR)\types.h $(SHDIR)\guid.h

all:
	mingw32-make clean
	mingw32-make client
	mingw32-make chat_window

client: client.o $(OBJ)
	$(CC) -o $(EXE) $(OBJ) client.o $(CFLAGS)

chat_window: chat_window.o client_graphics.o client_util.o
	$(CC) -o chat_window.exe chat_window.o client_graphics.o client_util.o $(CFLAGS)

dbg: client.o $(OBJ)
	$(CC) -o $(EXE) $(OBJ) client.o $(CFLAGS) $(CFDEBUG)

client.o: $(OBJ) $(SHDIR)\types.h
	$(CC) -c $(CLDIR)\client.c

client_util.o: $(SHDIR)\types.h $(CLDIR)\client_util.h
	$(CC) -c $(CLDIR)\client_util.c

client_list.o: $(SHHEADER) $(LISTDIR)\client_list.h
	$(CC) -c $(LISTDIR)\client_list.c

guid.o: $(SHHEADER)
	$(CC) -c $(SHDIR)\guid.c

string_helper.o: $(SHDIR)\string_helper.h $(SHDIR)\types.h
	$(CC) -c $(SHDIR)\string_helper.c

client_graphics.o: $(CLDIR)\client_graphics.c $(CLDIR)\client_graphics.h
	$(CC) -c $(CLDIR)\client_graphics.c

chat_window.o: $(CLDIR)\chat_window.c client_graphics.o client_util.o
	$(CC) -c $(CLDIR)\chat_window.c

run:
	$(EXE)

clean:
	del $(OBJ) client.o $(EXE)
