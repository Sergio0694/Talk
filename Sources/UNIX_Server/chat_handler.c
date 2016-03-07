#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/sem.h>
#include <sys/ipc.h>

#include "chat_handler.h"
#include "server_util.h"
#include "sem_util.h"
#include "../Tools/Shared/types.h"
#include "../Tools/Shared/Queue/Queue.h"

/* ============= GLOBAL VARIABLES ============= */
typedef struct threads_args_s
{
    queue_t queue;
    int sock1;
    int sock2;
    int semid;
} threads_args_t;

#define THREAD_CLEAN(sock1, sock2, semid, args) do  \
        {                                           \
            close(src1);                            \
            close(src2);                            \
            semctl(semid, 0, IPC_RMID, NULL);       \
            free(args);                             \
            pthread_exit(NULL);                     \
        } while(0)
/* ============================================ */

void* recv_thread(void* arg)
{
    // aux variables
    int ret;
    struct timeval tv;
    char buf[1024];
    int buf_len = sizeof(buf);

    // get handler arguments
    threads_args_t* args = (threads_args_t*)arg;
    queue_t queue = args->queue;
    int src1 = args->sock1;
    int src2 = args->sock2;
    int semid = args->semid;

    // initialize the timeval structure
    set_timeval(&tv, 60, 0);

    while(TRUE)
    {
        // argument for the select
        fd_set readfd;

        // initializes the set to be empty
        FD_ZERO(&readfd);

        // fill the fd set
        FD_SET(src1, &readfd);
        FD_SET(src2, &readfd);

        // call the select function for receive from both sockets
        ret = select(2, &readfd, NULL, NULL, &tv);
        // ignore the interruption by signals
        if (ret == -1 && errno == EINTR) continue;
        else if (ret == -1)
        {
            fprintf(stderr, "Select fails\n");
            THREAD_CLEAN(src1, src2, semid, args);
        }
        // timed out the clients will be disconnected
        else if (ret == 0) THREAD_CLEAN(src1, src2, semid, args);

        // check if src1 is still in readfd set
        if (FD_ISSET(src1, &readfd))
        {
            // receive from client1 -- the timeout on socket will be ignored
            ret = recv_from_client(src1, buf, buf_len);
            if (ret == -1) THREAD_CLEAN(src1, src2, semid, args);
        }

        // check if src2 is still in readfd set
        if (FD_ISSET(src2, &readfd))
        {
            // receive from client2 -- the timeout on socket will be ignored
            ret = recv_from_client(src1, buf, buf_len);
            if (ret == -1) THREAD_CLEAN(src1, src2, semid, args);
        }

        // add the received message at the message queue -- lock semaphore 0 and release semaphore 1
        struct sembuf sop = { 0 };
        SEM_LOCK(sop, semid);
        enqueue(queue);
        sop.sem_num = 1;
        SEM_RELEASE(sop, semid);
    }
}

void* send_thread(void* arg)
{
    // get handler arguments
    threads_args_t* args = (threads_args_t*)arg;
    queue_t queue = args->queue;
    int src1 = args->sock1;
    int src2 = args->sock2;
    int semid = args->semid;

}

void chat_handler(chat_args_t args)
{
    // aux variables
    int ret;
    pthread_t t1, t2;

    // get handler arguments
    suid_t client1 = args.suid[0];
    suid_t client2 = args.suid[1];

    // create the message queue
    queue_t queue = create();

    // create the semaphore
    int semid = semget(IPC_PRIVATE, /* semnum = */ 2, IPC_CREAT | 0660);
    ERROR_HELPER(semid, "Error in semaphore creation");

    // initialize the semaphore to 1
    union semun arg; // TODO: declare explicitly union semun
    arg.val = 1;
    ret = semctl(semid, 0, SETVAL, arg);
    ERROR_HELPER(ret, "Cannot initialize the semaphore");
    arg.val = 0;
    ret = semctl(semid, 1, SETVAL, arg);
    ERROR_HELPER(ret, "Cannot initialize the semaphore");

    // declare and initialize the struct argument for the threads
    threads_args_t targs = { queue, client1, client2, semid };

    // create the threads
    ret = pthread_create(&t1, NULL, recv_thread, &targs);
    if (ret != 0)
    {
        fprintf(stderr, "Can't create a new thread, error %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    ret = pthread_create(&t2, NULL, send_thread, &targs);
    if (ret != 0)
    {
        fprintf(stderr, "Can't create a new thread, error %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // synchronize the threads
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
}
