#ifndef QUEUE_H
#define QUEUE_H

// =================== Public types ====================
typedef struct queueBase* queue_t;

/* =====================================================================
*  Functions
*  =====================================================================
*  Description:
*    Functions used to create and manage a queue.
*  NOTE:
*    All of these functions work with side effect and have an O(1) cost */

/* ---------------------------------------------------------------------
*  Create
*  ---------------------------------------------------------------------
*  Description:
*    Creates an empty queue */
queue_t create_queue();

/* ---------------------------------------------------------------------
*  Enqueue
*  ---------------------------------------------------------------------
*  Description:
*    Adds a new item at the end of the target queue
*  Parameters:
*    target ---> The queue to edit
*    text ---> The content of the new item to add to the queue */
void enqueue(queue_t target, char* text);

/* ---------------------------------------------------------------------
*  Dequeue
*  ---------------------------------------------------------------------
*  Description:
*    Removes the first item from the target queue.
*    If the queue is empty, returns NULL
*  Parameters:
*    target ---> The queue to edit */
char* dequeue(queue_t target);

/* ---------------------------------------------------------------------
*  GetLength
*  ---------------------------------------------------------------------
*  Description:
*    Returns the length of the given queue
*  Parameters:
*    queue ---> The input queue to analyze */
size_t get_queue_length(queue_t queue);

/* ---------------------------------------------------------------------
*  Destroy
*  ---------------------------------------------------------------------
*  Description:
*    Deallocates a queue along with all its nodes and their content
*  Parameters:
*    queue ---> A pointer to the queue to destroy */
void destroy_queue(queue_t* queue);

#endif
