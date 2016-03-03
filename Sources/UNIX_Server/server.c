#include <errno.h>
#include <string.h>     /* strerror() and strlen() */
#include <pthread.h>
#include <arpa/inet.h>  /* htons(), ntohs() and inet_ntop() */
#include <netinet/in.h> /* struct sockaddr_in, INADDR_ANY, INET_ADDSTRLEN */
#include <sys/socket.h>
#include <sys/time.h>   /* struct timeval */
#include <sys/types.h>  /* key_t, pthread_t */
#include <sys/ipc.h>    /* IPC_CREAT, IPC_EXCL, IPC_NOWAIT */
#include <sys/sem.h>
#include <signal.h>

#include "../Tools/Shared/guid.h"
#include "../Tools/Server/users_list.h"
#include "../Tools/Shared/string_helper.h"
#include "chat_handler.h"
#include "server_util.h"
#include "server.h"

/* ===== GLOBAL VARIABLES ===== */
typedef struct thread_args_s
{
    int sock_desc;
    int semid;
    struct sockaddr_in* address;
} conn_thread_args_t;

list_t users_list;

// used in calls to semctl()
union semun
{
    int val;                /* value for SETVAL */
    struct semid_ds* buf;   /* buffer for IPC_STAT, IPC_SET */
    unsigned short* array;  /* array for GETALL, SETALL */
#if defined(__linux__)
    struct seminfo* __buf;  /* buffer for IPC_INFO (linux-specific) */
#endif
};
/* ============================ */

/* ===== PRIVATE FUNCTIONS ===== */

static void close_and_cleanup(conn_thread_args_t* args, char* msg)
{
	int socketd = args->sock_desc;
	struct sockaddr_in* client_addr = args->address;

    fprintf(stderr, "%s\n", msg);

	int ret = close(socketd);
	ERROR_HELPER(ret, "Cannot close the socket");
	free(client_addr);
	free(args);
	pthread_exit(NULL);
}

#define EPIPE_RCV if (errno == EPIPE) close_and_cleanup(args, "EPIPE error received")

static void name_pickup(conn_thread_args_t* args, int* name_len, char** name)
{
	char buf[1024];
	size_t buf_len = sizeof(buf);

	int socketd = args->sock_desc;

    printf("Waiting for name recv\n");
	*name_len = recv_from_client(socketd, buf, buf_len);
	if (*name_len < 0)
    {
        if (*name_len == TIME_OUT_EXPIRED) close_and_cleanup(args, "Waited too much for a name");
        close_and_cleanup(args, "Unexpected close from client");
    }
	while (*name_len == 0)
	{
		sprintf(buf, "0Please choose a non-empty name: ");
		send_to_client(socketd, buf);
		EPIPE_RCV;
		*name_len = recv_from_client(socketd, buf, buf_len);
	}
	while (!name_validation(buf, *name_len))
	{
		sprintf(buf, "0Please choose a name that not contains | or ~ character: ");
		send_to_client(socketd, buf);
		EPIPE_RCV;
		*name_len = recv_from_client(socketd, buf, buf_len);
	}
    printf("Correct name received ");
	*name = (char*)malloc(sizeof(char) * (*name_len));
	strcpy(*name, buf);
    printf("-- The name is %s\n", *name);
}

#define REMOVE_GUID(guid) do                                    \
        {                                                       \
            sop.sem_op = -1;                                    \
            ret = semop(semid, &sop, 1);                        \
            ERROR_HELPER(ret, "Cannot operate on semaphore");   \
                                                                \
            remove_guid(users_list, guid);                      \
                                                                \
            sop.sem_op = 1;                                     \
            ret = semop(semid, &sop, 1);                        \
            ERROR_HELPER(ret, "Cannot operate on semaphore");   \
        } while (0)         

/* ============================= */

void* client_connection_handler(void* arg)
{
    printf("Handling connection\n");
    // get handler arguments
    conn_thread_args_t* args = (conn_thread_args_t*)arg;
    int socketd = args->sock_desc;
    int semid = args->semid;
    struct sockaddr_in* client_addr = args->address;

    // aux variables
    char buf[1024], client_ip[INET_ADDRSTRLEN];
    size_t buf_len = sizeof(buf);
    int ret, name_len;
    string_t temp, name;
    pthread_t chat_thread;

    // timeval struct for recv timeout
    struct timeval tv;

    // parse client IP address and port
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port);

    // Welcome message
    sprintf(buf, "Welcome to Talk\nPlease choose a name: ");
    printf("Sending the Welcome message -- The client should see: %s\n", buf);
    send(socketd, buf, strlen(buf), 0);
	EPIPE_RCV;
    printf("Welcome message sent on %d socket\n", socketd);

    // set a 2 minutes timeout for choose the name
    tv.tv_sec = 120;
    tv.tv_usec = 0;
    ret = setsockopt(socketd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    ERROR_HELPER(ret, "Cannot set SO_RCVTIMEO option");

    // save the name
	name_pickup(args, &name_len, &name);

    // send the generated guid to the client
    guid_t guid = new_guid();
    temp = serialize_guid(guid);
	sprintf(buf, "1%s", temp);
    printf("Sending the generated guid %s\n", buf + 1);
    send_to_client(socketd, buf);
    printf("Guid sent\n");
	EPIPE_RCV;

    // ###### critical section here - semaphore needed ######

    // sem_num = 0 --> operate on semaphore 0
    // sem_op = -1 --> decrement the semaphore value, wait if is 0
    struct sembuf sop = { 0 };
    sop.sem_op = -1;
    ret = semop(semid, &sop, 1);
    ERROR_HELPER(ret, "Cannot operate on semaphore");

    /* if we are here sem value is equal to 0
     * all other threads wait until the semaphore will be incremented */

    // add the user to users_list
    add(users_list, name, guid, socketd);

    // make the users_list available
    sop.sem_op = 1;
    ret = semop(semid, &sop, 1);
    ERROR_HELPER(ret, "Cannot operate on semaphore");

    // ######################################################

    while (TRUE)
    {
        // sem_num = 0 --> operate on semaphore 0
        // sem_op = -1 --> decrement the semaphore value, wait if is 0
        sop.sem_op = -1;
        ret = semop(semid, &sop, 1);
        ERROR_HELPER(ret, "Cannot operate on semaphore");

        // serialize the users list
        temp = serialize_list(users_list);

        // make the users_list available
        sop.sem_op = 1;
        ret = semop(semid, &sop, 1);
        ERROR_HELPER(ret, "Cannot operate on semaphore");

        // send the serialized users list to the client
        sprintf(buf, "%s", temp);
        printf("Sending the users list..\n");
        send_to_client(socketd, buf);
    	EPIPE_RCV;
        printf("Users list sent\n");

        // wait for the client connection choice
        ret = recv_from_client(socketd, buf, buf_len);
        if (ret == TIME_OUT_EXPIRED)
        {
            REMOVE_GUID(guid);
            close_and_cleanup(args, "The client timed out");
        }

        // if the message received is DISCONNECT the client will be disconnected from the server
        if (strcmp(buf, "DISCONNECT") == 0)
        {
            REMOVE_GUID(guid);
            close_and_cleanup(args, "The client want to be disconnected");
        }
        
        // take the guid of the chosen client and his connection socket
        guid_t chosen_guid = deserialize_guid(buf);
        int chosen_socket = get_socket(users_list, chosen_guid);

        // build the struct for the chat handler
        suid_t chat_users[2] = { {socketd, guid}, {chosen_socket, chosen_guid} };
        chat_args_t chat_args;
        chat_args.suid[0] = chat_users[0];
        chat_args.suid[1] = chat_users[1];

        // sem_num = 0 --> operate on semaphore 0
        // sem_op = -1 --> decrement the semaphore value, wait if is 0
        sop.sem_op = -1;
        ret = semop(semid, &sop, 1);
        ERROR_HELPER(ret, "Cannot operate on semaphore");

        // set the available flag for the two users to false
        set_available_flag(users_list, guid, FALSE);
        set_available_flag(users_list, chosen_guid, FALSE);
        
        // make the users_list available
        sop.sem_op = 1;
        ret = semop(semid, &sop, 1);
        ERROR_HELPER(ret, "Cannot operate on semaphore");

        // start the chat
        // ...

    } // end of while
}

int main()
{
    // aux var
    int ret;
    int socket_desc, client_desc;
    int sockaddr_len = sizeof(struct sockaddr_in);

    // server setup
    users_list = create();
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_HELPER(socket_desc, "cannot open server socket");
    server_intial_setup(socket_desc);

    /* ==== semaphore creation and initialization ==== */

    // create the semaphore
    int semid = semget(IPC_PRIVATE, /* semnum = */ 1, IPC_CREAT | 0660);
    ERROR_HELPER(semid, "Error in semaphore creation");

    // initialize the semaphore to 1
    union semun arg;
    arg.val = 1;
    ret = semctl(semid, 0, SETVAL, arg);
    ERROR_HELPER(ret, "Cannot initialize the semaphore");

	/* =============================================== */

    // preventing the generation of SIGPIPE -- an EPIPE will be generated instead
    signal(SIGPIPE, SIG_IGN);

    // we allocate client_addr dynamically and initialize it to zero
    struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));

    while(TRUE)
    {
        printf("Waiting for connections ...\n");
    	// accept incoming connection
    	client_desc = accept(socket_desc, (struct sockaddr*)client_addr, (socklen_t*)&sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
        ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");

        printf("Connection accepted -- client socket is: %d\n", client_desc);

        pthread_t thread;

        // argument for thread
        conn_thread_args_t* args = (conn_thread_args_t*)malloc(sizeof(conn_thread_args_t));
        args->sock_desc = client_desc;
        args->semid = semid;
        args->address = client_addr;

        if (pthread_create(&thread, NULL, client_connection_handler, args) != 0)
        {
            fprintf(stderr, "Can't create a new thread, error %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        pthread_detach(thread);

        // we can't just reset fields: we need a new buffer for client_addr!
        client_addr = calloc(1, sizeof(struct sockaddr_in));
    }
}
