#include <Winsock2.h>
#include <string.h>

#include "client_util.h"
#include "..\Tools\Shared\types.h"

/* ============================================================================
*  Generic functions
*  ========================================================================= */

// Sends a buffer to a remote socket
void send_to_server(SOCKET socket, char* buf)
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
		ERROR_HELPER(ret, "Cannot send the message");

		// Update the progress and continue
		sent_bytes += ret;
		if (sent_bytes == msg_len) break;
	}
}

// Receives a buffer from a given socket
int recv_from_server(SOCKET socket, char* buf, size_t buf_len)
{
	// Counter of the read bytes
	int bytes_read = 0;

	// Messages longer than buf_len will be truncated
	while (bytes_read <= buf_len)
	{
		int ret = recv(socket, buf + bytes_read, 1, 0);
		ERROR_HELPER(ret == 0, "The server unexpectedly closed the connection");
		ERROR_HELPER(ret == -1, "Cannot read from socket!");
		if (buf[bytes_read] == STRING_TERMINATOR) break;
		if (bytes_read == buf_len) break;
		bytes_read += ret;
	}
	return bytes_read;
}