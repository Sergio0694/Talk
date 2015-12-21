#ifndef SERVER_H
#define SERVER_H

#include "Tools/Server/users_list.h"

#define PORT_NUMBER 25000
#define MAX_CONN_QUEUE  3   // max number of connections the server can queue

// macro to simplify error handling
#define ERROR_HELPER(ret, message)  do 									\
		{                                								\
            if (ret < 0) 												\
        	{                                              				\
                fprintf(stderr, "%s: %s\n", message, strerror(errno));  \
                exit(EXIT_FAILURE);                                     \
            }                                                           \
        } while (0)

typedef struct thread_args_s
{
    int sock_desc;
    struct sockaddr_in* address;
} conn_thread_args_t;

list_t users_list;

#endif