#include "queue.h"
#include <sched.h>
#include <stdatomic.h>

// static functions 
static void acquire_lock(ticket_lock* lock) {
    int my_ticket = atomic_fetch_add(&lock->ticket, 1);
    while (atomic_load(&lock->current_ticket) != my_ticket) {
        sched_yield();
    }
}

static void release_lock(ticket_lock* lock) {
    atomic_fetch_add(&lock->current_ticket, 1);
}


void queue_init(queue_t* queue) {
    atomic_init(&queue->head, 0);
    atomic_init(&queue->tail, 0);
    atomic_init(&queue->count, 0);
    
    atomic_init(&queue->lock.ticket, 0);
    atomic_init(&queue->lock.current_ticket, 0);
    condition_variable_init(&queue->not_empty);
}

bool is_queue_is_empty(queue_t* queue) {
    return atomic_load(&queue->count) == 0;
}

void enqueue(queue_t* queue, int item) {
    acquire_lock(&queue->lock);

    int tail = atomic_load(&queue->tail);
    queue->data[tail] = item;
    atomic_store(&queue->tail, (tail + 1) % QUEUE_SIZE);
    atomic_fetch_add(&queue->count, 1);
    
    // Signal that queue is not empty
    condition_variable_signal(&queue->not_empty);
    release_lock(&queue->lock);
}

int dequeue(queue_t* queue, bool* producers_done, atomic_bool* stop_flag) {
    acquire_lock(&queue->lock);
    
    while (is_queue_is_empty(queue)) {
        // if queue is empty and no more producers, we're done
        if (*producers_done) {
            release_lock(&queue->lock);
            return -1;  //no more items
        }
        
        // Wait for items to be added to the queue
        condition_variable_wait(&queue->not_empty, &queue->lock);
        
        // Check if we should stop
        if (atomic_load(stop_flag)) {
            atomic_fetch_add(&queue->lock.current_ticket, 1);
            return -1;
        }
    }
    
    // Get data and delete (dequeue)
    int head = atomic_load(&queue->head);
    int item = queue->data[head];
    atomic_store(&queue->head, (head + 1) % QUEUE_SIZE);
    atomic_fetch_sub(&queue->count, 1);
    
    release_lock(&queue->lock);
    
    return item;
}

// helper functions
void queue_signal_not_empty(queue_t* queue) {
    condition_variable_signal(&queue->not_empty);
}

void queue_broadcast_not_empty(queue_t* queue) {
    condition_variable_broadcast(&queue->not_empty);
}