#include <stdlib.h>
#include <winsock2.h>
#include <Windows.h>
#include <Winbase.h>
#include <Ws2tcpip.h> /* InetPton() */
#include <string.h>   /* strncpy() */
#include <ctype.h>    /* isdigit() */

#include "client_util.h"
#include "client_graphics.h"
#include "..\Shared\types.h"

#define PORT_NUMBER 25001
#define BUFFER_LEN 1024
#define KILL_MESSAGE "QUIT";

SOCKET initialize_socket()
{
	// Declare the variables
	int ret;

	// Enable the socket API
	initialize_socket_API();

	// Try to create the socket
	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socketd == INVALID_SOCKET, "There was an error while creating the socket");

	// Create the address struct and set the default values
	struct sockaddr_in server_addr = { 0 };
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUMBER);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	// Binding (of Isaac)
	ret = bind(server_socket, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
	ERROR_HELPER(ret == SOCKET_ERROR, "Error in the bind function");

	// Listen
	ret = listen(server_socket, 1);
	ERROR_HELPER(ret == SOCKET_ERROR, "Error in the listen function");

	// Accept
	SOCKET chat_socket = accept(server_socket, NULL, NULL);
	ERROR_HELPER(chat_socket == INVALID_SOCKET);
	return chat_socket;
}

void print_message(char* message, int len)
{
	int color = message[0] == 0 ? GREEN_TEXT : RED_TEXT;
	int i;
	change_console_color(LIGHT_GREY);
	for (i = 1; i < len; i++)
	{
		if (message[i] == '[')
		{
			printf("[");
			change_console_color(color);
			username_pending = TRUE;
		}
		else if (message[i] == ']')
		{
			change_console_color(LIGHT_GREY);
			printf("]");
			change_console_color(WHITE_TEXT);
		}
		else printf(message[i]);
	}
	printf("\n");
}

int main()
{
	// Get the client socket
	SOCKET chat_socket = initialize_socket();

	// Chat loop
	char buf[BUFFER_LEN];
	while (true)
	{
		// Receive the message
		int ret = recv_from_socket(chat_socket, buf, BUFFER_LEN);
		if (ret > 0)
		{
			// Check the quit message
			if (strncmp(buf, KILL_MESSAGE, strlen(KILL_MESSAGE)))
			{
				int ret = close_socket(chat_socket);
				ERROR_HELPER(ret == SOCKET_ERROR, "Error closing the socket");
				return 0;
			}

			// Print the message
			print_message(buf, ret);
		}
	}
	return 0;
}