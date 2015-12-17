#include <stdlib.h>
#include "Queue.h"

/* ============================================================================
*  Queue internal types
*  ========================================================================= */

// Single queue node
struct queueElem
{
    char* info;
    struct queueElem* next;
};

// Type declarations for the queue node and a node pointer
typedef struct queueElem queueNode;
typedef queueNode* nodePointer;

/* ---------------------------------------------------------------------
*  queueBase
*  ---------------------------------------------------------------------
*  Description:
*    The head of a queue: it stores a pointer to the first node, one
*    to the last node (so that both queue and unqueue have a O(1) cost)
*    and the current length of the list (this way getting the size has
*    a O(1) cost as well). */
struct queueBase
{
    nodePointer head;
    nodePointer tail;
    size_t length;
};

// Type declaration for the base type
typedef struct queueBase queueStruct;
typedef queueStruct* queue;

/* ============================================================================
*  Generic functions
*  ========================================================================= */

// Create
queue_t create()
{
    queue outQueue = (queue)malloc(sizeof(queueStruct));
    outQueue->head = NULL;
    outQueue->tail = NULL;
    outQueue->length = 0;
    return outQueue;
}

// Enqueue
void enqueue(queue_t target, char* text)
{
    // Allocate the new node
    nodePointer pointer = (nodePointer)malloc(sizeof(queueNode));
    pointer->info = text;
    pointer->next = NULL;

    // Add the single item in the queue, if it is empty
    if (target->head == NULL)
    {
        target->head = pointer;
        target->tail = pointer;
    }
    else
    {
        // Otherwise add as the last item
        target->tail->next = pointer;
        target->tail = pointer;
    }

    // Update the length of the queue
    target->length = target->length + 1;
}

// Dequeue
char* dequeue(queue_t target)
{
    // If the queue is empty, just return NULL
    if (target->length == 0) return NULL;

    // Get a reference to the first item and its content
    nodePointer pointer = target->head;
    char* value = pointer->info;

    // If there is only an item in the queue, just clear it
    if (target->length == 1)
    {
        free(pointer);
        target->head = NULL;
        target->tail = NULL;
        target->length = 0;
    }
    else
    {
        // Otherwise just deallocate it and update the queue
        target->head = pointer->next;
        free(pointer);
        target->length = target->length - 1;
    }

    // Finally return the content of the removed element
    return value;
}

// GetLength
size_t get_length(queue_t queue) { return queue->length; }

// Destroy
void destroy(queue_t* queue)
{
    // Deallocate all the items left in the queue
    nodePointer pointer = (*queue)->head;
    while (pointer != NULL)
    {
        nodePointer temp = pointer;
        pointer = pointer->next;
        if (temp->info != NULL) free(temp->info);
        free(temp);
    }

    // Deallocate the queue structure itself
    free(queue);
}