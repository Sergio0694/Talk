#ifndef CHAT_H
#define CHAT_H

#include "../Tools/Shared/guid.h"

typedef struct suid_s
{
	int socket;
	guid_t guid;
} suid_t;

typedef struct chat_args_s
{
	suid_t suid[2];
} chat_args_t;

void chat_handler(chat_args_t args);

#endif