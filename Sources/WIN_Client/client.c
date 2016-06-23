#include <stdlib.h>
#include <winsock2.h>
#include <Windows.h>
#include <Winbase.h>
#include <Ws2tcpip.h> /* InetPton() */
#include <string.h>   /* strncpy() */
#include <ctype.h>    /* isdigit() */
#include <time.h>

#include "client_util.h"
#include "ClientList\client_list.h"
#include "..\Shared\guid.h"
#include "..\Shared\string_helper.h"
#include "..\Shared\types.h"

#define SERVER_IP "192.168.56.101"
#define LOCAL_HOST "127.0.0.1"
#define PORT_NUMBER 25000
#define CHAT_WINDOW_PORT_NUMBER 25001
#define RANDOM_PORT_RANGE 20000
#define BUFFER_LENGTH 1024

guid_t client_guid;
client_list_t client_users_list = NULL;
HANDLE messageConsole = NULL;
HANDLE consoleBuffer = NULL;
SOCKET socketd = INVALID_SOCKET;
int chat_window_port;

HANDLE prepare_chat_window()
{
	STARTUPINFO startup_info;
	SecureZeroMemory((PVOID)&startup_info, sizeof(startup_info));
	startup_info.cb = sizeof(startup_info);
	startup_info.lpTitle = TEXT("Chat Window");
	PROCESS_INFORMATION p_info;
	SecureZeroMemory((PVOID)&p_info, sizeof(p_info));

	char cmd_args[32];
	srand((unsigned)time(NULL));
	chat_window_port = rand() % RANDOM_PORT_RANGE + CHAT_WINDOW_PORT_NUMBER;
	snprintf(cmd_args, 32, "%d", chat_window_port);

	BOOL res = CreateProcess(
		TEXT("..\\..\\Bin\\Client\\chat_window.exe"), /* Application name */
		cmd_args, /* Command line */
		NULL, /* Process attributes */
		NULL, /* Thread attributes */
		FALSE, /* Inherit handles */
		CREATE_NEW_CONSOLE,
		NULL, /* Environment */
		NULL, /* Current directory */
		(LPSTARTUPINFO)&startup_info,
		(LPPROCESS_INFORMATION)&p_info);
	ERROR_HELPER(!res, "Error creating the new console window");
	return p_info.hProcess;
}

// Creates a socket
static void create_socket()
{
	// Finally try to create the socket
	socketd = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socketd == INVALID_SOCKET, "There was an error while creating the socket");
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
static void choose_name()
{
	int ret;
	char* gets_ret;

	// Allocate the buffers to use
	char buffer[BUFFER_LENGTH], response[BUFFER_LENGTH];

	// Loop until we get a valid username
	while (TRUE)
	{
		// Choose a name and ask the server if it's valid
		gets_ret = fgets(buffer, BUFFER_LENGTH, stdin);
		ERROR_HELPER(gets_ret == NULL, "fgets fails");

		printf("Name chosen %s\nTrying to send it to the server\n", buffer);
		// Sends the name to the server and waits for a response
		send_to_socket(socketd, buffer);
		printf("Sent succesfully\nWaiting for a server response\n");
		int ret = recv_from_socket(socketd, response, BUFFER_LENGTH);
		char tmp[2] = { response[0], '\0' };
		int result = atoi(tmp);

		// Check the result
		if (result == 1) break;
		printf("%s\n", response + 1);
	}

	// Once the username has been chosen, save the client guid
	client_guid = deserialize_guid(response + 1);
	printf("Logged in, guid: %s\n", response + 1);
}

static void print_single_user(int index, string_t username)
{
//	change_console_color(DARK_TEXT);
	printf("[");
//	change_console_color(RED_TEXT);
	printf("%d", index);
//	change_console_color(DARK_TEXT);
	printf("]");
//	change_console_color(WHITE_TEXT);
	printf(" %s\n", username);
}

static void load_users_list()
{
	char buffer[BUFFER_LENGTH];
	printf("Waiting for the users list\n");
	recv_from_socket(socketd, buffer, BUFFER_LENGTH);
	printf("List received\n");
	printf("DEBUG: %s\n", buffer);
	client_users_list = deserialize_client_list(buffer, client_guid);

	// Print the users list
//	clear_screen();
	print_list(client_users_list, print_single_user);
}

int read_integer()
{
	char buf[10];
	const int maxIntCharLen = 10;
	char* res = fgets(buf, maxIntCharLen, stdin);
	ERROR_HELPER(res == NULL, "Error reading from the input buffer");
	//if (strncmp(buf, "R", 1) == 0) return REFRESH;
	int i = 0;
	while (buf[i] != '\n')
	{
		if (!isdigit((int)buf[i++])) return -1;
	}
	int number = atoi(buf);
	printf("%d\n", number);
	return number >= 0 ? number : -1;
}

guid_t* pick_target_user(string_t* username)
{
	printf("Pick a user to connect to: ");
	bool_t done;

	/*
	HANDLE input_handle = GetStdHandle(STD_INPUT_HANDLE);
    //WSAEventSelect(socketd, (WSAEVENT)input_handles[1], FD_READ);
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;*/

	// Prepare the data for the WaitForMultipleObjects call
	HANDLE input_handles[2];
	input_handles[0] = GetStdHandle(STD_INPUT_HANDLE);
    input_handles[1] = WSACreateEvent();
    WSAEventSelect(socketd, input_handles[1], FD_READ);

	do
	{
		/*bool_t index = FALSE;

		while (TRUE)
		{
			fd_set set;
			FD_ZERO(&set);
			FD_SET(socketd, &set);

			int ret = WaitForSingleObject(input_handle, 1000);
			if (ret != WAIT_TIMEOUT)
			{
				index = TRUE;
				ERROR_HELPER(ret == WAIT_FAILED, "WaitForSingleObject failed");
				break;
			}
			ret = select(1, &set, NULL, NULL, &tv);
			ERROR_HELPER(ret == SOCKET_ERROR, "Error on select call");
			if (ret != 0)
			{
				index = FALSE;
				break;
			}
		}*/

		// Wait for both the socket and the stdin
		int ret = WaitForMultipleObjects(2, input_handles, FALSE, INFINITE);
		ERROR_HELPER(ret == WAIT_FAILED || ret == WAIT_TIMEOUT,
			"Error in the WaitForMultipleObjects call");
		int index = ret - WAIT_OBJECT_0;

		// Check if another user has started a chat session with this client
		if (index == 1)
		{
			// receive the username of the client who have chosen you
			printf("DEBUG: data on socket - trying to read\n");
			char buffer[BUFFER_LENGTH];
			int read = recv_from_socket(socketd, buffer, BUFFER_LENGTH);
			*username = (char*)malloc(sizeof(char) * read);
			printf("DEBUG: %s\n", *username);
			strncpy(*username, buffer, read);
			CloseHandle(input_handles[1]);
			return NULL;
		}
		else if (index == 0)
		{
			int target = read_integer();
			if (target == -1)
			{
				printf("The selected index isn't valid\n");
				continue;
			}
			guid_t* guid = try_get_guid(client_users_list, target);
			if (guid == NULL)
			{
				printf("The input index isn't valid\n");
			}
			else
			{
				CloseHandle(input_handles[1]);
				return guid;
			}
		}
	} while (!done);
}

void send_target_guid(const guid_t guid)
{
	string_t serialized = serialize_guid(guid);
	serialized = strcat(serialized, "\n");
	send_to_socket(socketd, serialized);
}

void chat(string_t username)
{
	// Open the console window
	printf("DEBUG opening new console ...\n");
	HANDLE consoleWindow = prepare_chat_window();

	// Try to create the socket
	SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socketd == INVALID_SOCKET, "There was an error while creating the socket");

	// Create the address struct and set the default values
	struct sockaddr_in server_addr = { 0 };
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(chat_window_port);

	// Get the IP address in network byte order
	u_long inner_addr = inet_addr(LOCAL_HOST);
	server_addr.sin_addr.s_addr = inner_addr;

	// Try to connect
	int ret;
	while (TRUE)
	{
		ret = connect(client_socket, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
		if (ret == INVALID_SOCKET && WSAGetLastError() == WSAEHOSTUNREACH) continue;
		ERROR_HELPER(ret == INVALID_SOCKET, "Error in the connect function");
		break;
	}

	while (TRUE)
	{
		// Wait for the stdin and the socket
		HANDLE sources[2];
		sources[0] = GetStdHandle(STD_INPUT_HANDLE);
	    sources[1] = WSACreateEvent();
	    WSAEventSelect(socketd, sources[1], FD_READ);
		int ret = WaitForMultipleObjects(2, sources, FALSE, INFINITE);
		ERROR_HELPER(ret == WAIT_FAILED || ret == WAIT_TIMEOUT,
			"Error in the WaitForMultipleObjects call");
		int index = ret - WAIT_OBJECT_0;

		// Check if there is a new message to display
		if (index == 1)
		{
			// Receive the message from the server
			char buffer[BUFFER_LENGTH];
			int read = recv_from_socket(socketd, buffer, BUFFER_LENGTH);

			// Calculate the username to display
			char tmp[2] = { buffer[0], '\0' };
			int userIndex = atoi(tmp);

			// Display the message in the target console
			char message[BUFFER_LENGTH];
			char* temp_name = userIndex == 0 ? "Me" : username;
			snprintf(message, BUFFER_LENGTH, "[%s]%s", temp_name, buffer);
			send_to_socket(client_socket, message);
		}
		else if (index == 0)
		{
			// Send the new message if necessary
			char message[BUFFER_LENGTH];
			char* gets_ret = fgets(message, BUFFER_LENGTH, stdin);
			if (gets_ret == NULL) continue;
			send_to_socket(socketd, message);
		}
	}

	// Close the message console
	// TODO...
}

BOOL CtrlHandler(DWORD fdwCtrlType)
{
	BOOL close_res;

	// Skip other signals
	if (fdwCtrlType != CTRL_C_EVENT) return FALSE;

	// Cleanup and close everything
	if (socketd != INVALID_SOCKET)
	{
		int ret = closesocket(socketd);
		ERROR_HELPER(ret != 0, "Error while closing the socket");
	}

	// Deallocate the user list
	destroy_user_list(client_users_list);

	// Close the open console and handles, if necessary
	if (consoleBuffer != NULL)
	{
		close_res = CloseHandle(consoleBuffer);
		ERROR_HELPER(!close_res, "Error closing the console buffer");
	}
	if (messageConsole != NULL)
	{
		close_res = CloseHandle(messageConsole);
		ERROR_HELPER(!close_res, "Error closing the chat console");
	}

	// Confirm and return the result
	printf("SHUT DOWN completed\n");
	exit(EXIT_SUCCESS);
	//return TRUE;
}

int main()
{
	// Add the Ctrl + C signal handler
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

	// Socket API initialization
	initialize_socket_API();

	// Actual socket creation
	create_socket();
	printf("Socket created\nTrying to establish a connection with the server\n");

	// Connection
	struct sockaddr_in address = get_socket_address();
	int ret = connect(socketd, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
	ERROR_HELPER(ret == SOCKET_ERROR, "Something happened while connecting to the server");
	printf("Connected succesfully\n");

	// Get and print the welcome message
	char buffer[BUFFER_LENGTH];
	int bytes = recv(socketd, buffer, BUFFER_LENGTH, 0);
	printf("%s", buffer);

	// Login
	choose_name();

	// Main client loop
	while (TRUE)
	{
		// Print the users list
		load_users_list();

		// Get the guid of the target user to connect to
		string_t username = NULL;
		guid_t* target = pick_target_user(&username);
		if (target != NULL)
		{
			send_target_guid(*target);
			char buf[1024];
			printf("DEBUG: target guid sent -- trying to read from socket\n");
			recv_from_socket(socketd, buf, 1);
			char tmp[2] = { buf[0], '\0' };
			int resultCode = atoi(tmp);
			if (resultCode == 1)
			{
				recv_from_socket(socketd, buf, 1024);
				chat(buf);
			}
			else continue;
		}
		else chat(username + 1);
	}
}
