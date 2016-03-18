#ifndef SEM_UTIL_H
#define SEM_UTIL_H

#include "server.h"

#define SEM_LOCK(sop, semid) do                                 \
        {                                                       \
            sop.sem_op = -1;                                    \
            ret = semop(semid, &sop, 1);                        \
            ERROR_HELPER(ret, "Cannot operate on semaphore");   \
        } while(0)

#define SEM_RELEASE(sop, semid) do                              \
        {                                                       \
            sop.sem_op = 1;                                     \
            ret = semop(semid, &sop, 1);                        \
            ERROR_HELPER(ret, "Cannot operate on semaphore");   \
        } while(0)

#endif
