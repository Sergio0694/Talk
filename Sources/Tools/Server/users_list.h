/* ============================================================================
*  users_list.h
* ============================================================================

*  Authors:         (c) 2015 Sergio Pedri and Andrea Salvati */

#ifndef USERS_LIST_H
#define USERS_LIST_H

#include "..\Shared\guid.h"
#include "..\Shared\types.h"

// =================== Public types ====================
typedef listStruct* list_t;

// ==================== Functions ======================

/* =====================================================================
*  Create
*  =====================================================================
*  Description:
*    Creates a new, empty list */
list_t create();

/* =====================================================================
*  Destroy
*  =====================================================================
*  Description:
*    Deallocates a list and all its content
*  Parameters:
*    list ---> The list to destroy */
void destroy(&list_t list);

/* =====================================================================
*  Add
*  =====================================================================
*  Description:
*    Adds a new node with the given info in the last position
*  Parameters:
*    list ---> The list to edit 
*    name ---> The name of the new node 
*    guid ---> The GUID of the new node
*    ip ---> The IP of the new node */
void add(list_t list, char* name, guid_t guid, char* ip);

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
*  SetConnectionFlag
*  =====================================================================
*  Description:
*    Sets the "connection requested" flag of the item with the given GUID.
*    Returns TRUE if the operation was successful, 
*    FALSE if the item wasn't present
*  Parameters:
*    list ---> The input list
*    guid ---> The GUID of the item to find
*    target_value ---> The new value of the flag to set */
bool_t set_connection_flag(const list_t list, guid_t guid, bool_t target_value);

/* =====================================================================
*  GetIP
*  =====================================================================
*  Description:
*    Returns the IP address of the item with the given GUID.
*    If the GUID isn't found, the function just returns NULL.
*  Parameters:
*    list ---> The input list
*    guid ---> The GUID of the item to find */
char* get_ip(const list_t list, guid_t guid);

#endif