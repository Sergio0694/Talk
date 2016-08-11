/* ===========================================================================
*  server_util.h
* ============================================================================

*  Authors:         (c) 2015 Sergio Pedri and Andrea Salvati */

#ifndef SERVER_UTIL_H
#define SERVER_UTIL_H

#include <unistd.h> /* size_t */
#include "../Shared/types.h"

// ==================== Generic macros ======================

#define CLIENT_UNEXPECTED_CLOSE -1
#define TIME_OUT_EXPIRED -2
#define CONNECTION_REQUESTED -3
#define UNEXPECTED_ERROR -4

// ==================== Functions ======================

/* =====================================================================
*  SendToClient
*  =====================================================================
*  Description:
*    Sends a message buffers to a given socket
*  Parameters:
*    socket ---> The socket with an open connection to the client
*    buf ---> The buffer that contains the message to send */
int send_to_client(int socket, char* buf);

/* =====================================================================
*  ReceiveFromClient
*  =====================================================================
*  Description:
*    Receives a message from a socket
*  Parameters:
*    socket ---> The socket with an open connection to the client
*	 buf ---> The buffer to use to store the received message
*    buf_len ---> Max bytes to read */
int recv_from_client(int socket, char* buf, size_t buf_len);

bool_t name_validation(char* name, size_t len);

void server_intial_setup(int socket_desc);

void set_timeval(struct timeval* tv, int sec, int ms);

#endif
