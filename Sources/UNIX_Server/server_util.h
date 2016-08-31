/* ===========================================================================
*  server_util.h
* ============================================================================

*  Authors:         (c) 2016 Sergio Pedri and Andrea Salvati */

#ifndef SERVER_UTIL_H
#define SERVER_UTIL_H

#include <unistd.h> /* size_t */
#include <sys/time.h>

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

/* =====================================================================
*  NameValidation
*  =====================================================================
*  Description:
*    Check if the name is valid and do not contain special character used
*    by the serialization function
*  Parameters:
*    name ---> The name to check
*	 len ---> The length of the name*/
bool_t name_validation(char* name, size_t len);

/* =====================================================================
*  ServerInitialSetup
*  =====================================================================
*  Description:
*    Perform the initial operation such as the bind and the listen
*    operations
*  Parameters:
*    socket_desc ---> The socket used to perform such operations*/
void server_intial_setup(int socket_desc);

/* =====================================================================
*  SetTimeval
*  =====================================================================
*  Description:
*    Set the given timeval structure's field to the given sec and millisec
*  Parameters:
*    tv ---> The structure to be filled
*    sec ---> The seconds to set for the tv_sec field
*    ms ---> The milliseconds to set for the tv_usec field*/
void set_timeval(struct timeval* tv, int sec, int ms);

#endif
