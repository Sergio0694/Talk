#include <errno.h>
#include <string.h>     /* strerror() and strlen() */
#include <unistd.h>     /* close() */
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>   /* struct timeval */
#include <sys/types.h>  /* key_t, pthread_t */
#include <sys/ipc.h>    /* IPC_CREAT, IPC_EXCL, IPC_NOWAIT */
#include <sys/sem.h>
#include <sys/select.h>

#include "../Shared/guid.h"
#include "UsersList/users_list.h"
#include "../Shared/string_helper.h"
#include "sem_util.h"
#include "server_util.h"
#include "server.h"

#define REFRESH_MESSAGE "R"
#define DISCONNECT_MESSAGE "DISCONNECT"

/* ===== GLOBAL VARIABLES ===== */
typedef struct thread_args_s
{
    int sock_desc;
    int semid;
} conn_thread_args_t;

list_t users_list = NULL;

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

// Close everything before exiting the thread function
static void close_and_cleanup(conn_thread_args_t* args, char* msg)
{
    int socketd = args->sock_desc;

    fprintf(stderr, "%s\n", msg);

    while (TRUE)
    {
        int ret = close(socketd);
        if (ret == -1 && errno == EINTR) continue;
        else break;
    }
    free(args);
    pthread_exit(NULL);
}

// Performs a check on the return codes from recv_from_client function
static inline void check_recv_error(int ret, conn_thread_args_t* args, guid_t* guid)
{
    int semid = args->semid;
    struct sembuf sop = { 0 };

    if (ret == TIME_OUT_EXPIRED)
    {
        if (guid != NULL)
        {
            SEM_LOCK(sop, semid);
            remove_guid(users_list, *guid);
            SEM_RELEASE(sop, semid);
        }
        close_and_cleanup(args, "The client timed out");
    }
    else if (ret == CLIENT_UNEXPECTED_CLOSE)
    {
        if (guid != NULL)
        {
            SEM_LOCK(sop, semid);
            remove_guid(users_list, *guid);
            SEM_RELEASE(sop, semid);
        }
        close_and_cleanup(args, "Unexpected close from client");
    }
    else if (ret == UNEXPECTED_ERROR)
    {
        if (guid != NULL)
        {
            SEM_LOCK(sop, semid);
            remove_guid(users_list, *guid);
            SEM_RELEASE(sop, semid);
        }
        close_and_cleanup(args, "Cannot read from socket");
    }
}

// Performs a check on the return codes from send_to_client function
static inline void check_send_error(int ret, conn_thread_args_t* args, guid_t* guid)
{
    int semid = args->semid;
    struct sembuf sop = { 0 };

    if (ret < 0)
    {
        if (guid != NULL)
        {
            SEM_LOCK(sop, semid);
            remove_guid(users_list, *guid);
            SEM_RELEASE(sop, semid);
        }

        if (errno == EPIPE) close_and_cleanup(args, "EPIPE error received");
        close_and_cleanup(args, "Cannot send the message");
    }
}

static void name_pickup(conn_thread_args_t* args, char** name)
{
    int ret, name_len;
    char buf[BUFFER_LENGTH];
    size_t buf_len = sizeof(buf);

    int socketd = args->sock_desc;

    printf("Waiting for name recv\n");
    name_len = recv_from_client(socketd, buf, buf_len);
    check_recv_error(name_len, args, NULL);
    while (name_len == 0)
    {
        sprintf(buf, "0Please choose a non-empty name: ");
        ret = send_to_client(socketd, buf);
        check_send_error(ret, args, NULL);
        name_len = recv_from_client(socketd, buf, buf_len);
        check_recv_error(name_len, args, NULL);
    }
    while (!name_validation(buf, name_len))
    {
        sprintf(buf, "0Please choose a name that does not contains | or ~ character: ");
        ret = send_to_client(socketd, buf);
        check_send_error(ret, args, NULL);
        name_len = recv_from_client(socketd, buf, buf_len);
        check_recv_error(name_len, args, NULL);
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
    size_t bytes_read = 0;
    while (bytes_read <= buf_len)
    {
        ret = recv(socket, buf + bytes_read, 1, MSG_DONTWAIT);
        if (ret == -1 && errno == EAGAIN)
        {
            // continously check for connection request by other client
            if (get_connection_requested_flag(users_list, guid)) return CONNECTION_REQUESTED;
            continue;
        }
        // don't know if this makes sense
        if (ret == -1 && errno == EWOULDBLOCK) return TIME_OUT_EXPIRED;
        if (ret == 0) return CLIENT_UNEXPECTED_CLOSE;
        if (ret < 0) return UNEXPECTED_ERROR;
        printf("%c", buf[bytes_read]);
        if (buf[bytes_read] == '\n') break;

        // if there is no \n the message is truncated when is length is buf_len
        if (bytes_read == buf_len) break;
        bytes_read += ret;
    }
    buf[bytes_read++] = STRING_TERMINATOR;
    return bytes_read;
}

/* ============================= */

// Function executed by both the chooser and the chosen clients, src and dst represent the sockets
// by which the server receive and send the chat message from the two clients
int chat_handler(int src, int dst)
{
    // aux variables
    char buf[BUFFER_LENGTH], temp[BUFFER_LENGTH];
    int buf_len = sizeof(buf), ret;

    // timeval for the select timeout
    struct timeval tv;
    set_timeval(&tv, 120, 0);

    while (TRUE)
    {
        ret = recv_from_client(src, temp, buf_len);
        if (ret < 0) return ret;

        if (strncmp(temp, "QUIT", 4) == 0) break;

        // add to the received message a 0 to represent the source and
        // a 1 to represent the partner
        snprintf(buf, buf_len, "0%s", temp);
        ret = send_to_client(src, buf);
        if (ret < 0) return ret;
        snprintf(buf, buf_len, "1%s", temp);
        ret = send_to_client(dst, buf);
        if (ret < 0) return ret;
    }
    return 0;
}

// Function to handle the connection of the clients to the server
void* client_connection_handler(void* arg)
{
    // get handler arguments
    conn_thread_args_t* args = (conn_thread_args_t*)arg;
    int communication_socket = args->sock_desc;
    int semid = args->semid;

    // aux variables
    char buf[BUFFER_LENGTH];
    size_t buf_len = sizeof(buf);
    int ret;
    string_t temp, name;
    bool_t conn_req = FALSE;

    // chosen target information
    guid_t chosen_guid;
    int chosen_socket;

    // timeval struct for recv timeout
    struct timeval tv;

    // Welcome message
    sprintf(buf, "Welcome to Talk\nPlease choose a name: ");
    printf("DEBUG sending the Welcome message -- The client should see: %s\n", buf);
    ret = send(communication_socket, buf, strlen(buf), 0);
    check_send_error(ret, args, NULL);
    printf("DEBUG: Welcome message sent on %d socket\n", communication_socket);

    // set a 2 minutes timeout for the socket operations
    set_timeval(&tv, 120, 0);
    ret = setsockopt(communication_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    //ERROR_HELPER(ret, "Cannot set SO_RCVTIMEO option"); // TODO: change this
    if (ret < 0) close_and_cleanup(args, "Cannot set SO_RCVTIMEO option"); // CHANGED: to check

    // save the name
    name_pickup(args, &name);

    // send the generated guid to the client
    guid_t guid = new_guid();
    temp = serialize_guid(guid);
    snprintf(buf, buf_len, "1%s", temp);
    free(temp);
    printf("DEBUG: sending the generated guid %s\n", buf + 1);
    ret = send_to_client(communication_socket, buf);
    check_send_error(ret, args, NULL);
    printf("DEBUG: guid sent\n");

    // sem_num = 0 --> operate on semaphore 0
    // sem_op = -1 --> decrement the semaphore value, wait if is 0
    struct sembuf sop = { 0 };
    SEM_LOCK(sop, semid);
    // add the user to users_list
    add(users_list, name, guid, communication_socket, semid);
    // make the users_list available
    SEM_RELEASE(sop, semid);

    while (TRUE)
    {
        // serialize the users list, atomic operation
        SEM_LOCK(sop, semid);
        temp = serialize_list(users_list);
        SEM_RELEASE(sop, semid);

        // send the serialized users list to the client
        snprintf(buf, buf_len, "%s", temp);
        free(temp);
        printf("Sending the users list..\n");
        ret = send_to_client(communication_socket, buf);
        check_send_error(ret, args, &guid);
        printf("Users list sent\n");

        // wait for the client connection choice
        ret = nonblocking_recv(communication_socket, buf, buf_len, guid);
        check_recv_error(ret, args, &guid);

        // if ret == CONNECTION_REQUESTED you're choose from another client
        if (ret == CONNECTION_REQUESTED)
        {
            printf("DEBUG: I'm the chosen\n");
            // get the partner's name and his socket
            SEM_LOCK(sop, semid);
            chosen_guid = get_partner(users_list, guid);
            chosen_socket = get_socket(users_list, chosen_guid);
            char* ser_temp = serialize_guid(chosen_guid);
            printf("DEBUG partner guid: %s\n", ser_temp);
            printf("DEBUG my_socket %d partner socket: %d\n", communication_socket, chosen_socket);
            SEM_RELEASE(sop, semid);
            conn_req = TRUE;
        }

        if (strncmp(buf, REFRESH_MESSAGE, strlen(REFRESH_MESSAGE)) == 0)
        {
            printf("DEBUG received a refresh request\nDEBUG sending the refreshed list\n");
            continue;
        }

        // if the message received is DISCONNECT the client will be disconnected from the server
        if (strncmp(buf, DISCONNECT_MESSAGE, strlen(DISCONNECT_MESSAGE)) == 0)
        {
            // remove the client from the users list
            SEM_LOCK(sop, semid);
            remove_guid(users_list, guid);
            SEM_RELEASE(sop, semid);

            close_and_cleanup(args, "The client want to be disconnected");
        }

        // if !conn_req the client has performed a choose
        if (!conn_req)
        {
            // take the guid of the chosen client and his connection socket
            printf("DEBUG: I'm the one who choose\n");
            chosen_guid = deserialize_guid(buf);
            SEM_LOCK(sop, semid);
            chosen_socket = get_socket(users_list, chosen_guid);
            SEM_RELEASE(sop, semid);
            // DEBUG? char* ser_temp = serialize_guid(chosen_guid);
            printf("DEBUG partner guid: %s\n", buf);
            printf("DEBUG my_socket %d partner socket: %d\n", communication_socket, chosen_socket);
        }

        if (!conn_req)
        {
            // lock the binary semaphore
            SEM_LOCK(sop, semid);
            printf("DEBUG check for the availability of the user\n");
            // check for the availability of the desired user
            if (get_available_flag(users_list, chosen_guid))
            {
                // set the connection requested flag for the chosen client
                set_connection_flag(users_list, chosen_guid, TRUE);
                printf("DEBUG connection flag set\n");

                // set the available flag for the two users to false
                set_available_flag(users_list, guid, FALSE);
                set_available_flag(users_list, chosen_guid, FALSE);
                printf("DEBUG available flags set\n");

                // set the partner for both users
                set_partner(users_list, guid, chosen_guid);
                set_partner(users_list, chosen_guid, guid);
                printf("DEBUG partners set\n");
            }
            else
            {
                // make the users_list available
                SEM_RELEASE(sop, semid);

                // send a 0 to inform that the chosen user is not available
                buf[0] = '0';
                ret = send(communication_socket, (void*)&buf[0], 1, 0);
                check_send_error(ret, args, &guid);

                // the chosen user is not available for chat, reserialize the users_list updated
                continue;
            }

            // make the users_list available
            SEM_RELEASE(sop, semid);
        }

        SEM_LOCK(sop, semid);
        temp = get_name(users_list, chosen_guid);
        printf("DEBUG pick the partner's name: %s\n", temp);
        SEM_RELEASE(sop, semid);

        snprintf(buf, buf_len, "1%s", temp);

        int ret_temp = send_to_client(communication_socket, buf);
        check_send_error(ret_temp, args, &guid);
        //free(temp);
        printf("DEBUG name sent\nStarting a chat session..\n");

        // start the chat
        ret = chat_handler(communication_socket, chosen_socket);
        if (ret == TIME_OUT_EXPIRED)
        {
            fprintf(stderr, "Removing guid\n");
            /*SEM_LOCK(sop, semid);
            remove_guid(users_list, guid);
            SEM_RELEASE(sop, semid);*/
            close_and_cleanup(args, "Chat timeout expired");
        }
        if (ret < 0)
        {
            fprintf(stderr, "Removing guid\n");
            /*SEM_LOCK(sop, semid);
            remove_guid(users_list, guid);
            SEM_RELEASE(sop, semid);*/
            close_and_cleanup(args, "Unexpected error occurs");
        }

    } // end of while
}

// Handler for SIGINT and SIGHUP
void signal_handler(int signum)
{
    if (users_list != NULL)
    {
        fprintf(stderr, "\nSignal received, cleaning the users list\n");
        destroy_list(&users_list);
        fprintf(stderr, "Goodbye!\n");
    }
    exit(EXIT_SUCCESS);
}

// Error checked signal call
void add_signal(int signum, void (*f)(int))
{
    void (*ret)(int) = signal(signum, f);
    if (ret == SIG_ERR)
    {
        fprintf(stderr, "Error in signal function\n");
        exit(EXIT_FAILURE);
    }
}

void add_sigaction(int signum, struct sigaction act)
{
    int ret = sigaction(signum, &act, NULL);
    ERROR_HELPER(ret, "Error in sigaction");
}

int main()
{
    // aux var
    int ret;
    int socket_desc, client_desc;

    // server setup
    users_list = create_list();
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_HELPER(socket_desc, "Cannot open server socket");
    server_intial_setup(socket_desc);

    /* ==== semaphore creation and initialization ==== */

    // create the semaphore
    int semid = semget(IPC_PRIVATE, /* semnum = */ 1, IPC_CREAT | 0660);
    ERROR_HELPER(semid, "Error in semaphore creation");

    // initialize the semaphore to 1 -- is used to synchronize the access to the users list
    union semun arg;
    arg.val = 1;
    ret = semctl(semid, 0, SETVAL, arg);
    ERROR_HELPER(ret, "Cannot initialize the semaphore");

    /* =============================================== */

    // preventing the generation of SIGPIPE -- an EPIPE will be generated instead
    add_signal(SIGPIPE, SIG_IGN);

    // Ignore the sigchild event (should not happen anyway)
    add_signal(SIGCHLD, SIG_IGN);

    // prepare the arguments for the sigaction call
    sigset_t mask;
    sigfillset(&mask);
    struct sigaction act = { 0 };
    act.sa_handler = signal_handler;
    act.sa_mask = mask;

    // Handling all signals that matter with the same function
    add_sigaction(SIGHUP, act);
    add_sigaction(SIGINT, act);
    add_sigaction(SIGTERM, act);
    add_sigaction(SIGQUIT, act);

    while(TRUE)
    {
        printf("Waiting for connections ...\n");

        // accept incoming connection - don't care about the client address
        client_desc = accept(socket_desc, NULL, NULL);
        if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
        ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");

        printf("DEBUG connection accepted -- client socket is: %d\n", client_desc);

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

        if (pthread_create(&thread, NULL, client_connection_handler, args) != 0)
        {
            fprintf(stderr, "Can't create a new thread, error %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        pthread_detach(thread);
    }
}
