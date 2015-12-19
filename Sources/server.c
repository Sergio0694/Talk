#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h> // strerror()
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons()
#include <netinet/in.h> // struct sockaddr_in

#define PORT_NUMBER 25000
#define MAX_CONN_QUEUE  3   // max number of connections the server can queue
#define SERVER_ADDRESS  "127.0.0.1"

// macro to simplify error handling
#define ERROR_HELPER(ret, message)  do 									\
		{                                								\
            if (ret < 0) 												\
        	{                                              				\
                fprintf(stderr, "%s: %s\n", message, strerror(errno));  \
                exit(EXIT_FAILURE);                                     \
            }                                                           \
        } while (0)

#typedef enum { FALSE, TRUE } bool_t

void* handler(void* arg)
{
	// TODO
}

int main(int argc, char* argv[])
{
    int ret;

    int socket_desc, client_desc;

    // some fields are required to be filled with 0
    struct sockaddr_in server_addr = {0};

    int sockaddr_len = sizeof(struct sockaddr_in); 

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

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

    // we allocate client_addr dynamically and initialize it to zero
    struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));

    while(TRUE)
    {
    	// accept incoming connection
    	client_desc = accept(socket_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
        ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");

        // client_desc option settings TODO

        pthread_t thread;

        // argument for thread ...

        if (pthread_create(&thread, NULL, /* handler */, /* args */) != 0)
        {
            fprintf(stderr, "Can't create a new thread, error %d\n", errno);
            exit(EXIT_FAILURE);
        }

        pthread_detach(thread);

        // we can't just reset fields: we need a new buffer for client_addr!
        client_addr = calloc(1, sizeof(struct sockaddr_in));
    }
}