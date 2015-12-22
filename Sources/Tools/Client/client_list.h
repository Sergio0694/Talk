/* ============================================================================
*  client_list.h
* ============================================================================

*  Authors:         (c) 2015 Sergio Pedri and Andrea Salvati */

#ifndef CLIENT_LIST_H
#define CLIENT_LIST_H

#include "..\Shared\guid.h"
#include "..\Shared\types.h"
#include "..\Shared\string_helper.h"

// =================== Public types ====================
typedef struct clientList* client_list_t;

// ==================== Functions ======================

client_list_t deserialize_client_list(const string_t buffer, const guid_t guid);

void print_list(const client_list_t list, void(*function)(string_t));

#endif