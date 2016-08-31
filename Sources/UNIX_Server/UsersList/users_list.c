/* ===========================================================================
*  users_list.c
* ============================================================================

*  Authors:         (c) 2016 Sergio Pedri and Andrea Salvati */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* close() */
#include <errno.h>
#include <sys/sem.h>

#include "../sem_util.h"
#include "users_list.h"

#define EMPTY TRUE

// Semaphore operation
struct sembuf sop = { 0 };

/* ============================================================================
*  List internal types
*  ========================================================================= */

// Single list node
struct listElem
{
    char* name;
    guid_t guid;
    guid_t partner;
    bool_t available;
    int socket;
    bool_t empty;
};

typedef struct listElem* entry_t;

// Head node of the list
struct listBase
{
    int semid;
    int length;
    entry_t list[MAX_USERS];
};

/* ============================================================================
*  Generic functions
*  ========================================================================= */

// A macro that just returns checks if the list is empty
#define IS_EMPTY(target) target->length == 0

// CreateList
list_t create_list(int semid)
{
    int i;
    list_t out_list = (list_t)malloc(sizeof(struct listBase));
    if (out_list == NULL)
    {
        fprintf(stderr, "Malloc failed\n");
        exit(EXIT_FAILURE);
    }
    out_list->semid = semid;
    out_list->length = 0;

    // Allocate all the nodes in the list and sets all to empty
    for (i = 0; i < MAX_USERS; i++)
    {
        entry_t node = (entry_t)malloc(sizeof(struct listElem));
        if (node == NULL)
        {
            fprintf(stderr, "Malloc failed\n");
            exit(EXIT_FAILURE);
        }
        memset(node, 0, sizeof(struct listElem));
        node->empty = EMPTY;
        (out_list->list[i]) = node;
    }
    return out_list;
}

// DestroyList
void destroy_list(list_t list)
{
    int ret, i, semid = list->semid;

    SEM_LOCK(sop, semid);

    if (IS_EMPTY(list))
    {
        SEM_RELEASE(sop, semid);
        return;
    }

    // Deallocate all the nodes in the list
    for (i = 0; i < MAX_USERS; i++)
    {
        entry_t temp = list->list[i];

        if (temp->empty != EMPTY)
        {
            // Deallocate the content of the entry
            free(temp->name);
            free(temp->guid);
            while (TRUE)
            {
                ret = close(temp->socket);
                if (ret == -1 && errno == EINTR) continue;
                else if (ret == -1)
                {
                    SEM_RELEASE(sop, semid);
                    fprintf(stderr, "Error closing the semaphore %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                else break;
            }
        }
        free(temp);
    }

    // Finally free the list header
    free(list);
    SEM_RELEASE(sop, semid);
}

// Get length
int get_list_length(list_t list)
{
    SEM_LOCK(sop, list->semid);
    int l = list->length;
    SEM_RELEASE(sop, list->semid);
    return l;
}

// Add
bool_t add(list_t list, string_t name, guid_t guid, int socket)
{
    int semid = list->semid, i;
    SEM_LOCK(sop, semid);

    // Check if maximum number of users has been reached
    if (list->length == MAX_USERS)
    {
        SEM_RELEASE(sop, semid);
        return FALSE;
    }

    // Allocate the new node and set its info
    entry_t node = (entry_t)malloc(sizeof(struct listElem));
    if (node == NULL)
    {
        fprintf(stderr, "Malloc failed\n");
        exit(EXIT_FAILURE);
    }
    node->name = name;
    node->socket = socket;
    node->guid = guid;
    node->available = FALSE;
    node->partner = NULL;
    node->empty = FALSE;

    // Insert the node in the first available spot
    for (i = 0; i < MAX_USERS; i++)
    {
        entry_t entry = list->list[i];
        if (entry->empty)
        {
            // Place the new node in the empty spot and free the old node
            (list->list[i]) = node;
            free(entry);
            break;
        }
    }

    // Increase the number of elements in the list and release the semaphore
    list->length = list->length + 1;
    SEM_RELEASE(sop, semid);

    return TRUE;
}

// Remove GUID
bool_t remove_guid(list_t list, guid_t guid)
{
    int semid = list->semid, i;
    SEM_LOCK(sop, semid);

    // Check if the list is empty
    if (IS_EMPTY(list))
    {
        SEM_RELEASE(sop, semid);
        return FALSE;
    }

    // Iterates on all the spots and check for the node with the given guid
    for (i = 0; i < MAX_USERS; i++)
    {
        entry_t node = list->list[i];

        // If the GUID matches, remove the item from the list
        if (!node->empty && guid_equals(node->guid, guid))
        {
            // Deallocate the content of the item
            free(node->name);
            free(node->guid);
            while (TRUE)
            {
                int ret = close(node->socket);
                if (ret == -1 && errno == EINTR) continue;
                else if (ret == -1)
                {
                    SEM_RELEASE(sop, semid);
                    exit(EXIT_FAILURE);
                }
                else break;
            }

            // Update the list length and set the node to empty
            memset(node, 0, sizeof(struct listElem));
            node->empty = EMPTY;
            list->length = list->length - 1;
            SEM_RELEASE(sop, semid);
            return TRUE;
        }
    }

    // GUID not found, just return false
    SEM_RELEASE(sop, semid);
    return FALSE;
}

// Get node
static entry_t get_node(list_t list, guid_t guid)
{
    int semid = list->semid, i;
    SEM_LOCK(sop, semid);

    if (IS_EMPTY(list))
    {
        SEM_RELEASE(sop, semid);
        return NULL;
    }

    // Iterate the target list
    for (i = 0; i < MAX_USERS; i++)
    {
        entry_t pointer = list->list[i];

        // If the GUIDs match, return the current node
        if (!pointer->empty && guid_equals(pointer->guid, guid))
        {
            SEM_RELEASE(sop, semid);
            return pointer;
        }
    }

    // Search unsuccessful, return null
    SEM_RELEASE(sop, semid);
    return NULL;
}

// Set available flag
bool_t set_available_flag(const list_t list, guid_t guid, bool_t target_value)
{
    // Try to get the right item inside the list
    entry_t targetNode = get_node(list, guid);

    // If the GUID isn't present, just return false
    if (targetNode == NULL) return FALSE;

    // Update the flag
    SEM_LOCK(sop, list->semid);
    targetNode->available = target_value;
    SEM_RELEASE(sop, list->semid);
    return TRUE;
}

// Set the chat partner of the user "guid" to "partner"
bool_t set_partner(list_t list, guid_t guid, guid_t partner)
{
    entry_t n = get_node(list, guid);
    if (n == NULL) return FALSE;
    SEM_LOCK(sop, list->semid);
    n->partner = partner;
    SEM_RELEASE(sop, list->semid);
    return TRUE;
}

// Get the available flag of the client described by guid
bool_t get_available_flag(const list_t list, guid_t guid)
{
    entry_t n = get_node(list, guid);
    if (n == NULL) return FALSE;
    SEM_LOCK(sop, list->semid);
    bool_t av = n->available;
    SEM_RELEASE(sop, list->semid);
    return av;
}

// Get the partner guid
guid_t get_partner(const list_t list, guid_t guid)
{
    entry_t n = get_node(list, guid);
    if (n == NULL) return NULL;
    SEM_LOCK(sop, list->semid);
    guid_t p = n->partner;
    SEM_RELEASE(sop, list->semid);
    return p;
}

// Get client connection socket
int get_socket(const list_t list, guid_t guid)
{
    // Try to get the target node
    entry_t pointer = get_node(list, guid);
    if (pointer == NULL) return -1;

    // If the GUID has been found, return the corresponding socket
    SEM_LOCK(sop, list->semid);
    int s = pointer->socket;
    SEM_RELEASE(sop, list->semid);
    return s;
}

// Get the name of the client with the given guid
char* get_name(const list_t list, guid_t guid)
{
    entry_t n = get_node(list, guid);
    if (n == NULL) return NULL;
    SEM_LOCK(sop, list->semid);
    string_t ret = n->name;
    SEM_RELEASE(sop, list->semid);
    return ret;
}

// Iterate
void users_list_iterate(const list_t list, void(*f)(guid_t))
{
    int semid = list->semid, i;
    SEM_LOCK(sop, semid);

    // Input check
    if (IS_EMPTY(list))
    {
        SEM_RELEASE(sop, semid);
        return;
    }

    // Iterate and call the input function on each node
    for (i = 0; i < MAX_USERS; i++)
    {
        entry_t e = list->list[i];
        if (!e->empty) f(e->guid);
    }
    SEM_RELEASE(sop, semid);
}

// SerializeList
string_t serialize_list(const list_t list)
{
    int semid = list->semid, i;

    // Input check
    SEM_LOCK(sop, semid);
    if (IS_EMPTY(list))
    {
        SEM_RELEASE(sop, semid);
        return create_empty_string();
    }
    SEM_RELEASE(sop, semid);

    // Initialize the buffer and the local variables
    string_t buffer = create_empty_string();
    int total_len = 0;

    // Start iterating through the list
    SEM_LOCK(sop, semid);
    for (i = 0; i < MAX_USERS; i++)
    {
        entry_t pointer = list->list[i];

        // Only the available users will be serialized
        if (pointer->empty || !(pointer->available)) continue;

        // Get the length of the user name and its string terminator
        int name_len = strlen(pointer->name) + 1;

        // Item length: name + serialized GUID (33) + separator/string terminator
        int instance_len = name_len + 35;
        total_len += instance_len;

        // Allocate the internal buffer
        char* internal_buffer = (char*)malloc(sizeof(char) * instance_len);
        if (internal_buffer == NULL)
        {
            SEM_RELEASE(sop, semid);
            fprintf(stderr, "Malloc cannot allocate more space\n");
            exit(EXIT_FAILURE);
        }

        // Clone the name and add the internal separator in the last position
        string_t edited_name = string_clone(pointer->name);
        edited_name[name_len - 1] = INTERNAL_SEPARATOR;

        // Add it to the internal buffer, then free the copy
        strncpy(internal_buffer, edited_name, name_len);
        free(edited_name);

        // Serialize the GUID and copy it inside the buffer
        char* serialized_guid = serialize_guid(pointer->guid);
        strncpy(internal_buffer + name_len, serialized_guid, 33);
        free(serialized_guid);

        // Add the current string to the buffer
        buffer = string_concat(buffer, internal_buffer, EXTERNAL_SEPARATOR);
    }
    buffer[total_len - 1] = STRING_TERMINATOR;
    SEM_RELEASE(sop, semid);
    return buffer;
}
