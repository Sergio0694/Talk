#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* close() */
#include <errno.h>
#include <sys/sem.h>

#include "../sem_util.h"
#include "users_list.h"

// Semaphore operation
struct sembuf sop = { 0 };

/* ============================================================================
*  List internal types
*  ========================================================================= */

// Single list node
struct listElem
{
    string_t name;
    guid_t guid;
    guid_t partner;
    bool_t available;
    int socket;
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
    int semid;
    size_t length;
};

// Type declaration for the base type
typedef struct listBase listStruct;

/* ============================================================================
*  Generic functions
*  ========================================================================= */

// CreateList
list_t create_list(int semid)
{
    list_t outList = (list_t)malloc(sizeof(listStruct));
    if (outList == NULL)
    {
        fprintf(stderr, "Malloc cannot allocate more space\n");
        exit(EXIT_FAILURE);
    }
    outList->head = NULL;
    outList->tail = NULL;
    outList->length = 0;
    outList->semid = semid;
    return outList;
}

// DestroyList
void destroy_list(list_t* list)
{
    int ret;

    if (list == NULL) return;

    SEM_LOCK(sop, (*list)->semid);
    // Deallocate all the nodes in the list
    nodePointer pointer = (*list)->head;
    while (pointer != NULL)
    {
        // Get a temp reference to the current node
        nodePointer temp = pointer;
        pointer = pointer->next;

        // Deallocate the content of the node and the node itself
        printf("DEBUG cleaning the name\n");
        free(temp->name);
        printf("DEBUG cleaning the guid\n");
        free(temp->guid);
        //printf("DEBUG cleaning the partner guid\n");
        //free(temp->partner);
        while (TRUE)
        {
            printf("DEBUG closing the socket\n");
            ret = close(temp->socket);
            if (ret == -1 && errno == EINTR) continue;
            else if (ret == -1)
            {
                SEM_RELEASE(sop, (*list)->semid);
                fprintf(stderr, "Error closing the semaphore%s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            else break;
        }
    }

    // Finally free the list header
    SEM_RELEASE(sop, (*list)->semid);
    free((*list));
    printf("DEBUG SUCCESS!\n");
}

// Get length
int get_list_length(list_t list)
{
    if (list == NULL) return -1;
    SEM_LOCK(sop, list->semid);
    int l = list->length;
    SEM_RELEASE(sop, list->semid);
    return l;
}

// Add
bool_t add(list_t list, string_t name, guid_t guid, int socket)
{
    if (list == NULL) return FALSE;
    int semid = list->semid;

    SEM_LOCK(sop, semid);

    if (list->length == MAX_USERS)
    {
        SEM_RELEASE(sop, semid);
        return FALSE;
    }

    // Allocate the new node and set its info
    nodePointer node = (nodePointer)malloc(sizeof(listNode));
    if (node == NULL)
    {
        fprintf(stderr, "Malloc cannot allocate more space\n");
        exit(EXIT_FAILURE);
    }
    node->name = name;
    node->socket = socket;
    node->guid = guid;
    node->available = FALSE;
    node->next = NULL;
    node->partner = NULL;

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

    SEM_RELEASE(sop, semid);
    return TRUE;
}

// A macro that just returns checks if the list is either null or empty
#define IS_EMPTY(target) target->length == 0

// Remove GUID
bool_t remove_guid(list_t list, guid_t guid)
{
    if (list == NULL) return FALSE;
    int semid = list->semid;

    SEM_LOCK(sop, semid);

    // Check if the list is empty
    if (IS_EMPTY(list))
    {
        SEM_RELEASE(sop, semid);
        return FALSE;
    }

    // Create a temp iterator
    nodePointer pointer = list->head;
    while (pointer != NULL)
    {
        // If the GUID matches, remove the item from the list
        if (guid_equals(pointer->guid, guid))
        {
            // Deallocate the content of the item
            printf("DEBUG guid found in the users list, start freeing\n");
            free(pointer->name);
            printf("DEBUG name freed\n");
            free(pointer->guid);
            printf("DEBUG guid freed\n");
            //free(pointer->partner);
            while (TRUE)
            {
                int ret = close(pointer->socket);
                if (ret == -1 && errno == EINTR) continue;
                else if (ret == -1)
                {
                    SEM_RELEASE(sop, semid);
                    exit(EXIT_FAILURE);
                }
                else break;
            }
            printf("DEBUG socket closed\n");

            // Adjust the references
            if (pointer->next == NULL) list->tail = NULL;
            else
            {
                if (pointer->previous != NULL) (pointer->previous)->next = pointer->next;
                else (pointer->next)->previous = NULL;
            }
            printf("DEBUG references adjusted\n");

            // Update the list length and deallocate the removed item
            list->length = list->length - 1;
            free(pointer);
            printf("DEBUG node itself freed\n");
            SEM_RELEASE(sop, semid);
            return TRUE;
        }

        // Move to the next element in the list
        pointer = pointer->next;
    }

    // GUID not found, just return false
    SEM_RELEASE(sop, semid);
    return FALSE;
}

// Get node
static nodePointer get_node(list_t list, guid_t guid)
{
    if (list == NULL) return NULL;
    int semid = list->semid;

    SEM_LOCK(sop, semid);

    // Iterate the target list
    nodePointer pointer = list->head;
    while (pointer != NULL)
    {
        // If the GUIDs match, return the current node
        if (guid_equals(pointer->guid, guid))
        {
            SEM_RELEASE(sop, semid);
            return pointer;
        }
        pointer = pointer->next;
    }

    // Search unsuccessful, return null
    SEM_RELEASE(sop, semid);
    return NULL;
}

// Set available flag
bool_t set_available_flag(const list_t list, guid_t guid, bool_t target_value)
{
    // Input check
    if (list == NULL) return FALSE;
    if (IS_EMPTY(list)) return FALSE;

    // Try to get the right item inside the list
    nodePointer targetNode = get_node(list, guid);

    // If the GUID isn't present, just return false
    if (targetNode == NULL) return FALSE;

    // Update the flag
    SEM_LOCK(sop, list->semid);
    targetNode->available = target_value;
    SEM_RELEASE(sop, list->semid);
    return TRUE;
}

// Set the chat partner of the user "guid" at "partner"
bool_t set_partner(list_t list, guid_t guid, guid_t partner)
{
    nodePointer n = get_node(list, guid);
    if (n == NULL) return FALSE;
    SEM_LOCK(sop, list->semid);
    n->partner = partner;
    SEM_RELEASE(sop, list->semid);
    return TRUE;
}

// Get the available flag of the client described by guid
bool_t get_available_flag(const list_t list, guid_t guid)
{
    if (IS_EMPTY(list)) return FALSE;
    nodePointer n = get_node(list, guid);
    SEM_LOCK(sop, list->semid);
    bool_t av = n->available;
    SEM_RELEASE(sop, list->semid);
    return av;
}

// Get the partner guid
guid_t get_partner(const list_t list, guid_t guid)
{
    nodePointer n = get_node(list, guid);
    SEM_LOCK(sop, list->semid);
    guid_t p = n->partner;
    SEM_RELEASE(sop, list->semid);
    return p;
}

// Get client connection socket
int get_socket(const list_t list, guid_t guid)
{
    // Input check
    if (IS_EMPTY(list)) return -1;

    // Try to get the target node
    nodePointer pointer = get_node(list, guid);
    if (pointer == NULL) return -1;

    // If the GUID has been found, return the corresponding IP address
    SEM_LOCK(sop, list->semid);
    int s = pointer->socket;
    SEM_RELEASE(sop, list->semid);
    return s;
}

// Get the name of the client with the given guid
string_t get_name(const list_t list, guid_t guid)
{
    nodePointer n = get_node(list, guid);
    if (n == NULL) return NULL;
    SEM_LOCK(sop, list->semid);
    string_t ret = n->name;
    SEM_RELEASE(sop, list->semid);
    return ret;
}

// Iterate
void users_list_iterate(const list_t list, void(*f)(guid_t))
{
    // Input check
    if (list == NULL) return;
    if (IS_EMPTY(list)) return;

    // Iterate and call the input function on each node
    SEM_LOCK(sop, list->semid);
    nodePointer pointer = list->head;
    while (pointer != NULL)
    {
        f(pointer->guid);
        pointer = pointer->next;
    }
    SEM_RELEASE(sop, list->semid);
}

// SerializeList
string_t serialize_list(const list_t list)
{
    // Input check
    if (list == NULL) return create_empty_string();
    if (IS_EMPTY(list)) return create_empty_string();

    // Initialize the buffer and the local variables
    string_t buffer = create_empty_string();
    int total_len = 0;

    // Start iterating through the list
    SEM_LOCK(sop, list->semid);
    nodePointer pointer = list->head;
    while (pointer != NULL)
    {
        if (!(pointer->available))
        {
            pointer = pointer->next;
            continue;
        }

        printf("DEBUG name: %s\n", pointer->name);
        // Get the length of the user name and its string terminator
        int name_len = strlen(pointer->name) + 1;

        // Item length: name + serialized GUID (33) + separator/string terminator
        int instance_len = name_len + 35;
        total_len += instance_len;

        // Allocate the internal buffer
        char* internal_buffer = (char*)malloc(sizeof(char) * instance_len);
        if (internal_buffer == NULL)
        {
            SEM_RELEASE(sop, list->semid);
            fprintf(stderr, "Malloc cannot allocate more space\n");
            exit(EXIT_FAILURE);
        }

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
    buffer[total_len - 1] = STRING_TERMINATOR;
    SEM_RELEASE(sop, list->semid);
    return buffer;
}
