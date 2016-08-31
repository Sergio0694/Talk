/* ============================================================================
*  users_list.h
* ============================================================================

*  Authors:         (c) 2016 Sergio Pedri and Andrea Salvati */

#ifndef USERS_LIST_H
#define USERS_LIST_H

#include "../../Shared/guid.h"
#include "../../Shared/types.h"
#include "../../Shared/string_helper.h"

#define MAX_USERS 10

// =================== Public types ====================

typedef struct listBase* list_t;

// ==================== Functions ======================

/* =====================================================================
*  CreateList
*  =====================================================================
*  Description:
*    Creates a new, empty list */
list_t create_list(int semid);

/* =====================================================================
*  DestroyList
*  =====================================================================
*  Description:
*    Deallocates a list and all its content
*  Parameters:
*    list ---> The list to destroy */
void destroy_list(list_t list);

/* =====================================================================
*  GetLength
*  =====================================================================
*  Description:
*    Returns the number of element in the input list */
int get_list_length(list_t list);

/* =====================================================================
*  Add
*  =====================================================================
*  Description:
*    Adds a new node with the given info in the first available spot
*  Parameters:
*    list ---> The list to edit
*    name ---> The name of the new node
*    guid ---> The GUID of the new node
*    socket ---> The socket of the client */
bool_t add(list_t list, string_t name, guid_t guid, int socket);

/* =====================================================================
*  RemoveGUID
*  =====================================================================
*  Description:
*    Removes the item with the given GUID from the list
*  Parameters:
*    list ---> The list to edit
*    guid ---> The GUID of the item to remove */
bool_t remove_guid(list_t list, guid_t guid);

/* =====================================================================
*  SetAvailableFlag
*  =====================================================================
*  Description:
*    Sets the "available" flag of the item with the given GUID. Returns
*    TRUE if the operation was successful, FALSE if the item wasn't present
*  Parameters:
*    list ---> The input list
*    guid ---> The GUID of the item to find
*    target_value ---> The new value of the flag to set */
bool_t set_available_flag(const list_t list, guid_t guid, bool_t target_value);

/* =====================================================================
*  SetPartner
*  =====================================================================
*  Description:
*    Sets the "partner" parameter of the item with the given GUID.
*    Returns TRUE if the operation was successful,
*    FALSE if the item wasn't present
*  Parameters:
*    list ---> The input list
*    guid ---> The GUID of the item to find
*    partner ---> The value of the partner guid */
bool_t set_partner(list_t list, guid_t guid, guid_t partner);

/* =====================================================================
*  GetAvailableFlag
*  =====================================================================
*  Description:
*    Get the "available" flag of the item with the given GUID. Returns
*    the boolean value of the flag
*  Parameters:
*    list ---> The input list
*    guid ---> The GUID of the item to find */
bool_t get_available_flag(const list_t list, guid_t guid);

/* =====================================================================
*  GetPartner
*  =====================================================================
*  Description:
*    Returns the guid of the partner of the user with the given GUID.
*  Parameters:
*    list ---> The input list
*    guid ---> The GUID of the item to find */
guid_t get_partner(list_t list, guid_t guid);

/* =====================================================================
*  GetSocket
*  =====================================================================
*  Description:
*    Returns the connection socket of the client with the given GUID.
*    If the GUID isn't found, the function just returns -1.
*  Parameters:
*    list ---> The input list
*    guid ---> The GUID of the item to find */
int get_socket(const list_t list, guid_t guid);

/* =====================================================================
*  GetName
*  =====================================================================
*  Description:
*    Returns the name of the client with the given GUID.
*    If the GUID isn't found, the function just returns NULL.
*  Parameters:
*    list ---> The input list
*    guid ---> The GUID of the item to find */
string_t get_name(const list_t list, guid_t guid);

/* =====================================================================
*  UsersListIterate
*  =====================================================================
*  Description:
*    Calls the input function for all the items inside the source list
*  Parameters:
*    list ---> The input list
*    f ---> The function to call for each node in the list */
void users_list_iterate(const list_t list, void(*f)(guid_t));

/* =====================================================================
*  SerializeList
*  =====================================================================
*  Description:
*    Serializes the given list into a string. The function only
*    takes into account the name and the GUID of each entry.
*  Parameters:
*    list ---> The list to serialize */
string_t serialize_list(const list_t list);

#endif
