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
#define REFRESH -2

typedef struct thread_params_s
{
    guid_t* output_guid;
    string_t* username;
    HANDLE output_sem;
    HANDLE race_condition_sem;
} thread_params_t;

/* ======== GLOBALS ======== */
guid_t client_guid;
client_list_t client_users_list = NULL;
HANDLE consoleWindow = NULL;
SOCKET socketd = INVALID_SOCKET;
int chat_window_port;
/* ========================= */

/*
// Thread function to handle the pressing of the return key
DWORD return_handler(LPVOID params)
{
    int ret;

    thread_params_t* args = (thread_params_t*)params;
    HANDLE stdin_handle = args->stdin_handle;
    HANDLE event = args->event_handle;

    while (TRUE)
    {
        ret = WaitForSingleObject(stdin_handle, INFINITE);
        ERROR_HELPER(ret == WAIT_FAILED, "Error in WaitForSingleObject");
        if (GetAsyncKeyState(VK_RETURN))
        {
            printf("DEBUG \\n key pressed - signaling ... \n");
            ret = SetEvent(event);
            ERROR_HELPER(ret == 0, "Error while signaling the event");
            ret = ResetEvent(event);
            ERROR_HELPER(ret == 0, "Error while resetting the event");
        }
    }
}
*/

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
//    change_console_color(DARK_TEXT);
    printf("[");
//    change_console_color(RED_TEXT);
    printf("%d", index);
//    change_console_color(DARK_TEXT);
    printf("]");
//    change_console_color(WHITE_TEXT);
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

int read_integer()
{
    char buf[10];
    const int maxIntCharLen = 10;
    char* res = fgets(buf, maxIntCharLen, stdin);
    ERROR_HELPER(res == NULL, "Error reading from the input buffer");
    if (strncmp(buf, "R", 1) == 0)
    {
        printf("DEBUG refresh requested\n");
        return REFRESH;
    }
    int i = 0;
    while (buf[i] != '\n')
    {
        if (!isdigit((int)buf[i++])) return -1;
    }
    int number = atoi(buf);
    printf("%d\n", number);
    return number >= 0 ? number : -1;
}

// First picker function that checks for a request by another user
DWORD WINAPI picker_handler_in(LPVOID arg)
{
    // Unpack the arguments
    thread_params_t* parameters = (thread_params_t*)arg;
    HANDLE semaphore = parameters->output_sem;
    HANDLE race_semaphore = parameters->race_condition_sem;
    string_t* username = (string_t*)parameters->username;

    // Receive the username of the client who picked you
    printf("DEBUG: data on socket - trying to read\n");
    char buffer[BUFFER_LENGTH];
    int read = recv_from_socket(socketd, buffer, BUFFER_LENGTH);

    // Save the return value
    DWORD ret = WaitForSingleObject(race_semaphore, INFINITE);
    ERROR_HELPER(ret == WAIT_FAILED, "Error waiting for the semaphore");
    parameters->output_guid = NULL;

    // Copy the username actual value
    *username = (char*)malloc(sizeof(char) * read);
    strncpy(*username, buffer, read);
    printf("DEBUG: %s\n", *username);

    // Unlock the caller
    ReleaseSemaphore(
        semaphore, // Semaphore to release
        1, // Release increment
        NULL // I don't care about the previous semaphore count
    );
    return TRUE;
}

// Second picker function that checks for a manual user request
DWORD WINAPI picker_handler_out(LPVOID arg)
{
    // Unpack the arguments
    thread_params_t* parameters = (thread_params_t*)arg;
    HANDLE semaphore = parameters->output_sem;
    HANDLE race_semaphore = parameters->race_condition_sem;

    // Get the target user index
    guid_t* return_value;
    while (TRUE)
    {
        int target = read_integer();
        if (target == -1)
        {
            printf("The selected index isn't valid\n");
            continue;
        }
        else if (target == REFRESH)
        {
            printf("DEBUG returning (guid_t*)REFRESH\n");
            return_value = (guid_t*)REFRESH;
            break;
        }
        printf("DEBUG trying get guid\n");
        guid_t* guid = try_get_guid(client_users_list, target);
        if (guid == NULL) printf("The input index isn't valid\n");
        else
        {
            return_value = guid;
            break;
        }
    }

    // Save the return values
    DWORD ret = WaitForSingleObject(race_semaphore, INFINITE);
    ERROR_HELPER(ret == WAIT_FAILED, "Error waiting for the semaphore");
    parameters->output_guid = return_value;
    parameters->username = NULL;

    // Unlock the caller
    ReleaseSemaphore(
        semaphore, // Semaphore to release
        1, // Release increment
        NULL // I don't care about the previous semaphore count
    );
    return TRUE;
}

guid_t* pick_target_user(string_t* username)
{
    printf("Pick a user to connect to: ");

    // Prepare the semaphore to synchronize the caller and the two threads
    HANDLE picker_semaphore = CreateSemaphore(
        NULL, // Default security attributes
        0, // Initial count to 0 to lock the caller function
        1, // Maximum count if both threads return at the same time
        NULL // Unnamed semaphore
    );
    ERROR_HELPER(picker_semaphore == NULL, "Error creating the semaphore");

    // Prepare the semaphore to synchronize the caller and the two threads
    HANDLE race_semaphore = CreateSemaphore(
        NULL, // Default security attributes
        1, // Initial count to 1
        1, // Same value for the maximum count
        NULL // Unnamed semaphore
    );
    ERROR_HELPER(picker_semaphore == NULL, "Error creating the race semaphore");

    // Prepare the struct to pass the parameters to the two threads
    thread_params_t arg = { 0 };
    arg.username = username;
    arg.output_guid = NULL;
    arg.output_sem = picker_semaphore;
    arg.race_condition_sem = race_semaphore;

    // Create the two picker threads
    HANDLE picker_threads[2];
    picker_threads[0] = CreateThread(
        NULL,       // Default security attributes
        0,          // Default stack size
        (LPTHREAD_START_ROUTINE) picker_handler_in, // Handler function
        (LPVOID)&arg, // Handler arguments
        0,          // Default creation flags
        NULL); // Ignore the thread identifier
    ERROR_HELPER(picker_threads[0] == NULL, "Error creating the first picker thread");
    picker_threads[1] = CreateThread(
        NULL,       // Default security attributes
        0,          // Default stack size
        (LPTHREAD_START_ROUTINE) picker_handler_out, // Handler function
        (LPVOID)&arg, // Handler arguments
        0,          // Default creation flags
        NULL); // Ignore the thread identifier
    ERROR_HELPER(picker_threads[1] == NULL, "Error creating the second picker thread");

    // Wait until at least one of the two spawned threads has returned
    DWORD ret = WaitForSingleObject(picker_semaphore, INFINITE);
    ERROR_HELPER(ret == WAIT_FAILED, "Error waiting for the semaphore");

    // Get the return value
    guid_t* return_value = arg.output_guid;

    // Cleanup
    ERROR_HELPER(!(TerminateThread(picker_threads[0], TRUE)
        || TerminateThread(picker_threads[1], TRUE)),
        "Error closing the picker threads");
    ERROR_HELPER((CloseHandle(picker_semaphore) && CloseHandle(race_semaphore)) == FALSE,
        "Error closing the semaphores");

    // Finally return the result
    return return_value;
}

void send_target_guid(const guid_t guid)
{
    string_t serialized = serialize_guid(guid);
    serialized = strncat(serialized, "\n", strlen(serialized) + 1);
    send_to_socket(socketd, serialized);
}

void chat(string_t username)
{
    // Open the console window
    printf("DEBUG opening new console ...\n");
    consoleWindow = prepare_chat_window();

    // Try to create the socket
    SOCKET chat_window_socket = socket(AF_INET, SOCK_STREAM, 0);
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
        ret = connect(chat_window_socket, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
        if (ret == SOCKET_ERROR && WSAGetLastError() == WSAEHOSTUNREACH) continue;
        ERROR_HELPER(ret == SOCKET_ERROR, "Error in the connect function");
        break;
    }

    printf("DEBUG connected to chat window socket!\n");

    while (TRUE)
    {
        ret = fflush(stdin);
        ERROR_HELPER(ret != 0, "Error while flushing the stdin");

        HANDLE sources[2];
        sources[1] = GetStdHandle(STD_INPUT_HANDLE);
        //sources[1] = event_handle;
        sources[0] = WSACreateEvent();
        WSAEventSelect(socketd, sources[0], FD_READ);

        // Wait for the stdin and the socket
        int index = WaitForMultipleObjects(2, sources, FALSE, INFINITE);
        ERROR_HELPER(index == WAIT_FAILED || index == WAIT_TIMEOUT,
            "Error in the WaitForMultipleObjects call");

        printf("DEBUG Something has changed his state %d: %s\n", index, index == 0 ? "SOCKET" : "stdin");

        // Check if there is a new message to display
        if (index == 0)
        {
            // Receive the message from the server
            char buffer[BUFFER_LENGTH];
            recv_from_socket(socketd, buffer, BUFFER_LENGTH);

            // Calculate the username to display
            char tmp[2] = { buffer[0], '\0' };
            int userIndex = atoi(tmp);

            // Display the message in the target console
            char message[BUFFER_LENGTH];
            char* temp_name = userIndex == 0 ? "Me" : username;
            snprintf(message, BUFFER_LENGTH, "%d[%s]%s", userIndex, temp_name, buffer + 1);
            printf("DEBUG message to send to chat window %s -- userIndex %d\n",
                    message, userIndex);
            send_to_socket(chat_window_socket, message);
        }
        else if (index == 1)
        {
            // Send the new message if necessary
            char message[BUFFER_LENGTH];
            char* gets_ret = fgets(message, BUFFER_LENGTH, stdin);
            if (gets_ret == NULL) continue;
            send_to_socket(socketd, message);

            // Send the message to the chat window
            char chat_window_message[BUFFER_LENGTH];
            snprintf(chat_window_message, BUFFER_LENGTH, "0[Me]%s", message);
            send_to_socket(chat_window_socket, chat_window_message);
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

    if (consoleWindow != NULL)
    {
        close_res = CloseHandle(consoleWindow);
        ERROR_HELPER(!close_res, "Error closing the chat window");
    }

    /*
    // Close the thread, if necessary
    if (return_thread != NULL)
    {
        close_res = CloseHandle(return_thread);
        ERROR_HELPER(!close_res, "Error closing the return thread");
    }
    if (event_handle != NULL)
    {
        close_res = CloseHandle(event_handle);
        ERROR_HELPER(!close_res, "Error closing the event handle");
    }
    */

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
    ERROR_HELPER(bytes == SOCKET_ERROR, "Error in recv function");
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
        if (target != NULL && target != (guid_t*)REFRESH)
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
        else if (target == (guid_t*)REFRESH)
        {
            printf("DEBUG the user requested a list refresh\n");
            char R[2] = { 'R', '\n' };
            send_to_socket(socketd, R);
            continue;
        }
        else chat(username + 1);
    }
}
