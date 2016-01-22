#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <Ws2tcpip.h> /* InetPton */
#include "client_util.h"
#include "..\Tools\Client\client_list.h"
#include "..\Tools\Shared\guid.h"
#include "..\Tools\Shared\string_helper.h"
#include "..\Tools\Shared\types.h"

#define SERVER_IP "127.0.0.1"
#define PORT_NUMBER 25000
#define BUFFER_LENGTH 1024

guid_t client_guid;

// Initializes the WINSOCKET API
static void initialize_socket_API()
{
	// Make the API call
	WSADATA data = { 0 };
	int startup_ret = WSAStartup(MAKEWORD(2, 2), (LPWSADATA)&data);

	// Everything went fine, just return
	if (startup_ret == 0) return;

	// Display an error message and exit the process
	ERROR_HELPER(startup_ret == WSAVERNOTSUPPORTED, "The 2.2 SOCKET APIs are not supported on your system");
	ERROR_HELPER(startup_ret != 0, "There was an error during the SOCKET API initialization");
}

// Creates a socket
static SOCKET create_socket()
{
	// Finally try to create the socket
	SOCKET socket = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socket == INVALID_SOCKET, "There was an error while creating the socket");
	return socket;
}

// Creates a sockaddr_in struct with the server address
static struct sockaddr_in get_socket_address()
{
	// Create the address struct and set the default values
	struct sockaddr_in server_addr = { 0 };
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUMBER);

	// Get the IP address in network byte order
	struct addr_in inner_addr = { 0 };
	bool_t ret = InetPton(AF_INET, (PCTSTR)TEXT(SERVER_IP), (LPVOID)&inner_addr);
	ERROR_HELPER(!ret, "There was an error while converting the server IP address");
	server_addr.sin_addr.s_addr = inner_addr;
	return server_addr;
}

// Logins into the server with a new username
static void choose_name(SOCKET socket)
{
	// Allocate the buffers to use
	char buffer[BUFFER_LENGTH], response[BUFFER_LENGTH];

	// Loop until we get a valid username
	while (1)
	{
		// Choose a name and ask the server if it's valid
		scanf_s("%s", buffer, BUFFER_LENGTH);

		// Sends the name to the server and waits for a response
		send_to_server(socket, buffer);
		int ret = recv_from_server(socket, response, BUFFER_LENGTH);
		int result = atoi(response[0]);

		// Check the result
		if (result == 1) break;
		printf_s("%s\n", response + 1);
	}

	// Once the username has been chosen, save the client guid
	client_guid = deserialize_guid(response + 1);
}

static void print_single_user(string_t username)
{
	printf_s("%s\n", username);
}

static void load_users_list(SOCKET socket)
{
	char buffer[BUFFER_LENGTH];
	recv_from_server(socket, buffer, BUFFER_LENGTH);
	client_list_t users_list = deserialize_client_list(buffer);
	print_list(users_list, print_single_user);
}

int main()
{
	// Socket API initialization
	initialize_socket_API();

	// Actual socket creation
	SOCKET socket = create_socket();

	// Connection
	struct sockaddr_in address = get_socket_address();
	int ret = connect(socket, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
	ERROR_HELPER(ret == SOCKET_ERROR, "Something happened while connecting to the server");

	// Get and print the welcome message
	char buffer[BUFFER_LENGTH];
	int bytes = recv(socket, &buffer, BUFFER_LENGTH);
	printf("%s", buffer);

	// Login
	choose_name(socket);

	// Print the users list
	load_users_list(socket);
}