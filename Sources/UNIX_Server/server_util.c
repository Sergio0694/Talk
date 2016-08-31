/* ===========================================================================
*  server_util.c
* ============================================================================

*  Authors:         (c) 2016 Sergio Pedri and Andrea Salvati */

#include <stddef.h> /* size_t */
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h> /* struct sockaddr_in, INADDR_ANY, INET_ADDSTRLEN */

#include "server.h"
#include "server_util.h"

// Send a message to the client
int send_to_client(int socket, char* buf)
{
    int ret;
    size_t sent_bytes = 0, msg_len = strlen(buf);

    // Substitute the string terminator with a \n
    buf[msg_len++] = '\n';

    // Handle partial sends
    while (TRUE)
    {
        ret = send(socket, buf + sent_bytes, msg_len, 0);
        if (ret == -1 && errno == EINTR) continue;
        if (ret < 0) return ret;
        sent_bytes += ret;
        if (sent_bytes == msg_len) break;
    }
    return sent_bytes;
}

// Receive a message from the client and add the string terminator
int recv_from_client(int socket, char* buf, size_t buf_len)
{
    int ret;
    size_t bytes_read = 0;
    strncpy(buf, "", buf_len);

    // Messages longer than buf_len will be truncated
    while (bytes_read <= buf_len)
    {
        ret = recv(socket, buf + bytes_read, 1, 0);
        if (ret == -1 && errno == EWOULDBLOCK) return TIME_OUT_EXPIRED; // timeout expired
        if (ret == 0) return CLIENT_UNEXPECTED_CLOSE; // unexpected close from client
        if (ret < 0) return UNEXPECTED_ERROR;

        // A complete message from client finishes with a \n
        if (buf[bytes_read] == '\n') break;

        bytes_read += ret;

        // If there is no \n the message is truncated when its length is buf_len
        if (bytes_read == buf_len) break;
    }

    // substitute the \n with the string terminator
    buf[bytes_read] = STRING_TERMINATOR;
    printf("\nMessage received: %s\n", buf);
    return bytes_read;
}

// Check for the presence of unwanted special character
bool_t name_validation(char* name, size_t len)
{
    unsigned i;
    for (i = 0; i < len; i++)
    {
        if (name[i] == EXTERNAL_SEPARATOR ||
            name[i] == INTERNAL_SEPARATOR) return FALSE;
    }
    return TRUE;
}

// Perform initial operations like bind and listen
void server_intial_setup(int socket_desc)
{
    int ret;

    // Some fields are required to be filled with 0
    struct sockaddr_in server_addr = { 0 };
    int sockaddr_len = sizeof(struct sockaddr_in);

    // Initialize sockaddr_in fields
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT_NUMBER);
    server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface

    // We enable SO_REUSEADDR to quickly restart our server after a crash
    int reuseaddr_opt = 1;
    ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
    ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

    // Bind address to socket
    ret = bind(socket_desc, (struct sockaddr*)&server_addr, sockaddr_len);
    ERROR_HELPER(ret, "Cannot bind address to socket");

    // Start listening
    ret = listen(socket_desc, MAX_CONN_QUEUE);
    ERROR_HELPER(ret, "Cannot listen on socket");
}

// Assign the struct timeval fields
void set_timeval(struct timeval* tv, int sec, int ms)
{
    tv->tv_sec = sec;
    tv->tv_usec = ms;
}
