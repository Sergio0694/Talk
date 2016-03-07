#include <stddef.h> /* size_t */
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h> /* struct sockaddr_in, INADDR_ANY, INET_ADDSTRLEN */

#include "server.h"
#include "server_util.h"

void send_to_client(int socket, char* buf)
{
    int sent_bytes = 0;
    size_t msg_len = strlen(buf);

    buf[msg_len++] = '\n';

    int ret;
    while (TRUE)
    {
        ret = send(socket, buf + sent_bytes, msg_len, 0);
        if (ret == -1 && errno == EINTR) continue;
        ERROR_HELPER(ret, "Cannot send the message");
        sent_bytes += ret;
        if (sent_bytes == msg_len) break;
    }
}

// Receive a message from the client and add the string terminator
int recv_from_client(int socket, char* buf, size_t buf_len)
{
    int ret;
    int bytes_read = 0;

    // messages longer than buf_len will be truncated
    while (bytes_read <= buf_len) 
    {
        ret = recv(socket, buf + bytes_read, 1, 0);
        if (ret == 0 && errno == EWOULDBLOCK) return TIME_OUT_EXPIRED; // timeout expired
        if (ret == 0) return -1; // unexpected close from client
        ERROR_HELPER(ret, "Cannot read from socket");

        // a complete message from client finish with a \n
        if (buf[bytes_read] == '\n') break;

        // if there is no \n the message is truncated when is length is buf_len  
        if (bytes_read == buf_len) break;
        bytes_read += ret;
    }

    // substitute the \n with the string terminator
    buf[bytes_read++] = STRING_TERMINATOR;
    printf("\nMessage received: %s\n", buf);
    return bytes_read;
}

bool_t name_validation(char* name, size_t len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        if (name[i] == EXTERNAL_SEPARATOR || 
            name[i] == INTERNAL_SEPARATOR) return FALSE;
    }
    return TRUE;
}

void server_intial_setup(int socket_desc)
{
    int ret;

    // some fields are required to be filled with 0
    struct sockaddr_in server_addr = {0};

    int sockaddr_len = sizeof(struct sockaddr_in);

    // initialize sockaddr_in fields
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT_NUMBER);
    server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface

    // We enable SO_REUSEADDR to quickly restart our server after a crash
    int reuseaddr_opt = 1;
    ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
    ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

    // bind address to socket
    ret = bind(socket_desc, (struct sockaddr*) &server_addr, sockaddr_len);
    ERROR_HELPER(ret, "Cannot bind address to socket");

    // start listening
    ret = listen(socket_desc, MAX_CONN_QUEUE);
    ERROR_HELPER(ret, "Cannot listen on socket");
}

// Assign the struct timeval fields
void set_timeval(struct timeval* tv, int sec, int ms)
{
    tv->tv_sec = sec;
    tv->tv_usec = ms;
}