/* ===========================================================================
*  client_util.h
* ============================================================================

*  Authors:         (c) 2015 Sergio Pedri and Andrea Salvati */

#ifndef CLIENT_UTIL_H
#define CLIENT_UTIL_H

#include <stdio.h>

// ==================== Generic macros ======================

#define ERROR_HELPER(test, message)					\
	do												\
	{												\
		if (test)									\
		{											\
			fprintf(stderr, "%s\n", message);			\
			exit(EXIT_FAILURE);						\
		}											\
	} while (0)

// ==================== Functions ======================

/* =====================================================================
*  SendToServer
*  =====================================================================
*  Description:
*    Sends a message buffers to a given socket
*  Parameters:
*    socket ---> The socket with an open connection to the server
*    buf ---> The buffer that contains the message to send */
void send_to_server(SOCKET socket, char* buf);

/* =====================================================================
*  ReceiveFromServer
*  =====================================================================
*  Description:
*    Receives a message from a socket
*  Parameters:
*    socket ---> The socket with an open connection to the server
*	 buf ---> The buffer to use to store the received message
*    buf_len ---> The size of the target buffer */
int recv_from_server(SOCKET socket, char* buf, size_t buf_len);

#endif
