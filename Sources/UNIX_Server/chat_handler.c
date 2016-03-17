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
#include "../Shared/types.h"
#include "../Shared/Queue/Queue.h"

void chat_handler(chat_args_t args)
{
    // aux variables
    int ret;

    // get handler arguments
    suid_t client1 = args.suid[0];
    suid_t client2 = args.suid[1];

    // create the message queue
    queue_t queue = create_queue();
}
