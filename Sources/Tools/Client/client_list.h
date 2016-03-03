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
typedef struct clientListBase* client_list_t;

// ==================== Functions ======================

/* =====================================================================
*  TryGetGuid
*  =====================================================================
*  Description:
*    Tries to get a target guid from the input list
*  Parameters:
*    list ---> The source list
*    index ---> The index of the target guid to retrieve */
guid_t* try_get_guid(const client_list_t list, const int index);

/* =====================================================================
*  DeserializeClientList
*  =====================================================================
*  Description:
*    Deserializes a list from an input buffer
*  Parameters:
*    buffer ---> The buffer that contains the serialization data
*    guid ---> The GUID to skip when deserializing the buffer */
client_list_t deserialize_client_list(const string_t buffer, const guid_t guid);

/* =====================================================================
*  PrintList
*  =====================================================================
*  Description:
*    Prints all the items inside a target list
*  Parameters:
*    list ---> The list to print
*    function ---> The callback function to invoke to print each item */
void print_list(const client_list_t list, void(*function)(string_t));

#endif