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
	struct listElem* next;
};

/* ============================================================================
*  Generic functions
*  ========================================================================= */

// Adds a single item to the target list
static void add(client_list_t list, char* name, guid_t guid)
{
	// Allocate the new node and set its info
	client_list_t node = (client_list_t)malloc(sizeof(struct clientList));
	node->name = name;
	node->guid = guid;
	node->next = NULL;

	// Add as the last element in the list
	
}

// Create
client_list_t deserialize_client_list(const string_t buffer, const guid_t guid)
{
	// Input check
	int len = strlen(buffer);
	if (buffer == NULL || len == 0) return NULL;

	// Allocate the return list
	client_list_t outList = (client_list_t)malloc(sizeof(struct clientList));

	printf("\n\nINPUT:\n%s\n\nDESERIALIZED:\n", buffer);

	string_t current_name;
	guid_t current_guid;
	bool_t tuple_found = FALSE;
	int index = 0, target = len + 1;
	for (int i = 0; i < target; i++)
	{
		if (buffer[i] == INTERNAL_SEPARATOR)
		{
			buffer[i] = STRING_TERMINATOR;
			current_name = string_clone(buffer + index);
			index = i + 1;
		}
		else if (buffer[i] == EXTERNAL_SEPARATOR || buffer[i] == STRING_TERMINATOR)
		{
			buffer[i] = STRING_TERMINATOR;
			current_guid = deserialize_guid(buffer + index);
			index = i + 1;
			tuple_found = TRUE;
		}
		if (tuple_found)
		{
			printf("\n>> %s ---> ", current_name);
			print_guid(current_guid);
			if (guid_equals(guid, current_guid)) printf(" MATCH");
			tuple_found = FALSE;
		}
	}
	//outList->head = NULL;
	return outList;
}

void print_list(const client_list_t list, void(*function)(string_t))
{

}