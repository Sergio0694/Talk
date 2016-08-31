/* ===========================================================================
*  client_util.h
* ============================================================================

*  Authors:         (c) 2015 Sergio Pedri and Andrea Salvati */

#ifndef CLIENT_UTIL_H
#define CLIENT_UTIL_H

#include <stdio.h>
#include <winsock2.h>

// ==================== Generic macros ======================

#define ERROR_HELPER(test, message)                   \
    do                                                \
    {                                                 \
        if (test)                                     \
        {                                             \
            fprintf(stderr, "%s\n", message);         \
            exit(EXIT_FAILURE);                       \
        }                                             \
    } while (0)

// ==================== Functions ======================

/* =====================================================================
*  SendToSocket
*  =====================================================================
*  Description:
*    Sends a message buffers to a given socket
*  Parameters:
*    socket ---> The socket with an open connection to the server
*    buf ---> The buffer that contains the message to send */
void send_to_socket(SOCKET socket, char* buf);

/* =====================================================================
*  ReceiveFromSocket
*  =====================================================================
*  Description:
*    Receives a message from a socket
*  Parameters:
*    socket ---> The socket with an open connection to the server
*     buf ---> The buffer to use to store the received message
*    buf_len ---> The size of the target buffer */
int recv_from_socket(SOCKET socket, char* buf, size_t buf_len);

/* =====================================================================
*  InitializeSocketAPI
*  =====================================================================
*  Description:
*    Initializes the WINAPI for the sockets */
void initialize_socket_API();

/* =====================================================================
*  CheckedFgets
*  =====================================================================
*  Description:
*    Performs a fgets and checks for its return valu
*  Parameters:
*    buffer ---> The buffer used to save the read input
*    buffer_length ---> The length of the buffer */
void checked_fgets(char* buffer, size_t buffer_length);

#endif
