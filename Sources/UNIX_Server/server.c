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

/* MENU MACROS */
#define REFRESH_MESSAGE "R"
#define WAIT_FOR_CONNECTIONS "W"
#define CONNECT_WITH_ANOTHER_USER "C"
#define DISCONNECT_MESSAGE "DISCONNECT"

/* ===== GLOBAL VARIABLES ===== */
typedef struct thread_args_s
{
    int sock_desc;;
} conn_thread_args_t;

list_t users_list = NULL;
int semid;

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
static void close_and_cleanup(conn_thread_args_t* args, guid_t guid, char* msg)
{
    int socketd = args->sock_desc;
    remove_guid(users_list, guid);
    fprintf(stderr, "%s\n", msg);
    free(args);
    pthread_exit(NULL);
}

// Performs a check on the return codes from recv_from_client function
static inline void check_recv_error(int ret, conn_thread_args_t* args, guid_t guid)
{
    if (ret == TIME_OUT_EXPIRED) close_and_cleanup(args, guid, "The client timed out");
    else if (ret == CLIENT_UNEXPECTED_CLOSE)
        close_and_cleanup(args, guid, "Unexpected close from client");
    else if (ret == UNEXPECTED_ERROR) close_and_cleanup(args, guid, "Cannot read from socket");
}

// Performs a check on the return codes from send_to_client function
static inline void check_send_error(int ret, conn_thread_args_t* args, guid_t guid)
{
    if (ret < 0)
    {
        if (errno == EPIPE) close_and_cleanup(args, guid, "EPIPE error received");
        close_and_cleanup(args, guid, "Cannot send the message");
    }
}

static void name_pickup(conn_thread_args_t* args, char** name, guid_t guid)
{
    int ret, name_len;
    char buf[BUFFER_LENGTH];
    size_t buf_len = sizeof(buf);

    int socketd = args->sock_desc;

    printf("Waiting for name recv\n");
    name_len = recv_from_client(socketd, buf, buf_len);
    check_recv_error(name_len, args, guid);
    while (name_len == 0)
    {
        sprintf(buf, "0Please choose a non-empty name: ");
        ret = send_to_client(socketd, buf);
        check_send_error(ret, args, guid);
        name_len = recv_from_client(socketd, buf, buf_len);
        check_recv_error(name_len, args, guid);
    }
    while (!name_validation(buf, name_len))
    {
        sprintf(buf, "0Please choose a name that does not contains | or ~ character: ");
        ret = send_to_client(socketd, buf);
        check_send_error(ret, args, guid);
        name_len = recv_from_client(socketd, buf, buf_len);
        check_recv_error(name_len, args, guid);
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

    // aux variables
    char buf[BUFFER_LENGTH];
    size_t buf_len = sizeof(buf);
    int ret;
    string_t temp, name;

    // chosen target information
    guid_t partner_guid;
    int partner_socket;

    // client representation on the server side
    guid_t guid = new_guid();

    // timeval struct for recv timeout
    struct timeval tv;

    // Welcome message
    sprintf(buf, "Welcome to Talk\nPlease choose a name: ");
    printf("DEBUG sending the Welcome message\n");
    ret = send(communication_socket, buf, strlen(buf), 0);
    check_send_error(ret, args, guid);
    printf("DEBUG: Welcome message sent on %d socket\n", communication_socket);

    // set a 2 minutes timeout for the socket operations
    set_timeval(&tv, 120, 0);
    ret = setsockopt(communication_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    if (ret < 0) close_and_cleanup(args, guid, "Cannot set SO_RCVTIMEO option");

    // save the name
    name_pickup(args, &name, guid);

    // send the generated guid to the client - append a 1 to the guid to communicate the success
    printf("DEBUG serialize the generated guid\n");
    temp = serialize_guid(guid);
    snprintf(buf, buf_len, "1%s", temp);
    printf("DEBUG: sending the generated guid %s\n", buf + 1);
    ret = send_to_client(communication_socket, buf);
    check_send_error(ret, args, guid);
    printf("DEBUG: guid sent\n");

    // cleanup
    strncpy(buf, "", buf_len);
    free(temp);

    // add the user to users_list
    printf("DEBUG adding the user to the users_list\n");
    add(users_list, name, guid, communication_socket);

    while (TRUE)
    {
        // receive the command from the client
        printf("DEBUG: Waiting for new commands from the user\n");
        ret = recv_from_client(communication_socket, buf, buf_len);
        check_recv_error(ret, args, guid);
        printf("DEBUG: Command %s received\n", buf);

        // the user command is to resend the user list
        if (strncmp(buf, REFRESH_MESSAGE, strlen(REFRESH_MESSAGE)) == 0)
        {
            printf("DEBUG received a refresh request\nDEBUG sending the refreshed list\n");
            temp = serialize_list(users_list);

            // send the serialized users list to the client
            snprintf(buf, buf_len, "%s", temp);
            free(temp);
            printf("Sending the users list..\n");
            ret = send_to_client(communication_socket, buf);
            check_send_error(ret, args, guid);
            printf("Users list sent\n");
            continue;
        }

        // if the message received is DISCONNECT the client will be disconnected from the server
        if (strncmp(buf, DISCONNECT_MESSAGE, strlen(DISCONNECT_MESSAGE)) == 0)
        {
            printf("DEBUG: received a disconnect message\n");
            close_and_cleanup(args, guid, "The client want to be disconnected");
        }

        if (strncmp(buf, WAIT_FOR_CONNECTIONS, strlen(WAIT_FOR_CONNECTIONS)) == 0)
        {
            // Make the user available for chat
            printf("DEBUG: received a wait message, setting the user as available\n");
            set_available_flag(users_list, guid, TRUE);
            struct sembuf sop = { 0 };
            sop.sem_num = 1;
            SEM_LOCK(sop, semid);
            partner_guid = get_partner(users_list, guid);
            partner_socket = get_socket(users_list, partner_guid);
        }

        if (strncmp(buf, CONNECT_WITH_ANOTHER_USER, strlen(CONNECT_WITH_ANOTHER_USER)) == 0)
        {
            // Receive the chosen guid from the client
            printf("DEBUG message connect received, waiting for the chosen guid to be received\n");
            ret = recv_from_client(communication_socket, buf, buf_len);
            check_recv_error(ret, args, guid);
            printf("DEBUG: guid received - %s\n", buf);


            // save the partner guid and check for his availability
            partner_guid = deserialize_guid(buf);
            printf("DEBUG guid deserialized check for availability\n");
            if (!get_available_flag(users_list, partner_guid))
            {
                strncpy(buf, "0", 2);
                ret = send(communication_socket, buf, strlen(buf), 0);
                check_send_error(ret, args, guid);
                continue;
            }

            // Retrieve the partner socket and save the partner info in the users list and the user
            // info in the partner's position inside the users list
            partner_socket = get_socket(users_list, partner_guid);
            if (!set_partner(users_list, guid, partner_guid))
            {
                close_and_cleanup(args, guid, "Cannot save the chat partner's info in the list");
            }
            if (!set_partner(users_list, partner_guid, guid))
            {
                close_and_cleanup(args, guid, "Cannot save the chat partner's info in the list");
            }

            // partner setted up
            struct sembuf sop = { 0 };
            sop.sem_num = 1;
            SEM_RELEASE(sop, semid);

            // send the user's name to the chosen user
            snprintf(buf, buf_len, "1%s", name);
            ret = send_to_client(partner_socket, buf);
            check_send_error(ret, args, guid);

            // send the partner name to the user
            printf("DEBUG: retrieving and sending the partner name\n");
            temp = get_name(users_list, partner_guid);
            printf("DEBUG: chosen partner info guid = %s, socket = %d, name = %s\n",
                    buf, partner_socket, temp);
            memset(buf, 0, buf_len);
            snprintf(buf, buf_len, "1%s", temp);
            ret = send_to_client(communication_socket, buf);
            check_send_error(ret, args, guid);
            printf("DEBUG partner name '%s' sent, time to start the chat session\n", buf);

            free(temp);
            memset(buf, 0, buf_len);
        }

        // start the chat
        ret = chat_handler(communication_socket, partner_socket);
        if (ret == TIME_OUT_EXPIRED)
        {
            close_and_cleanup(args, guid, "Chat timeout expired");
        }
        if (ret < 0)
        {
            close_and_cleanup(args, guid, "Unexpected error occurs");
        }

    } // end of while
}

// Handler for SIGINT and SIGHUP
void signal_handler(int signum)
{
    if (users_list != NULL)
    {
        printf("\nDEBUG Signal received, cleaning the users list\n");
        destroy_list(&users_list);
    }
    printf("DEBUG Removing the semaphore\n");
    int ret = semctl(semid, 0, IPC_RMID, NULL);
    ERROR_HELPER(ret, "Error while removing the semaphore");
    printf("Goodbye!\n");
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

    /* ==== semaphore creation and initialization ==== */

    // create the semaphore
    semid = semget(IPC_PRIVATE, /* semnum = */ 2, IPC_CREAT | 0660);
    ERROR_HELPER(semid, "Error in semaphore creation");

    // initialize the semaphore to 1 -- is used to synchronize the access to the users list
    union semun arg;
    arg.val = 1;
    ret = semctl(semid, 0, SETVAL, arg);
    ERROR_HELPER(ret, "Cannot initialize the semaphore");
    arg.val = 0;
    ret = semctl(semid, 1, SETVAL, arg);
    ERROR_HELPER(ret, "Cannot initialize the semaphore");

    /* =============================================== */

    // server setup
    users_list = create_list(semid);
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_HELPER(socket_desc, "Cannot open server socket");
    server_intial_setup(socket_desc);

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

        if (pthread_create(&thread, NULL, client_connection_handler, args) != 0)
        {
            fprintf(stderr, "Can't create a new thread, error %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        pthread_detach(thread);
    }
}
