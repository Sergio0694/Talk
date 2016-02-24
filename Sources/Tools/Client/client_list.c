#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client_list.h"

/* ============================================================================
*  List internal types
*  ========================================================================= */

// Single list node
struct clientList
{
	string_t name;
	guid_t guid;
	struct clientList* next;
};

// Type declarations for the list node and a node pointer
typedef struct clientList clientListNode;
typedef clientListNode* clientNodePointer;

// List head
struct clientListBase
{
	clientNodePointer head;
};

/* ============================================================================
*  Generic functions
*  ========================================================================= */

// Adds a single item to the target list
static void add(client_list_t list, char* name, guid_t guid)
{
	// Allocate the new node and set its info
	clientNodePointer node = (clientNodePointer)malloc(sizeof(clientListNode));
	node->name = name;
	node->guid = guid;
	node->next = NULL;

	// Add as the last element in the list
	if (list->head == NULL) list->head = node;
	else
	{
		// If the list contains at least one item, add as the last node
		clientNodePointer pointer = list->head;
		while (pointer->next != NULL) pointer = pointer->next;
		pointer->next = node;
	}
}

// Create
client_list_t deserialize_client_list(const string_t buffer, const guid_t guid)
{
	// Input check
	int len = strlen(buffer);
	if (buffer == NULL || len == 0) return NULL;

	// Allocate the return list
	client_list_t outList = (client_list_t)malloc(sizeof(struct clientListBase));
	outList->head = NULL;

	// Local variables to store the temp values of the deserialized items
	string_t current_name;
	guid_t current_guid;
	bool_t tuple_found = FALSE;

	// Current position in the input buffer and total buffer length
	int index = 0, target = len + 1;
	for (int i = 0; i < target; i++)
	{
		// When the internal separator is reached, get the current name
		if (buffer[i] == INTERNAL_SEPARATOR)
		{
			buffer[i] = STRING_TERMINATOR;
			current_name = string_clone(buffer + index);
			index = i + 1;
		}
		else if (buffer[i] == EXTERNAL_SEPARATOR || buffer[i] == STRING_TERMINATOR)
		{
			// Second separator or final \0: deserialize the current GUID
			buffer[i] = STRING_TERMINATOR;
			current_guid = deserialize_guid(buffer + index);
			index = i + 1;
			tuple_found = TRUE;
		}

		// Store the current values every time a name and a GUID get deserialized
		if (tuple_found)
		{
			// Skip the input GUID and add all the other ones
			if (!guid_equals(guid, current_guid)) add(outList, current_name, current_guid);
			tuple_found = FALSE;
		}
	}
	return outList;
}

// Print list
void print_list(const client_list_t list, void(*function)(string_t))
{
	// Check if the list is empty
	if (list == NULL || list->head == NULL) return;

	// Iterate over every item and call the function to print its content
	clientNodePointer pointer = list->head;
	while (pointer != NULL)
	{
		function(pointer->name);
		pointer = pointer->next;
	}
}