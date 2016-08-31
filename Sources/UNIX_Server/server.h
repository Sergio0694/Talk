/* ============================================================================
*  server.h
* ============================================================================

*  Authors:         (c) 2016 Sergio Pedri and Andrea Salvati */

#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>  /* fprint() */
#include <stdlib.h> /* EXIT_FAILURE */

#define PORT_NUMBER 25000
#define MAX_CONN_QUEUE  3   // Max number of connections the server can queue

#define BUFFER_LENGTH 1024

// Macro to simplify error handling
#define ERROR_HELPER(ret, message)  do                                  \
        {                                                               \
            if (ret < 0)                                                \
            {                                                           \
                fprintf(stderr, "%s: %s\n", message, strerror(errno));  \
                exit(EXIT_FAILURE);                                     \
            }                                                           \
        } while (0)

#endif
