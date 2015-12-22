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
#include <sys/types.h>
#include <sys/ipc.h>    /* IPC_CREAT, IPC_EXCL, IPC_NOWAIT */
#include <sys/sem.h>
#include <signal.h>

#include "Tools/Shared/types.h"
#include "Tools/Shared/guid.h"
#include "Tools/Server/users_list.h"
#include "server.h"
#include "server_util.h"

/* ===== GLOBAL VARIABLES ===== */
list_t users_list;

typedef struct thread_args_s
{
    int sock_desc;
    struct sockaddr_in* address;
} conn_thread_args_t;
/* ============================ */

void* client_connection_handler(void* arg)
{
    // get handler arguments
    conn_thread_args_t* args = (conn_thread_args_t*)arg;
    int socketd = args->sock_desc;
    struct sockaddr_in* client_addr = args->address;

    // aux variables
    char buf[1024];
    size_t buf_len = sizeof(buf);
    int ret;

    // timeval struct for recv timeout
    struct timeval tv;

    // parse client IP address and port
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port); // port number is an unsigned short

    // Welcome message
    sprintf(buf, "Welcome to Talk\nPlease choose a name: ");
    send_to_client(socketd, buf);

    // set a 2 minutes timeout for choose the name
    tv.tv_sec = 120;
    tv.tv_usec = 0;
    ret = setsockopt(socketd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    ERROR_HELPER(ret, "Cannot set SO_RCVTIMEO option");

    // save the name
    size_t name_len = recv_from_client(socketd, buf, buf_len);
    if (name_len < 0)
    {
        if (name_len == TIME_OUT_EXPIRED) send_to_client(socketd, "I'm waiting too much");
        // else unexpected close from client
        int ret = close(socketd);
        ERROR_HELPER(ret, "Error while closing the socket");
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

    // set a 5 seconds timeout for recv on client socket
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    ret = setsockopt(socketd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    ERROR_HELPER(ret, "Cannot set SO_RCVTIMEO option");
}

int main(int argc, char* argv[])
{
    int ret;

    users_list = create();

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

        if (pthread_create(&thread, NULL, client_connection_handler, args) != 0)
        {
            fprintf(stderr, "Can't create a new thread, error %d\n", errno);
            exit(EXIT_FAILURE);
        }

        pthread_detach(thread);

        // we can't just reset fields: we need a new buffer for client_addr!
        client_addr = calloc(1, sizeof(struct sockaddr_in));
    }
}