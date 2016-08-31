/* ===========================================================================
*  server.c
* ============================================================================

*  Authors:         (c) 2016 Sergio Pedri and Andrea Salvati */

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

list_t users_list;
int semid, server_socket;

// Used in calls to semctl()
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
    fprintf(stderr, "%s\n", msg);
    remove_guid(users_list, guid);
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

// Checks the received name
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
// used by the server to receive and send chat messages between the two clients
int chat_handler(int src, int dst)
{
    // Aux variables
    char buf[BUFFER_LENGTH], temp[BUFFER_LENGTH];
    int buf_len = sizeof(buf), ret;

    // Timeval for the select timeout
    struct timeval tv;
    set_timeval(&tv, 120, 0);

    while (TRUE)
    {
        ret = recv_from_client(src, temp, buf_len);
        if (ret < 0) return ret;

        // Add to the received message a 0 to represent the source and
        // a 1 to represent the partner
        snprintf(buf, buf_len, "0%s", temp);
        ret = send_to_client(src, buf);
        if (ret < 0) return ret;
        snprintf(buf, buf_len, "1%s", temp);
        ret = send_to_client(dst, buf);
        if (ret < 0) return ret;

        if (strncmp(temp, "QUIT", 4) == 0) break;
    }
    return 0;
}

// Function to handle the connection of the clients to the server
void* client_connection_handler(void* arg)
{
    // Get handler arguments
    conn_thread_args_t* args = (conn_thread_args_t*)arg;
    int communication_socket = args->sock_desc;

    // Aux variables
    char buf[BUFFER_LENGTH];
    size_t buf_len = sizeof(buf);
    int ret;
    string_t temp, name;

    // Chosen target information
    guid_t partner_guid;
    int partner_socket;

    // Client representation on the server side
    struct sembuf sop = { 0 };
    sop.sem_num = 2;
    SEM_LOCK(sop, semid);
    guid_t guid = new_guid();
    SEM_RELEASE(sop, semid);

    // Timeval struct for recv timeout
    struct timeval tv;

    // Welcome message
    sprintf(buf, "Welcome to Talk\nPlease choose a name: ");
    ret = send(communication_socket, buf, strlen(buf), 0);
    check_send_error(ret, args, guid);

    // Set a 2 minutes timeout for the socket operations
    set_timeval(&tv, 120, 0);
    ret = setsockopt(communication_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    if (ret < 0) close_and_cleanup(args, guid, "Cannot set SO_RCVTIMEO option");

    // Save the name
    name_pickup(args, &name, guid);

    // Send the generated guid to the client - prepend a 1 to the guid to communicate the success
    temp = serialize_guid(guid);
    snprintf(buf, buf_len, "1%s", temp);
    ret = send_to_client(communication_socket, buf);
    check_send_error(ret, args, guid);

    // Cleanup
    free(temp);

    // Add the user to users_list
    bool_t added = add(users_list, name, guid, communication_socket);
    if (!added) close_and_cleanup(args, guid, "Cannot add the user to the users list");

    while (TRUE)
    {
        // Receive the command from the client
        ret = recv_from_client(communication_socket, buf, buf_len);
        check_recv_error(ret, args, guid);

        // The user command is to resend the users list
        if (strncmp(buf, REFRESH_MESSAGE, strlen(REFRESH_MESSAGE)) == 0)
        {
            temp = serialize_list(users_list);

            // Send the serialized users list to the client
            strncpy(buf, temp, buf_len);
            free(temp);
            printf("Sending the users list..\n");
            ret = send_to_client(communication_socket, buf);
            check_send_error(ret, args, guid);
            printf("Users list sent\n");
            continue;
        }

        // If the message received is DISCONNECT the client will be disconnected from the server
        if (strncmp(buf, DISCONNECT_MESSAGE, strlen(DISCONNECT_MESSAGE)) == 0)
        {
            close_and_cleanup(args, guid, "The client want to be disconnected");
        }

        if (strncmp(buf, WAIT_FOR_CONNECTIONS, strlen(WAIT_FOR_CONNECTIONS)) == 0)
        {
            // Make the user available for chat
            set_available_flag(users_list, guid, TRUE);

            // Wait for the partner to be set up by the chooser
            memset(&sop, 0, sizeof(struct sembuf));
            sop.sem_num = 1;
            SEM_LOCK(sop, semid);
            partner_guid = get_partner(users_list, guid);
            partner_socket = get_socket(users_list, partner_guid);
        }

        if (strncmp(buf, CONNECT_WITH_ANOTHER_USER, strlen(CONNECT_WITH_ANOTHER_USER)) == 0)
        {
            // Receive the chosen guid from the client
            ret = recv_from_client(communication_socket, buf, buf_len);
            check_recv_error(ret, args, guid);

            // Save the partner guid and check for his availability
            partner_guid = deserialize_guid(buf);
            if (!get_available_flag(users_list, partner_guid))
            {
                strncpy(buf, "0The user is not available at the moment, try a refresh", buf_len);
                ret = send_to_client(communication_socket, buf);
                check_send_error(ret, args, guid);
                continue;
            }

            // Retrieve the partner socket and save the partner info in the users list and the user
            // info in the partner's position inside the users list
            partner_socket = get_socket(users_list, partner_guid);
            if (!set_partner(users_list, guid, partner_guid) ||
                !set_partner(users_list, partner_guid, guid))
            {
                strncpy(buf, "0Problems with internal operation, try later", buf_len);
                ret = send_to_client(communication_socket, buf);
                check_send_error(ret, args, guid);
                close_and_cleanup(args, guid, "Cannot save the chat partner's info in the list");
            }

            // Partner set up
            memset(&sop, 0, sizeof(struct sembuf));
            sop.sem_num = 1;
            SEM_RELEASE(sop, semid);

            // Send the user's name to the chosen user
            strncpy(buf, "", buf_len);
            snprintf(buf, buf_len, "1%s", name);
            ret = send_to_client(partner_socket, buf);
            check_send_error(ret, args, guid);

            // Send the partner's name to the user
            temp = get_name(users_list, partner_guid);
            strncpy(buf, "", buf_len);
            snprintf(buf, buf_len, "1%s", temp);
            ret = send_to_client(communication_socket, buf);
            check_send_error(ret, args, guid);
        }

        // Before entering the chat make both users unavailable
        set_available_flag(users_list, guid, FALSE);
        set_available_flag(users_list, partner_guid, FALSE);

        // Start the chat
        ret = chat_handler(communication_socket, partner_socket);
        if (ret == TIME_OUT_EXPIRED) close_and_cleanup(args, guid, "Chat timeout expired");
        if (ret < 0) close_and_cleanup(args, guid, "Unexpected error occurs");

    } // End of while
}

// Handler for SIGINT, SIGHUP, SIGTERM and SIGQUIT
void signal_handler(int signum)
{
    // Destroy the users list
    destroy_list(users_list);

    // Remove the shared semaphore
    int ret = semctl(semid, 0, IPC_RMID, NULL);
    ERROR_HELPER(ret, "Error while removing the semaphore");

    // Close the server socket and exit the process
    while (TRUE)
    {
        ret = close(server_socket);
        if (ret == -1 && errno == EINTR) continue;
        else if (ret == -1) exit(EXIT_FAILURE);
        else break;
    }
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

void add_sigaction(int signum)
{
    // Prepare the arguments for the sigaction call
    sigset_t mask;
    sigfillset(&mask);
    struct sigaction act = { 0 };
    act.sa_handler = signal_handler;
    act.sa_mask = mask;

    int ret = sigaction(signum, &act, NULL);
    ERROR_HELPER(ret, "Error in sigaction");
}

int main()
{
    // Aux var
    int ret;
    int client_desc;

    /* ==== Semaphores creation and initialization ==== */

    // Create the semaphore set
    semid = semget(IPC_PRIVATE, /* semnum = */ 3, IPC_CREAT | 0660);
    ERROR_HELPER(semid, "Error in semaphore creation");

    // Initialize the semaphore 0 to 1 -- it is used to synchronize the access to the users list
    union semun arg;
    arg.val = 1;
    ret = semctl(semid, 0, SETVAL, arg);
    ERROR_HELPER(ret, "Cannot initialize the semaphore");

    // Initialize the semaphore 1 to 0 -- it is used to synchronize the partner's setting
    arg.val = 0;
    ret = semctl(semid, 1, SETVAL, arg);
    ERROR_HELPER(ret, "Cannot initialize the semaphore");

    // Initialize the semaphore 2 to 0 -- it is used to synchronize the guid creation
    arg.val = 1;
    ret = semctl(semid, 2, SETVAL, arg);
    ERROR_HELPER(ret, "Cannot initialize the semaphore");

    /* =============================================== */

    // Server setup
    users_list = create_list(semid);
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_HELPER(server_socket, "Cannot open server socket");
    server_intial_setup(server_socket);

    // Preventing the generation of SIGPIPE -- an EPIPE will be generated instead
    add_signal(SIGPIPE, SIG_IGN);

    // Ignore the sigchild event (should not happen anyway)
    add_signal(SIGCHLD, SIG_IGN);

    // Handling all signals that matter with the same function
    add_sigaction(SIGHUP);
    add_sigaction(SIGINT);
    add_sigaction(SIGTERM);
    add_sigaction(SIGQUIT);

    while(TRUE)
    {
        printf("Waiting for connections ...\n");

        // Accept incoming connection - don't care about the client address
        client_desc = accept(server_socket, NULL, NULL);
        if (client_desc == -1 && errno == EINTR) continue; // Check for interruption by signals
        ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");

        pthread_t thread;

        // Argument for thread
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
