#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>     /* strerror() and strlen() */
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>  /* htons(), ntohs() and inet_ntop() */
#include <netinet/in.h> /* struct sockaddr_in, INADDR_ANY, INET_ADDSTRLEN */
#include <sys/socket.h>
#include <sys/time.h>   /* struct timeval */

#include "Tools/Shared/types.h"
#include "Tools/Shared/guid.h"
#include "server.h"

void send_to_client(int socket, char* buf)
{
    int sent_bytes = 0;
    size_t msg_len = strlen(buf);
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

size_t recv_from_client(int socket, char* buf, size_t buf_len) 
{
    int ret;
    int bytes_read = 0;

    // messages longer than buf_len will be truncated
    while (bytes_read <= buf_len) 
    {        
        ret = recv(socket, buf + bytes_read, 1, 0);
        if (ret == 0) return -1;
        ERROR_HELPER(ret, "Cannot read from socket!\n");
        if (buf[bytes_read] == '\n') break;
        if (bytes_read == buf_len)
        {
            buf[bytes_read] = '\n';
            break;
        }
        bytes_read += ret;
    }
    buf[bytes_read] = '\0';
    return bytes_read; // bytes_read == strlen(buf)
}

bool_t name_validation(char* name, size_t len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        if (name[i] == '|' || name[i] == '~') return FALSE;
    }
    return TRUE;
}

void* handler(void* arg)
{
    // get handler arguments
    conn_thread_args_t* args = (conn_thread_args_t*)arg;
    int socketd = args->sock_desc;
    struct sockaddr_in* client_addr = args->address;

    // aux variables
    char buf[1024];
    size_t buf_len = sizeof(buf);

    // parse client IP address and port
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port); // port number is an unsigned short

    // Welcome message
    sprintf(buf, "Welcome to Talk\nPlease choose a name: ");
    send_to_client(socketd, buf);

    // save the name
    size_t name_len = recv_from_client(socketd, buf, buf_len);
    if (name_len == -1) // unexpected close of socket from client
    {
        int ret = close(socketd);
        ERROR_HELPER(ret, "Errore nella chiusura di una socket");
        free(args->address);
        free(args);
        pthread_exit(NULL);
    }
    while (name_len == 0)
    {
        sprintf(buf, "Please choose a non-empty name: ");
        send_to_client(socketd, buf);
        name_len = recv_from_client(socketd, buf, buf_len);
    }
    while (!name_validation(buf, name_len))
    {
        sprintf(buf, "Please choose a name that not contains | or ~ character: ");
        send_to_client(socketd, buf);
        name_len = recv_from_client(socketd, buf, buf_len);
    }
    char name[name_len];
    strcpy(name, buf); // name contains \0

    // send the generated guid to the client
    guid_t guid = new_guid();
    buf = serialize_guid(guid);
    send_to_client(socketd, buf);

    // ###### critical section here - semaphore needed ######

    // add the user to users_list
    add(users_list, name, guid, client_ip);

    // ######################################################

	struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(socketd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

void server_intial_setup(int socket_desc)
{
    int ret;

    users_list = create();

    // some fields are required to be filled with 0
    struct sockaddr_in server_addr = {0};

    int sockaddr_len = sizeof(struct sockaddr_in);

    // initialize sockaddr_in fields
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(PORT_NUMBER);
    sockaddr_in.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface

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

int main(int argc, char* argv[])
{
    int ret;

    int socket_desc, client_desc;

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_HELPER(socket_desc, "cannot open server socket");

    server_intial_setup(socket_desc);

    // we allocate client_addr dynamically and initialize it to zero
    struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));

    while(TRUE)
    {
    	// accept incoming connection
    	client_desc = accept(socket_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
        ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");
        
        pthread_t thread;

        // argument for thread ...
        conn_thread_args_t* args = (conn_thread_args_t*)malloc(sizeof(conn_thread_args_t));
        args->sock_desc = client_desc;
        args->address = client_addr;

        if (pthread_create(&thread, NULL, /* handler */, args) != 0)
        {
            fprintf(stderr, "Can't create a new thread, error %d\n", errno);
            exit(EXIT_FAILURE);
        }

        pthread_detach(thread);

        // we can't just reset fields: we need a new buffer for client_addr!
        client_addr = calloc(1, sizeof(struct sockaddr_in));
    }
}