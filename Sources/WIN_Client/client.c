#include <stdlib.h>
#include <winsock2.h>
#include <Windows.h>
#include <Ws2tcpip.h> /* InetPton */
#include <string.h> /* strncpy */

#include "client_util.h"
#include "ClientList\client_list.h"
#include "..\Shared\guid.h"
#include "..\Shared\string_helper.h"
#include "..\Shared\types.h"
#include "client_graphics.h"

#define SERVER_IP "192.168.1.103"
#define PORT_NUMBER 25000
#define BUFFER_LENGTH 1024

guid_t client_guid;
client_list_t client_users_list = NULL;
HANDLE messageConsole = NULL;
HANDLE consoleBuffer = NULL;

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
	char* gets_ret;

	// Allocate the buffers to use
	char buffer[BUFFER_LENGTH], response[BUFFER_LENGTH];

	// Loop until we get a valid username
	while (1)
	{
		// Choose a name and ask the server if it's valid
		gets_ret = fgets(buffer, BUFFER_LENGTH, stdin);
		ERROR_HELPER(gets_ret == NULL, "fgets fails");

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

int read_integer()
{
	char buf[10];
	const int maxIntCharLen = 10;
	char* res = fgets(buf, maxIntCharLen, stdin);
	ERROR_HELPER(res == NULL, "Error reading from the input buffer");
	int i = 0;
	while (buf[i] != '\0')
	{
		if (!(buf[i] >= 0 && buf[i] <= 9)) return -1;
	}
	int number = atoi(buf);
	return number >= 0 ? number : -1;
}

guid_t* pick_target_user(SOCKET socket, string_t* username)
{
	printf("Pick a user to connect to: ");
	bool_t done;
	do
	{
		// Wait for both the socket and the stdin
		HANDLE stdin = GetStdinHandle(STD_INPUT_HANDLE);
		HANDLE sources[] = { stdin, socket };
		int ret = WaitForMultipleObjects(2, sources, FALSE, INFINITE);
		ERROR_HELPER(ret == WAIT_FAILED || ret == WAIT_TIMEOUT,
			"Error in the WaitForMultipleObjects call");
		int index = ret - WAIT_OBJECT_0;

		// Check if another user has started a chat session with this client
		if (index == 1)
		{
			char buffer[BUFFER_LENGTH];
			int read = recv_from_server(socket, buffer, BUFFER_LENGTH);
			*username = (char*)malloc(sizeof(char) * read);
			strncpy(*username, buffer, read);
			return NULL;
		}
		else if (index == 0)
		{
			int target = read_integer();
			if (target == -1)
			{
				printf("The selected index isn't valid");
				continue;
			}
			guid_t* guid = try_get_guid(client_users_list, target);
			if (guid == NULL)
			{
				printf("The input index isn't valid\n");
			}
			else return guid;
		}
	} while (!done);
}

void send_target_guid(const guid_t guid)
{
	string_t serialized = serialize_guid(guid);
	send_to_server(socket, serialized);
}

void chat(SOCKET socket, string_t username)
{
	// Open the two console windows
	messageConsole = prepare_chat_window();
	consoleBuffer = get_console_screen_buffer_handle();
	write_console_message(
		"/* ========================\n*  CHAT WINDOW\n*  ====================== */\n\n",
		consoleBuffer, DARK_GREEN);

	while (TRUE)
	{
		// Wait for the stdin and the socket
		HANDLE stdin = GetStdinHandle(STD_INPUT_HANDLE);
		HANDLE sources[] = { stdin, socket };
		int ret = WaitForMultipleObjects(2, sources, FALSE, INFINITE);
		ERROR_HELPER(ret == WAIT_FAILED || ret == WAIT_TIMEOUT,
			"Error in the WaitForMultipleObjects call");
		int index = ret - WAIT_OBJECT_0;

		// Check if there is a new message to display
		if (index == 1)
		{
			// Receive the message from the server
			char buffer[BUFFER_LENGTH];
			int read = recv_from_server(socket, buffer, BUFFER_LENGTH);

			// Calculate the username to display
			char tmp[2] = { buffer[0], '\0' };
			int userIndex = atoi(tmp);

			// Display the message in the target console
			write_console_message("[", consoleBuffer, DARK_TEXT);
			string_t displayedName = userIndex == 0 ? "Me" : username;
			write_console_message(displayedName, consoleBuffer, RED_TEXT);
			write_console_message("] ", consoleBuffer, DARK_TEXT);
			write_console_message(buffer + 1, consoleBuffer, WHITE_TEXT);
			write_console_message("\n", consoleBuffer, WHITE_TEXT);

		}
		else if (index == 0)
		{
			// Send the new message if necessary
			char message[BUFFER_LENGTH];
			char* gets_ret = fgets(message, BUFFER_LENGTH, stdin);
			if (gets_ret == NULL) continue;
			send_to_server(socket, message);
		}
	}

	// Close the message console
	BOOL close_res = CloseHandle(consoleBuffer);
	ERROR_HELPER(!close_res, "Error closing the console buffer");
	close_res = CloseHandle(messageConsole);
	ERROR_HELPER(!close_res, "Error closing the chat console");
}

BOOL CtrlHandler(DWORD fdwCtrlType)
{
	// Skip other signals
	if (fwdCtrlType != CTRL_C_EVENT) return FALSE;

	// Cleanup and close everything
	if (socket != NULL)
	{
		int ret = closesocket(socket);
		ERROR_HELPER(ret != 0, "Error while closing the socket");
	}

	// Deallocate the user list
	destroy_user_list(client_users_list);

	// Close the open console and handles, if necessary
	if (consoleBuffer != NULL)
	{
		BOOL close_res = CloseHandle(consoleBuffer);
		ERROR_HELPER(!close_res, "Error closing the console buffer");
	}
	if (messageConsole != NULL)
	{
		close_res = CloseHandle(messageConsole);
		ERROR_HELPER(!close_res, "Error closing the chat console");
	}

	// Confirm and return the result
	printf("SHUT DOWN completed");
	return TRUE;
}

int main()
{
	// Add the Ctrl + C signal handler
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

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

	// Main client loop
	while (TRUE)
	{
		// Print the users list
		load_users_list(socket);

		// Get the guid of the target user to connect to
		string_t username = NULL;
		guid_t* target = pick_target_user(socket, &username);
		if (target != NULL)
		{
			send_to_server(socket, serialize_guid(target));
			char buf[1024];
			recv_from_server(socket, buf, 1);
			char tmp[2] = { buf[0], '\0' };
			int resultCode = atoi(tmp);
			if (resultCode == 1)
			{
				recv_from_server(socket, buf, 1024);
				chat(socket, buf + 1);
			}
			else continue;
		}
		else chat(socket, username);
	}
}

HANDLE prepare_chat_window()
{
	STARTUP_INFO startup_info;
	SecureZeroMemory((PVOID)&startup_info, sizeof(startup_info));
	startup_info.cb = sizeof(startup_info);
	PROCESS_INFORMATION p_info;
	SecureZeroMemory((PVOID)&p_info, sizeof(p_info));
	BOOL res = CreateProcess(
		NULL, /* Application name */
		NULL, /* Command line */
		NULL, /* Process attributes */
		NULL, /* Thread attributes */
		FALSE, /* Inherit handles */
		CREATE_NEW_CONSOLE,
		NULL, /* Environment */
		NULL, /* Current directory */
		(LPSTARTUP_INFO)&startup_info,
		(LPPROCESS_INFORMATION)&p_info);
	ERROR_HELPER(!res, "Error creating the new console window");
	return p_info.hProcess;
}

HANDLE get_console_screen_buffer_handle(HANDLE console_handle)
{
	HANDLE ret = CreateConsoleScreenBuffer(
		GENERIC_WRITE, /* Desired access */
		FILE_SHARE_WRITE, /*Share mode */
		NULL, /* Security attributes */
		CONSOLE_TEXTMODE_BUFFER, /* Flags */
		NULL /* Reserved */);
	ERROR_HELPER(ret == INVALID_HANDLE_VALUE, "Error creating the console buffer");
	return ret;
}

void write_console_message(string_t message, HANDLE hConsole_buffer, int color)
{
	// Set the desied color for the message to display
	SetConsoleTextAttribute(hConsole_buffer, color);

	// Try to write the message to the target console buffer
	int len = strlen(message);
	DWORD written = 0;
	BOOL res = WriteConsole(
		console_buffer, /* Console buffer handle */
		(void*)message, /* Message buffer */
		len, /* Number of characters to write */
		(LPDWORD)&written, /* Number of written characters */
		NULL /* Reserved */);

	// Check for errors
	ERROR_HELPER(res == 0, "Error writing into the target console buffer");
	ERROR_HELPER(written != len, "The message wasn't written completely");
}

void write_message_console_title(HANDLE console_buffer)
{

}
