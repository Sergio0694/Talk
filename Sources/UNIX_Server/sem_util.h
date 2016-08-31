/* ===========================================================================
*  sem_util.h
* ============================================================================

*  Authors:         (c) 2016 Sergio Pedri and Andrea Salvati */

#ifndef SEM_UTIL_H
#define SEM_UTIL_H

#include "server.h"

#define SEM_LOCK(sop, semid) do                                             \
        {                                                                   \
            sop.sem_op = -1;                                                \
            int result_in_semop = semop(semid, &sop, 1);                    \
            ERROR_HELPER(result_in_semop, "Cannot operate on semaphore");   \
        } while(0)

#define SEM_RELEASE(sop, semid) do                                          \
        {                                                                   \
            sop.sem_op = 1;                                                 \
            int result_in_semop2 = semop(semid, &sop, 1);                   \
            ERROR_HELPER(result_in_semop2, "Cannot operate on semaphore");  \
        } while(0)

#endif
