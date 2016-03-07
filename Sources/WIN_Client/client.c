#include <stdlib.h>
#include <winsock2.h>
#include <Windows.h>
#include <Ws2tcpip.h> /* InetPton */

#include "client_util.h"
#include "..\Tools\Client\client_list.h"
#include "..\Tools\Shared\guid.h"
#include "..\Tools\Shared\string_helper.h"
#include "..\Tools\Shared\types.h"
#include "client_graphics.h"

#define SERVER_IP "192.168.1.103"
#define PORT_NUMBER 25000
#define BUFFER_LENGTH 1024

guid_t client_guid;
client_list_t client_users_list;

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
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(sock == INVALID_SOCKET, "There was an error while creating the socket");
	return sock;
}

// Creates a sockaddr_in struct with the server address
static struct sockaddr_in get_socket_address()
{
	// Create the address struct and set the default values
	struct sockaddr_in server_addr = { 0 };
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUMBER);

	// Get the IP address in network byte order
	u_long inner_addr = inet_addr(SERVER_IP);
	server_addr.sin_addr.s_addr = inner_addr;
	return server_addr;
}

// Logins into the server with a new username
static void choose_name(SOCKET socket)
{
	int ret;

	// Allocate the buffers to use
	char buffer[BUFFER_LENGTH], response[BUFFER_LENGTH];

	// Loop until we get a valid username
	while (1)
	{
		// Choose a name and ask the server if it's valid
		ret = scanf("%1024s", buffer);
		ERROR_HELPER(ret == -1, "Scanf failed");

		printf("Name chosen %s\nTrying to send it to the server\n", buffer);
		// Sends the name to the server and waits for a response
		send_to_server(socket, buffer);
		printf("Sent succesfully\nWaiting for a server response\n");
		int ret = recv_from_server(socket, response, BUFFER_LENGTH);
		char tmp[2] = { response[0], '\0' };
		int result = atoi(tmp);

		// Check the result
		if (result == 1) break;
		printf("%s\n", response + 1);
	}

	// Once the username has been chosen, save the client guid
	client_guid = deserialize_guid(response + 1);
	printf("Logged in, guid: %s", response + 1);
}

static void print_single_user(int index, string_t username)
{
	change_console_color(DARK_TEXT);
	printf("[");
	change_console_color(RED_TEXT);
	printf("%d", index);
	change_console_color(DARK_TEXT);
	printf("]");
	change_console_color(WHITE_TEXT);
	printf(" %s\n", username);
}

static void load_users_list(SOCKET socket)
{
	char buffer[BUFFER_LENGTH];
	printf("Waiting for the users list\n");
	recv_from_server(socket, buffer, BUFFER_LENGTH);
	printf("List received\n");
	client_users_list = deserialize_client_list(buffer, client_guid);

	// Print the users list
	clear_screen();
	print_list(client_users_list, print_single_user);
}

guid pick_target_user()
{
	printf("Pick a user to connect to: ");
	bool_t done;
	do
	{
		int target;
		scanf("%d", &target);
		guid_t* guid = try_get_guid(client_users_list, target);
		if (guid == NULL)
		{
			printf("The input index isn't valid\n");
		}
		else return *guid;
	} while (!done);
}

void send_target_guid(const guid_t guid)
{
	string_t serialized = serialize_guid(guid);
	send_to_server(socket, serialized);
}

void chat(SOCKET socket)
{
	while (TRUE)
	{
		// Initialize the input set for the select function
		fd_set input;
		FD_SET(stdin, &input);
		FD_SET(socket, &input);

		// Prepare the timeout
		struct timeval timeout;
		timeout.tv_sec = 120;
		timeout.tv_usec = 0;

		// Call the select
		int ret = select(0, &input, NULL, NULL, &timeout);

		// Timeout expired
		if (ret == 0) // TODO
		{

		}

		// Error handling
		if (ret == SOCKET_ERROR)
		{
			int error = WSAGetLastError();
			if (error == WSAEINTR) continue;
			// TODO: close the thread
		}

		// Check if there is a new message to display
		if (FD_ISSET(socket, &input) != 0)
		{
			char buffer[BUFFER_LENGTH];
			int read = recv_from_server(socket, buffer, BUFFER_LENGTH);
			// TODO: print message
		}

		// Send the new message if necessary
		if (FD_ISSET(stdin, &input) != 0)
		{
			char message[BUFFER_LENGTH];
			char* gets_ret = fgets(message, BUFFER_LENGTH, stdin);
			if (gets_ret == NULL) continue;
			send_to_server(socket, message);
		}
	}
}

int main()
{
	// Socket API initialization
	initialize_socket_API();

	// Actual socket creation
	SOCKET socket = create_socket();
	printf("Socket created\nTrying to establish a connection with the server\n");

	// Connection
	struct sockaddr_in address = get_socket_address();
	int ret = connect(socket, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
	ERROR_HELPER(ret == SOCKET_ERROR, "Something happened while connecting to the server");
	printf("Connected succesfully\n");

	// Get and print the welcome message
	char buffer[BUFFER_LENGTH];
	int bytes = recv(socket, buffer, BUFFER_LENGTH, 0);
	printf("%s", buffer);

	// Login
	choose_name(socket);

	// Print the users list
	load_users_list(socket);

	// Get the guid of the target user to connect to
	guid_t target = pick_target_user();
	send_to_server();
}