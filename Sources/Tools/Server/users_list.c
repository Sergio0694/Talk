#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "users_list.h"

/* ============================================================================
*  List internal types
*  ========================================================================= */

// Single list node
struct listElem
{
	string_t name;
	guid_t guid;
	bool_t available;
	bool_t connection_requested;
	string_t ip;
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
void destroy(list_t* list)
{
	// Deallocate all the nodes in the list
	nodePointer pointer = (*list)->head;
	while (pointer != NULL)
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

// Get length
int get_length(list_t list)
{
	if (list == NULL) return -1;
	return list->length;
}

// Add
void add(list_t list, string_t name, guid_t guid, string_t ip)
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

// Get node
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

// Sets a custom flag in a given node
static bool_t set_custom_flag(const list_t list, guid_t guid, bool_t target_value, bool_t first_flag)
{
	// Input check
	if (IS_EMPTY(list)) return FALSE;

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
string_t get_ip(const list_t list, guid_t guid)
{
	// Input check
	if (IS_EMPTY(list)) return NULL;

	// Try to get the target node
	nodePointer pointer = get_node(list, guid);
	if (pointer == NULL) return NULL;

	// If the GUID has been found, return the corresponding IP address
	return pointer->ip;
}

// SerializeList
string_t serialize_list(const list_t list)
{
	// Input check
	if (IS_EMPTY(list)) return create_empty_string();

	// Initialize the buffer and the local variables
	string_t buffer = create_empty_string();
	int total_len = 0, position = 0;

	// Start iterating through the list
	nodePointer pointer = list->head;
	while (pointer != NULL)
	{
		// Get the length of the user name and its string terminator
		int name_len = strlen(pointer->name) + 1;

		// Item length: name + serialized GUID (33) + separator/string terminator
		int instance_len = name_len + 35;
		total_len += instance_len;

		// Allocate the internal buffer
		char* internal_buffer = (char*)malloc(sizeof(char) * instance_len);

		// Clone the name and add the internal separator in the last position
		string_t edited_name = string_clone(pointer->name);
		edited_name[name_len - 1] = INTERNAL_SEPARATOR;

		// Add it to the internal buffer, then free the copy
		strcpy(internal_buffer, edited_name);
		free(edited_name);

		// Serialize the GUID and copy it inside the buffer
		char* serialized_guid = serialize_guid(pointer->guid);
		strcpy(internal_buffer + name_len, serialized_guid);
		free(serialized_guid);

		// Add the current string to the buffer
		buffer = string_concat(buffer, internal_buffer, EXTERNAL_SEPARATOR);

		// Move to the next item inside the list
		pointer = pointer->next;
	}
	return buffer;
}