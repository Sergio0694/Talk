/* ===========================================================================
*  client_util.c
* ============================================================================

*  Authors:         (c) 2016 Sergio Pedri and Andrea Salvati */

#include <Winsock2.h>
#include <string.h>

#include "client_util.h"
#include "..\Shared\types.h"

/* ============================================================================
*  Generic functions
*  ========================================================================= */

// Sends a buffer to a remote socket
void send_to_socket(SOCKET socket, char* buf)
{
    // Calculates the length of the message to send
    int sent_bytes = 0;
    size_t msg_len = strlen(buf);

    while (TRUE)
    {
        // Starts the operation
        int ret = send(socket, buf + sent_bytes, msg_len, 0);

        // Continue in case of an interrupt
        if (ret == -1 && WSAGetLastError() == WSAEINTR) continue;

        // Check for errors
        ERROR_HELPER(ret == SOCKET_ERROR, "Cannot send the message");

        // Update the progress and continue
        sent_bytes += ret;
        if (sent_bytes == msg_len) break;
    }
}

// Receives a buffer from a given socket
int recv_from_socket(SOCKET socket, char* buf, size_t buf_len)
{
    // Counter of the read bytes
    int bytes_read = 0;

    // Clear the buffer
    strncpy(buf, "", buf_len);

    // Messages longer than buf_len will be truncated
    while (bytes_read <= buf_len)
    {
        int ret = recv(socket, buf + bytes_read, 1, 0);
        ERROR_HELPER(ret == 0, "The server unexpectedly closed the connection");
        if (ret == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) continue;
        if (ret == SOCKET_ERROR) fprintf(stderr, "%d: ", WSAGetLastError());
        ERROR_HELPER(ret == SOCKET_ERROR, "Cannot read from socket!");
        if (buf[bytes_read] == '\n') break;
        bytes_read += ret;
        if (bytes_read == buf_len) break;
    }
    buf[bytes_read] = STRING_TERMINATOR;
    return bytes_read;
}

// Initializes the WINSOCKET API
void initialize_socket_API()
{
    // Make the API call
    WSADATA data = { 0 };
    int startup_ret = WSAStartup(MAKEWORD(2, 2), (LPWSADATA)&data);

    // Everything went fine, just return
    if (startup_ret == 0) return;

    // Display an error message and exit the process
    ERROR_HELPER(startup_ret == WSAVERNOTSUPPORTED,
        "The 2.2 SOCKET APIs are not supported on your system");
    ERROR_HELPER(startup_ret != 0, "There was an error during the SOCKET API initialization");
}

// Performs a fgets and checks for its return value
void checked_fgets(char* buffer, size_t buffer_length)
{
    char* fgets_ret;
    strncpy(buffer, "", buffer_length);
    fgets_ret = fgets(buffer, buffer_length, stdin);
    ERROR_HELPER(fgets_ret == NULL && ferror(stdin), "Error in fgets");
}
