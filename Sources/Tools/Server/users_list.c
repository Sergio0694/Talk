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

