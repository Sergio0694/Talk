#include <stdio.h>
#include <stdlib.h>
#include "users_list.h"
#include "..\Shared\guid.h"
#include "..\Shared\types.h"

/* ============================================================================
*  Queue internal types
*  ========================================================================= */

// Single list node
struct listElem
{
	char* name;
	guid_t guid;
	bool_t available;
	bool_t connection_requested;
	char* ip;
	struct listElem* next;
	struct listElem* previous;
};

// Type declarations for the list node and a node pointer
typedef struct listElem listNode;
typedef listNode* nodePointer;

/* ---------------------------------------------------------------------
*  ListBase
*  ---------------------------------------------------------------------
*  Description:
*    The head of a list: it stores a pointer to the first node, one
*    to the last node (so that add and remove operations have a O(1) cost)
*    and the current length of the list (this way getting the size has
*    a O(1) cost as well). */
struct listBase
{
	nodePointer head;
	nodePointer tail;
	size_t length;
};

// Type declaration for the base type
typedef struct listBase listStruct;

/* ============================================================================
*  Generic functions
*  ========================================================================= */

// Create
list_t create()
{
	list_t outList = (list_t)malloc(sizeof(listStruct));
	outList->head = NULL;
	outList->tail = NULL;
	outList->length = 0;
	return outList;
}

// Destroy
void destroy(&list_t list)
{
	// Deallocate all the nodes in the list
	nodePointer pointer = (*list)->head;
	while (pointer != null)
	{
		// Get a temp reference to the current node
		nodePointer temp = pointer;
		pointer = pointer->next;

		// Deallocate the content of the node and the node itself
		free(temp->name);
		free(temp->ip);
		free(temp);
	}

	// Finally free the list header
	free(list);
}

char* serialize(list_t list)
{
	if (IS_EMPTY(list)) return NULL;
	char* buffer = NULL;

	nodePointer pointer = list->head;
	while (pointer != NULL)
	{

	}
}

// Add
void add(list_t list, char* name, guid_t guid, char* ip)
{
	// Allocate the new node and set its info
	nodePointer node = (nodePointer)malloc(sizeof(listNode));
	node->name = name;
	node->ip = ip;
	node->guid = guid;
	node->available = TRUE;
	node->connection_requested = FALSE;
	node->next = NULL;

	// Add as the last element in the list
	if (list->head == NULL)
	{
		list->head = node;
		list->tail = node;
		node->previous = NULL;
	}
	else
	{
		node->previous = list->tail;
		list->tail->next = node;
		list->tail = node;
	}
	list->length = list->length + 1;
}

// A macro that just returns checks if the list is either null or empty
#define IS_EMPTY(target) target == NULL || target->length == 0

// Remove GUID
bool_t remove_guid(list_t list, guid_t guid)
{
	// Check if the list is empty
	if (IS_EMPTY(list)) return FALSE;

	// Create a temp iterator
	nodePointer pointer = list->head;
	while (pointer != NULL)
	{
		// If the GUID matches, remove the item from the list
		if (pointer->guid == guid)
		{
			// Deallocate the content of the item
			free(pointer->name);
			free(pointer->ip);

			// Adjust the references
			if (pointer->next == NULL) list->tail = NULL;
			else pointer->previous->next = pointer->next;

			// Update the list length and deallocate the removed item
			list->length = list->length - 1;
			free(pointer);
			return TRUE;
		}

		// Move to the next element in the list
		pointer = pointer->next;
	}

	// GUID not found, just return false
	return FALSE;
}

// Get node [helper function]
static nodePointer get_node(list_t list, guid_t guid);

static nodePointer get_node(list_t list, guid_t guid)
{
	// Iterate the target list
	nodePointer pointer = list->head;
	while (pointer != NULL)
	{
		// If the GUIDs match, return the current node
		if (pointer->guid == guid) return pointer;
		pointer = pointer->next;
	}

	// Search unsuccessful, return null
	return NULL;
}

// Sets a custom flag in a given node [helper function]
static bool_t set_custom_flag(const list_t list, guid_t guid, bool_t target_value, bool_t first_flag);

static bool_t set_custom_flag(const list_t list, guid_t guid, bool_t target_value, bool_t first_flag)
{
	// Input check
	VALID_LIST_CHECK(list);

	// Try to get the right item inside the list
	nodePointer targetNode = get_node(list, guid);

	// If the GUID isn't present, just return false
	if (targetNode == NULL) return FALSE;

	// Update the right flag
	if (first_flag == TRUE) targetNode->available = target_value;
	else targetNode->connection_requested = target_value;
	return TRUE;
}

// Set available flag
bool_t set_available_flag(const list_t list, guid_t guid, bool_t target_value)
{
	return set_custom_flag(list, guid, target_value, TRUE);
}

// Set connection requested flag
bool_t set_connection_flag(const list_t list, guid_t guid, bool_t target_value)
{
	return set_custom_flag(list, guid, target_value, FALSE);
}

// Get IP address
char* get_ip(const list_t list, guid_t guid)
{
	// Input check
	if (IS_EMPTY(list)) return NULL;

	// Try to get the target node
	nodePointer pointer = get_node(list, guid);
	if (pointer == NULL) return NULL;

	// If the GUID has been found, return the corresponding IP address
	return pointer->ip;
}