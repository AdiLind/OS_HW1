#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <stdatomic.h>
#include "cond_var.h"

#define QUEUE_SIZE 999999

typedef struct {
    int data[QUEUE_SIZE];
    atomic_int head;      
    atomic_int tail;      
    atomic_int count;     
    ticket_lock lock;     // Lock for the queue operations
    condition_variable not_empty;  // Condition var for signaling
} queue_t;

// Queue functions
void queue_init(queue_t* queue);
bool is_queue_is_empty(queue_t* queue);
void enqueue(queue_t* queue, int item);
int dequeue(queue_t* queue, bool* producers_done, atomic_bool* stop_flag);
void queue_signal_not_empty(queue_t* queue);
void queue_broadcast_not_empty(queue_t* queue);
static void acquire_lock(ticket_lock* lock);
static void release_lock(ticket_lock* lock);

#endif // QUEUE_H