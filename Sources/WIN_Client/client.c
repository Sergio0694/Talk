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
#include "client_graphics.h"

#define SERVER_IP "192.168.56.101"
#define PORT_NUMBER 25000
#define BUFFER_LENGTH 1024

/* MENU MACROS */
#define REFRESH_MESSAGE 'R'
#define REPRINT_MENU 'M'
#define WAIT_FOR_CONNECTIONS 'W'
#define CONNECT_WITH_ANOTHER_USER 'C'

typedef struct thread_params_s
{
    guid_t* output_guid;
    string_t* username;
    HANDLE output_sem;
    HANDLE race_condition_sem;
} thread_params_t;

typedef struct chat_thread_args_s
{
    string_t username;
} chat_thread_args_t;

/* ======== GLOBALS ======== */
guid_t client_guid;
client_list_t client_users_list = NULL;
SOCKET socketd = INVALID_SOCKET;
int chat_window_port;
/* ========================= */

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
    char* gets_ret;

    // Allocate the buffers to use
    char buffer[BUFFER_LENGTH], response[BUFFER_LENGTH];

    // Loop until we get a valid username
    while (TRUE)
    {
        // Choose a name and ask the server if it's valid
        gets_ret = fgets(buffer, BUFFER_LENGTH, stdin);
        ERROR_HELPER(gets_ret == NULL && ferror(stdin), "fgets fails");

        printf("The name is %s\nTrying to send it to the server\n", buffer);

        // Sends the name to the server and waits for a response
        send_to_socket(socketd, buffer);
        printf("Sent succesfully\nWaiting for a server response\n");
        recv_from_socket(socketd, response, BUFFER_LENGTH);
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
    change_console_color(DARK_TEXT);
    printf("[");
    change_console_color(DARK_GREEN);
    printf("%d", index);
    change_console_color(DARK_TEXT);
    printf("]");
    change_console_color(WHITE_TEXT);
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
    print_list(client_users_list, print_single_user);
}

guid_t* pick_target_user()
{
    printf("Pick a user to connect to: ");

    // Get the target user index
    while (TRUE)
    {
        // Read an integer from stdin require conversion
        const int maxIntCharLen = 2;
        char buf[maxIntCharLen];
        char* res = fgets(buf, maxIntCharLen, stdin);
        ERROR_HELPER(res == NULL && ferror(stdin), "Error reading from the input buffer");

        // Check if what I read from stdin was an actual digit
        if (!isdigit(buf[0]))
        {
            printf("Only digits are accepted\nRetry: ");
            continue;
        }
        int target = atoi(buf);
        printf("%d\n", target);

        // The target value has been converted, check the result
        printf("DEBUG trying get guid\n");
        guid_t* guid = try_get_guid(client_users_list, target);
        if (guid == NULL) printf("The input index isn't valid\n");
        else return guid;
    }

}

void send_target_guid(const guid_t guid)
{
    string_t serialized = serialize_guid(guid);
    serialized = strncat(serialized, "\n", strlen(serialized) + 1);
    send_to_socket(socketd, serialized);
}

void print_message(char* message, int len)
{
    int color = message[0] == '0' ? GREEN_TEXT : RED_TEXT;
    int i;
    change_console_color(DARK_TEXT);
    for (i = 1; i < len; i++)
    {
        if (message[i] == '[')
        {
            printf("[");
            change_console_color(color);
        }
        else if (message[i] == ']')
        {
            change_console_color(DARK_TEXT);
            printf("]");
            change_console_color(WHITE_TEXT);
        }
        else printf("%c", message[i]);
    }
    printf("\n");
}

// First chat handler that receives messages from the server
DWORD WINAPI chat_handler_in(LPVOID arg)
{
    // Unpack the arguments
    chat_thread_args_t* parameters = (chat_thread_args_t*)arg;
    string_t username = parameters->username;

    while (TRUE)
    {
        // Receive the message from the server
        char buffer[BUFFER_LENGTH];
        SecureZeroMemory(buffer, BUFFER_LENGTH);
        recv_from_socket(socketd, buffer, BUFFER_LENGTH);

        // Calculate the username to display
        char tmp[2] = { buffer[0], '\0' };
        int userIndex = atoi(tmp);

        // Display the message in the target console
        char message[BUFFER_LENGTH];
        SecureZeroMemory(message, BUFFER_LENGTH);
        char* temp_name = userIndex == 0 ? "Me" : username;
        snprintf(message, BUFFER_LENGTH, "%d[%s]%s", userIndex, temp_name, buffer + 1);
        int len = strlen(message);
        print_message(message, len);
    }
    return TRUE;
}

// Second chat handler that reads the new user messages and sends them
DWORD WINAPI chat_handler_out(LPVOID arg)
{
    while (TRUE)
    {
        // Send the new message if necessary
        char message[BUFFER_LENGTH];
        char* gets_ret = fgets(message, BUFFER_LENGTH, stdin);
        ERROR_HELPER(gets_ret == NULL && ferror(stdin) &&
            GetLastError() != ERROR_OPERATION_ABORTED, "Error in fgets");
        //if (gets_ret == NULL) continue;
        send_to_socket(socketd, message);
    }
}

void chat(string_t username)
{
    // Prepare the threads structure
    chat_thread_args_t arg = { 0 };
    arg.username = username;

    // Setup the chat threads
    HANDLE threads[2];
    threads[0] = CreateThread(
        NULL,       // Default security attributes
        0,          // Default stack size
        (LPTHREAD_START_ROUTINE)chat_handler_in, // Handler function
        (LPVOID)&arg, // Handler arguments
        0,          // Default creation flags
        NULL); // Ignore the thread identifier
    ERROR_HELPER(threads[0] == NULL, "Error creating the first picker thread");
    threads[1] = CreateThread(
        NULL,       // Default security attributes
        0,          // Default stack size
        (LPTHREAD_START_ROUTINE)chat_handler_out, // Handler function
        (LPVOID)&arg, // Handler arguments
        0,          // Default creation flags
        NULL); // Ignore the thread identifier

    // TODO: close the two threads when necessary
    DWORD join = WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    ERROR_HELPER(join == WAIT_FAILED, "Error while joining the two threads");
}

BOOL CtrlHandler(DWORD fdwCtrlType)
{
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

    // Confirm and return the result
    printf("SHUT DOWN completed\n");
    exit(EXIT_SUCCESS);
}

int main()
{
    // aux variables
    int ret;
    char buffer[BUFFER_LENGTH];
    SecureZeroMemory(buffer, BUFFER_LENGTH);

    // Add the Ctrl + C signal handler
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

    // Socket API initialization and socket creation
    initialize_socket_API();
    create_socket();
    printf("Socket created\nTrying to establish a connection with the server\n");

    // Connection
    struct sockaddr_in address = get_socket_address();
    ret = connect(socketd, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
    ERROR_HELPER(ret == SOCKET_ERROR, "Something happened while connecting to the server");
    printf("Connected succesfully\n");

    // Get and print the welcome message
    ret = recv(socketd, buffer, BUFFER_LENGTH, 0);
    ERROR_HELPER(ret == SOCKET_ERROR, "Error in recv function");
    printf("%s", buffer);

    // Login
    choose_name();

    // Print the command menu
    print_menu(); // TODO: implement this

    // Main client loop
    while (TRUE)
    {
        // Read from the standard input the user choice
        char* fgets_ret = fgets(buffer, BUFFER_LENGTH, stdin);
        ERROR_HELPER(fgets_ret == NULL && ferror(stdin), "Error in fgets");
        int menu_length = strlen(buffer);
        BOOL correct = check_menu_choice(buffer, menu_length); // TODO: implement this
        if (!correct) continue;

        char menu_choice[menu_length];
        strncpy(menu_choice, buffer, menu_length);
        SecureZeroMemory(buffer, BUFFER_LENGTH);

        // If menu_choice is refresh user list receive and print the users list
        if (menu_choice[0] == REFRESH_MESSAGE)
        {
            load_users_list();
            continue;
        }

        // If menu_choice is print the menu again print the menu
        if (menu_choice[0] == REPRINT_MENU)
        {
            print_menu();
            continue;
        }

        // if menu_choice is wait use a recv to wait for the server notification and start the chat
        if (menu_choice[0] == WAIT_FOR_CONNECTIONS)
        {
            // send the wait message to the server to make me available to chat
            send_to_socket(socketd, buffer);

            // receive the server response - is the username of the guy who chose me to chat
            // with attached a 1 if everything it's ok
            recv_from_socket(socketd, buffer, BUFFER_LENGTH);
            if (buffer[0] == '0')
            {
                printf("Something goes wrong in the connection with the other user\n");
                printf("Please repeat the process or change action\n");
                continue;
            }
            chat(buffer + 1);
        }

        // if menu_choice is to choose read from input the choice and send it to server
        // wait for server response and start the chat if it's positive
        if (menu_choice[0] == CONNECT_WITH_ANOTHER_USER)
        {
            // choose the user from the list and retrieve his guid
            guid_t* partner = pick_target_user();

            // send it to the server to check if the user is available for chat
            send_target_guid(*partner);

            // receive the server response - is made by a 0 or a 1 followed by the chose user's name
            recv_from_socket(socketd, buffer, BUFFER_LENGTH);
            if (buffer[0] == '0')
            {
                printf("Something goes wrong in the connection with the other user\n");
                printf("Please repeat the process or change action\n");
                continue;
            }
            chat(buffer + 1);
        }
    }
}
