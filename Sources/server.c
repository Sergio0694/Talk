#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>     /* strerror() and strlen() */
#include <pthread.h>
#include <unistd.h>     /* probably not used - to be verified */
#include <arpa/inet.h>  /* htons(), ntohs() and inet_ntop() */
#include <netinet/in.h> /* struct sockaddr_in, INADDR_ANY, INET_ADDSTRLEN */
#include <sys/socket.h>
#include <sys/time.h>   /* struct timeval */
#include <sys/types.h>  /* key_t, pthread_t */
#include <sys/ipc.h>    /* IPC_CREAT, IPC_EXCL, IPC_NOWAIT */
#include <sys/sem.h>
#include <signal.h>

#include "Tools/Shared/types.h"
#include "Tools/Shared/guid.h"
#include "Tools/Server/users_list.h"
#include "server.h"
#include "server_util.h"

/* ===== GLOBAL VARIABLES ===== */
typedef struct thread_args_s
{
    int sock_desc;
    int semid;
    struct sockaddr_in* address;
} conn_thread_args_t;

list_t users_list;

// used in calls to semctl()
#ifdef _SEM_SEMUN_UNDEFINED
    union semun
    {
        int val;                /* value for SETVAL */
        struct semid_ds* buf;   /* buffer for IPC_STAT, IPC_SET */
        unsigned short* array;  /* array for GETALL, SETALL */
    #if defined(__linux__)
        struct seminfo* __buf; /* buffer for IPC_INFO (linux-specific) */
    #endif
    };
#endif
/* ============================ */

void* client_connection_handler(void* arg)
{
    // get handler arguments
    conn_thread_args_t* args = (conn_thread_args_t*)arg;
    int socketd = args->sock_desc;
    int semid = args->semid;
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

    // configure the SIGPIPE handler
    sigset_t set;
    ret = sigfillset(&set); // all signal will be blocked
    ERROR_HELPER(ret, "Cannot fill the sigset");
    struct sigaction sigact;
    sigact.sa_handler = /* some defined handler */;
    sigact.sa_mask = set; // the handler cannot be interrupted
    sigact.sa_flags = /* I don't know now */;
    ret = sigaction(SIGPIPE, &sigact, NULL); // don't care about the old sigact
    ERROR_HELPER(ret, "Cannot change signal disposition");

    // Welcome message
    sprintf(buf, "Welcome to Talk\nPlease choose a name: ");
    send_to_client(socketd, buf);

    // set a 2 minutes timeout for choose the name
    tv.tv_sec = 120;
    tv.tv_usec = 0;
    ret = setsockopt(socketd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    ERROR_HELPER(ret, "Cannot set SO_RCVTIMEO option");

    // save the name
    int name_len = recv_from_client(socketd, buf, buf_len);
    if (name_len < 0)
    {
        if (name_len == TIME_OUT_EXPIRED) send_to_client(socketd, "I'm waiting too much");
        // else unexpected close from client
        int ret = close(args->socket);
        ERROR_HELPER(ret, "Errore nella chiusura di una socket");
        free(args->address);
        free(args);
        pthread_exit(NULL);
    }
    while (name_len == 0)
    {
        sprintf(buf, "0Please choose a non-empty name: ");
        send_to_client(socketd, buf);
        name_len = recv_from_client(socketd, buf, buf_len);
    }
    while (!name_validation(buf, name_len))
    {
        sprintf(buf, "0Please choose a name that not contains | or ~ character: ");
        send_to_client(socketd, buf);
        name_len = recv_from_client(socketd, buf, buf_len);
    }
    char name[name_len];
    strcpy(name, buf); // name contains \0

    // send the generated guid to the client
    guid_t guid = new_guid();
    char* serialized_guid = serialize_guid(guid);
	sprintf(buf, "1%s", serialized_guid);
    send_to_client(socketd, buf);

    // ###### critical section here - semaphore needed ######

    // sem_num = 0 --> operate on semaphore 0
    // sem_op = 0 --> wait for value to equal 0
    struct sembuf sop = { 0 };
    ret = semop(semid, &sop, 1);
    ERROR_HELPER(ret, "Cannot operate on semaphore");

    // if we are here sem value is equal to 0 -- increment it
    sop.sem_op = 1;
    ret = semop(semid, &sop, 1);
    ERROR_HELPER(ret, "Cannot operate on semaphore");

    // add the user to users_list
    add(users_list, name, guid, client_ip);

    // make available the users_list
    sop.sem_op = -1;
    ret = semop(semid, &sop, 1);
    ERROR_HELPER(ret, "Cannot operate on semaphore");

    // ######################################################

    // send the serialized users list to the client
    buf = serialize_list(users_list);
    send_to_client(socketd, buf);

    // set a 5 seconds timeout for recv on client socket
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    ret = setsockopt(socketd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    ERROR_HELPER(ret, "Cannot set SO_RCVTIMEO option");
}

int main()
{
    // aux var
    int ret;
    int socket_desc, client_desc;

    // server setup
    users_list = create();
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_HELPER(socket_desc, "cannot open server socket");
    server_intial_setup(socket_desc);

    /* ==== semaphore creation and initialization ==== */

    // create the semaphore
    int semid = semget(IPC_PRIVATE, /* semnum = */ 1, 0660);
    ERROR_HELPER(semid, "Error in semaphore creation");

    // initialize the semaphore to 0
    struct semun arg;
    arg.val = 0;
    ret = semctl(semid, 0, SETVAL, arg);
    ERROR_HELPER(ret, "Cannot initialize the semaphore");

    // we allocate client_addr dynamically and initialize it to zero
    struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));

    while(TRUE)
    {
    	// accept incoming connection
    	client_desc = accept(socket_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
        ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");
        
        pthread_t thread;

        // argument for thread
        conn_thread_args_t* args = (conn_thread_args_t*)malloc(sizeof(conn_thread_args_t));
        args->sock_desc = client_desc;
        args->semid = semid;
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