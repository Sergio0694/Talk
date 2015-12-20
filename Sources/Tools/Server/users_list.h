/* ============================================================================
*  users_list.h
* ============================================================================

*  Authors:         (c) 2015 Sergio Pedri and Andrea Salvati */

#ifndef USERS_LIST_H
#define USERS_LIST_H

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

#endif