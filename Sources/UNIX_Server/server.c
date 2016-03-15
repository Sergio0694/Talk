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

#include "../Shared/guid.h"
#include "UsersList/users_list.h"
#include "../Shared/string_helper.h"
#include "sem_util.h"
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

    while (TRUE)
    {
        int ret = close(socketd);
        if (ret == -1 && errno == EINTR) continue;
        else break;
    }
    free(client_addr);
    free(args);
    pthread_exit(NULL);
}

// Performs a check on the return codes from recv_from_client function
static inline void check_recv_error(int ret, conn_thread_args_t* args)
{
    if (ret == TIME_OUT_EXPIRED) close_and_cleanup(args, "The client timed out");
    else if (ret == CLIENT_UNEXPECTED_CLOSE)
        close_and_cleanup(args, "Unexpected close from client");
    else if (ret == UNEXPECTED_ERROR) close_and_cleanup(args, "Cannot read from socket");
}

// Performs a check on the return codes from send_to_client function
static inline void check_send_error(int ret, conn_thread_args_t* args)
{
    if (ret < 0)
    {
        if (errno == EPIPE) close_and_cleanup(args, "EPIPE error received");
        close_and_cleanup(args, "Cannot send the message");
    }
}

static void name_pickup(conn_thread_args_t* args, char** name)
{
    int ret, name_len;
    char buf[1024];
    size_t buf_len = sizeof(buf);

    int socketd = args->sock_desc;

    printf("Waiting for name recv\n");
    name_len = recv_from_client(socketd, buf, buf_len);
    check_recv_error(name_len, args);
    while (name_len == 0)
    {
        sprintf(buf, "0Please choose a non-empty name: ");
        ret = send_to_client(socketd, buf);
        check_send_error(ret, args);
        name_len = recv_from_client(socketd, buf, buf_len);
        check_recv_error(name_len, args);
    }
    while (!name_validation(buf, name_len))
    {
        sprintf(buf, "0Please choose a name that does not contains | or ~ character: ");
        ret = send_to_client(socketd, buf);
        check_send_error(ret, args);
        name_len = recv_from_client(socketd, buf, buf_len);
        check_recv_error(name_len, args);
    }
    printf("Correct name received ");
    *name = (char*)malloc(sizeof(char) * (name_len));
    if (*name == NULL)
    {
        fprintf(stderr, "Malloc cannot allocate more space\n");
        exit(EXIT_FAILURE);
    }
    strncpy(*name, buf, name_len);
    printf("-- The name is %s\n", *name);
}

int nonblocking_recv(int socket, char* buf, size_t buf_len, guid_t guid)
{
    int ret;
    int bytes_read = 0;

    while (bytes_read <= buf_len)
    {
        ret = recv(socket, buf + bytes_read, 1, MSG_DONTWAIT);
        if (ret == -1 && errno == EAGAIN)
        {
            // continously che for connection request by othe client
            if (get_connection_requested_flag(users_list, guid)) return CONNECTION_REQUESTED;
            continue;
        }
        // don't know if this makes sense
        if (ret == -1 && errno == EWOULDBLOCK) return TIME_OUT_EXPIRED;
        if (ret == 0) return CLIENT_UNEXPECTED_CLOSE;
        if (ret < 0) return UNEXPECTED_ERROR;

        if (buf[bytes_read] == '\n') break;

        // if there is no \n the message is truncated when is length is buf_len
        if (bytes_read == buf_len) break;
        bytes_read += ret;
    }

    buf[bytes_read++] = STRING_TERMINATOR;
    return bytes_read;
}

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
    int ret;
    string_t temp, name;
    suid_t chat_users[2];

    // chosen target information
    guid_t chosen_guid;
    int chosen_socket;

    // timeval struct for recv timeout
    struct timeval tv;

    // parse client IP address and port
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port);

    // Welcome message
    sprintf(buf, "Welcome to Talk\nPlease choose a name: ");
    printf("Sending the Welcome message -- The client should see: %s\n", buf);
    ret = send(socketd, buf, strlen(buf), 0);
    check_send_error(ret, args);
    printf("Welcome message sent on %d socket\n", socketd);

    // set a 2 minutes timeout for the socket operations
    set_timeval(&tv, 120, 0);
    ret = setsockopt(socketd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    ERROR_HELPER(ret, "Cannot set SO_RCVTIMEO option");

    // save the name
    name_pickup(args, &name);

    // send the generated guid to the client
    guid_t guid = new_guid();
    temp = serialize_guid(guid);
    snprintf(buf, buf_len, "1%s", temp);
    free(temp);
    printf("Sending the generated guid %s\n", buf + 1);
    ret = send_to_client(socketd, buf);
    check_send_error(ret, args);
    printf("Guid sent\n");

    // sem_num = 0 --> operate on semaphore 0
    // sem_op = -1 --> decrement the semaphore value, wait if is 0
    struct sembuf sop = { 0 };
    SEM_LOCK(sop, semid);
    // add the user to users_list
    add(users_list, name, guid, socketd);
    // make the users_list available
    SEM_RELEASE(sop, semid);

    // name is no longer useful, free it
    free(name);

    while (TRUE)
    {
        // lock the binary semaphore
        SEM_LOCK(sop, semid);

        // serialize the users list
        temp = serialize_list(users_list);

        // make the users_list available
        SEM_RELEASE(sop, semid);

        // send the serialized users list to the client
        snprintf(buf, buf_len, "%s", temp);
        free(temp);
        printf("Sending the users list..\n");
        ret = send_to_client(socketd, buf);
        if (ret < 0)
        {
            SEM_LOCK(sop, semid);
            remove_guid(users_list, guid);
            SEM_RELEASE(sop, semid);
        }
        check_send_error(ret, args);
        printf("Users list sent\n");

        // wait for the client connection choice
        ret = nonblocking_recv(socketd, buf, buf_len, guid);
        if (ret < 0 && ret != CONNECTION_REQUESTED)
        {
            // remove the user from the users list
            SEM_LOCK(sop, semid);
            remove_guid(users_list, guid);
            SEM_RELEASE(sop, semid);
        }
        check_recv_error(ret, args);
        if (ret == CONNECTION_REQUESTED)
        {
            // get the partner socket
            SEM_LOCK(sop, semid);
            chosen_guid = get_partner(users_list, guid);
            chosen_socket = get_socket(users_list, chosen_guid);
            SEM_RELEASE(sop, semid);
        }

        // if the message received is DISCONNECT the client will be disconnected from the server
        if (strcmp(buf, "DISCONNECT") == 0)
        {
            // remove the client from the users list
            SEM_LOCK(sop, semid);
            remove_guid(users_list, guid);
            SEM_RELEASE(sop, semid);

            close_and_cleanup(args, "The client want to be disconnected");
        }

        // if ret != CONNECTION_REQUESTED the client has performed a choose
        if (ret != CONNECTION_REQUESTED)
        {
            // take the guid of the chosen client and his connection socket
            chosen_guid = deserialize_guid(buf);
            chosen_socket = get_socket(users_list, chosen_guid);
        }

        // build the struct for the chat handler
        chat_users[0].socket = socketd;
        chat_users[0].guid = guid;
        chat_users[1].socket = chosen_socket;
        chat_users[1].guid = chosen_guid;
        chat_args_t chat_args;
        chat_args.suid[0] = chat_users[0];
        chat_args.suid[1] = chat_users[1];

        if (ret != CONNECTION_REQUESTED)
        {
            // lock the binary semaphore
            SEM_LOCK(sop, semid);

            // check for the availability of the desired user
            if (get_available_flag(users_list, chosen_guid))
            {
                // set the connection requested flag for the chosen client
                set_connection_flag(users_list, chosen_guid, TRUE);

                // set the available flag for the two users to false
                set_available_flag(users_list, guid, FALSE);
                set_available_flag(users_list, chosen_guid, FALSE);

                // set the partner for both users
                set_partner(users_list, guid, chosen_guid);
                set_partner(users_list, chosen_guid, guid);
            }
            else
            {
                // make the users_list available
                SEM_RELEASE(sop, semid);

                // the chosen user is not available for chat, reserialize the users_list updated
                continue;
            }

            // make the users_list available
            SEM_RELEASE(sop, semid);
        }

        // start the chat
        chat_handler(chat_args);

    } // end of while
}

int main()
{
    // aux var
    int ret;
    int socket_desc, client_desc;
    int sockaddr_len = sizeof(struct sockaddr_in);

    // server setup
    users_list = create_list();
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
        if (args == NULL)
        {
			fprintf(stderr, "Malloc cannot allocate more space\n");
			exit(EXIT_FAILURE);
		}
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
