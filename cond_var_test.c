#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include "cond_var.h"


void ticketlock_init(ticket_lock* lock);
void ticketlock_acquire(ticket_lock* lock);
void ticketlock_release(ticket_lock* lock);


// Shared variables for testing
ticket_lock test_lock;
condition_variable test_cv;
int shared_counter = 0;
int is_ready = 0;
int producer_done = 0;

// Helper functions to measure time
struct timespec timer_start() {
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    return start_time;
}

double timer_elapsed_ms(struct timespec start_time) {
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    return (end_time.tv_sec - start_time.tv_sec) * 1000.0 + 
           (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
}

//======== Basic Functionality Tests ========

// Test 1: Signal one waiting thread
void* wait_for_signal(void* arg) {
    int thread_id = *(int*)arg;
    
    printf("Thread %d waiting for signal\n", thread_id);
    
    // Acquire lock, then wait on condition
    ticketlock_acquire(&test_lock);
    
    while (!is_ready) {
        printf("Thread %d going to wait on condition\n", thread_id);
        condition_variable_wait(&test_cv, &test_lock);
        printf("Thread %d woke up from wait\n", thread_id);
    }
    
    // Update shared counter
    shared_counter++;
    printf("Thread %d incremented counter to %d\n", thread_id, shared_counter);
    
    ticketlock_release(&test_lock);
    
    return NULL;
}

void test_basic_signal() {
    printf("\n===== Basic Signal Test =====\n");
    
    // Initialize
    shared_counter = 0;
    is_ready = 0;
    ticketlock_init(&test_lock);
    condition_variable_init(&test_cv);
    
    // Create thread that will wait
    pthread_t thread;
    int thread_id = 1;
    
    pthread_create(&thread, NULL, wait_for_signal, &thread_id);
    
    // Give thread time to start waiting
    sleep(1);
    
    // Signal thread to wake up
    printf("Main thread: acquiring lock\n");
    ticketlock_acquire(&test_lock);
    
    printf("Main thread: setting is_ready to 1\n");
    is_ready = 1;
    
    printf("Main thread: signaling condition\n");
    condition_variable_signal(&test_cv);
    
    ticketlock_release(&test_lock);
    printf("Main thread: released lock\n");
    
    // Wait for thread to finish
    pthread_join(thread, NULL);
    
    // Check result
    assert(shared_counter == 1);
    printf("Basic signal test passed!\n");
}

// Test 2: Broadcast to multiple waiting threads
#define NUM_THREADS 5

void* wait_for_broadcast(void* arg) {
    int thread_id = *(int*)arg;
    
    printf("Thread %d waiting for broadcast\n", thread_id);
    
    // Acquire lock, then wait on condition
    ticketlock_acquire(&test_lock);
    
    while (!is_ready) {
        printf("Thread %d going to wait on condition\n", thread_id);
        condition_variable_wait(&test_cv, &test_lock);
        printf("Thread %d woke up from wait\n", thread_id);
    }
    
    // Update shared counter
    shared_counter++;
    printf("Thread %d incremented counter to %d\n", thread_id, shared_counter);
    
    ticketlock_release(&test_lock);
    
    return NULL;
}

void test_broadcast() {
    printf("\n===== Broadcast Test =====\n");
    
    // Initialize
    shared_counter = 0;
    is_ready = 0;
    ticketlock_init(&test_lock);
    condition_variable_init(&test_cv);
    
    // Create threads that will wait
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, wait_for_broadcast, &thread_ids[i]);
    }
    
    // Give threads time to start waiting
    sleep(1);
    
    // Broadcast to all threads
    printf("Main thread: acquiring lock\n");
    ticketlock_acquire(&test_lock);
    
    printf("Main thread: setting is_ready to 1\n");
    is_ready = 1;
    
    printf("Main thread: broadcasting condition\n");
    condition_variable_broadcast(&test_cv);
    
    ticketlock_release(&test_lock);
    printf("Main thread: released lock\n");
    
    // Wait for threads to finish
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Check result
    assert(shared_counter == NUM_THREADS);
    printf("Broadcast test passed!\n");
}

//======== Producer-Consumer Test ========

#define BUFFER_SIZE 10
#define ITEMS_TO_PRODUCE 100

int buffer[BUFFER_SIZE];
int buffer_count = 0;
int produce_index = 0;
int consume_index = 0;

void* producer(void* arg) {
    for (int i = 1; i <= ITEMS_TO_PRODUCE; i++) {
        ticketlock_acquire(&test_lock);
        
        // Wait while buffer is full
        while (buffer_count == BUFFER_SIZE) {
            condition_variable_wait(&test_cv, &test_lock);
        }
        
        // Add item to buffer
        buffer[produce_index] = i;
        produce_index = (produce_index + 1) % BUFFER_SIZE;
        buffer_count++;
        
        // Signal consumer
        condition_variable_signal(&test_cv);
        
        ticketlock_release(&test_lock);
    }
    
    // Indicate producer is done
    ticketlock_acquire(&test_lock);
    producer_done = 1;
    condition_variable_broadcast(&test_cv);  // Wake up any waiting consumers
    ticketlock_release(&test_lock);
    
    return NULL;
}

void* consumer(void* arg) {
    int consumed_count = 0;
    int thread_id = *(int*)arg;
    
    while (1) {
        ticketlock_acquire(&test_lock);
        
        // Wait while buffer is empty
        while (buffer_count == 0) {
            // Exit if producer is done and buffer is empty
            if (producer_done) {
                ticketlock_release(&test_lock);
                printf("Consumer %d consumed %d items\n", thread_id, consumed_count);
                return NULL;
            }
            condition_variable_wait(&test_cv, &test_lock);
        }
        
        // Remove item from buffer
        int item = buffer[consume_index];
        consume_index = (consume_index + 1) % BUFFER_SIZE;
        buffer_count--;
        consumed_count++;
        
        // Signal producer
        condition_variable_signal(&test_cv);
        
        ticketlock_release(&test_lock);
    }
    
    return NULL;
}

void test_producer_consumer() {
    printf("\n===== Producer-Consumer Test =====\n");
    
    // Initialize
    buffer_count = 0;
    produce_index = 0;
    consume_index = 0;
    producer_done = 0;
    ticketlock_init(&test_lock);
    condition_variable_init(&test_cv);
    
    // Create threads
    pthread_t prod_thread;
    pthread_t cons_threads[3];
    int cons_ids[3] = {1, 2, 3};
    
    // Start consumer threads
    for (int i = 0; i < 3; i++) {
        pthread_create(&cons_threads[i], NULL, consumer, &cons_ids[i]);
    }
    
    // Start producer thread
    pthread_create(&prod_thread, NULL, producer, NULL);
    
    // Wait for threads to complete
    pthread_join(prod_thread, NULL);
    for (int i = 0; i < 3; i++) {
        pthread_join(cons_threads[i], NULL);
    }
    
    // Check final state
    assert(buffer_count == 0);
    assert(producer_done == 1);
    
    printf("Producer-Consumer test passed!\n");
}

//======== Stress Test ========

#define STRESS_THREADS 30
#define ITERATIONS_PER_THREAD 300
int stress_counter =0;
void* stress_thread(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        ticketlock_acquire(&test_lock);
        
        // Increment counter
        stress_counter++;
        
        if (i % 10 == 0) {
            printf("Thread %d completed %d iterations\n", thread_id, i);
        }
        
        // Signal or broadcast randomly
        if (rand() % 10 == 0) {
            condition_variable_broadcast(&test_cv);
        } else {
            condition_variable_signal(&test_cv);
        }
        
        // Occasionally wait
        if (rand() % 5 == 0) {
            printf("Thread %d going to wait\n", thread_id);
            condition_variable_wait(&test_cv, &test_lock);
            printf("Thread %d woke up\n", thread_id);
        }
        
        ticketlock_release(&test_lock);
        
        // Small random delay
        usleep(10);  // 0.01ms
    }
    
    printf("Thread %d completed all iterations\n", thread_id);
    return NULL;
}

void test_stress() {
    printf("\n===== Stress Test =====\n");
    
    // Initialize
    stress_counter = 0;
    ticketlock_init(&test_lock);
    condition_variable_init(&test_cv);
    
    // Seed random number generator
    srand(time(NULL));
    
    // Create threads
    pthread_t threads[STRESS_THREADS];
    int thread_ids[STRESS_THREADS];
    
    struct timespec start_time = timer_start();
    
    for (int i = 0; i < STRESS_THREADS; i++) {
        thread_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, stress_thread, &thread_ids[i]);
    }
    
    // Wait for threads to complete
    for (int i = 0; i < STRESS_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    double elapsed = timer_elapsed_ms(start_time);
    
    // Check final counter
    assert(stress_counter == STRESS_THREADS * ITERATIONS_PER_THREAD);
    
    printf("Stress test passed! Time: %.2f ms\n", elapsed);
    printf("Final counter value: %d\n", stress_counter);
}

//======== Edge Cases ========

// Test 3: Signal with no waiting threads
void test_signal_no_waiters() {
    printf("\n===== Signal with No Waiters Test =====\n");
    
    // Initialize
    ticketlock_init(&test_lock);
    condition_variable_init(&test_cv);
    
    // Signal with no waiting threads
    ticketlock_acquire(&test_lock);
    printf("Signaling with no waiting threads\n");
    condition_variable_signal(&test_cv);
    ticketlock_release(&test_lock);
    
    printf("Signal with no waiters test passed!\n");
}

// Test 4: Broadcast with no waiting threads
void test_broadcast_no_waiters() {
    printf("\n===== Broadcast with No Waiters Test =====\n");
    
    // Initialize
    ticketlock_init(&test_lock);
    condition_variable_init(&test_cv);
    
    // Broadcast with no waiting threads
    ticketlock_acquire(&test_lock);
    printf("Broadcasting with no waiting threads\n");
    condition_variable_broadcast(&test_cv);
    ticketlock_release(&test_lock);
    
    printf("Broadcast with no waiters test passed!\n");
}

// Test 5: Spurious wakeup handling
volatile int spurious_checks = 0;

void* test_spurious_thread(void* arg) {
    ticketlock_acquire(&test_lock);
    
    // Wait for a signal, but check the predicate
    while (!is_ready) {
        spurious_checks++;
        condition_variable_wait(&test_cv, &test_lock);
        printf("Thread woke up, checking condition (is_ready = %d)\n", is_ready);
    }
    
    shared_counter++;
    
    ticketlock_release(&test_lock);
    return NULL;
}

void test_spurious_wakeup() {
    printf("\n===== Spurious Wakeup Test =====\n");
    
    // Initialize
    shared_counter = 0;
    is_ready = 0;
    spurious_checks = 0;
    ticketlock_init(&test_lock);
    condition_variable_init(&test_cv);
    
    // Create thread that will wait
    pthread_t thread;
    pthread_create(&thread, NULL, test_spurious_thread, NULL);
    
    // Give thread time to start waiting
    sleep(1);
    
    // Signal the thread but DON'T set the predicate
    ticketlock_acquire(&test_lock);
    printf("Signaling without changing the predicate\n");
    condition_variable_signal(&test_cv);
    ticketlock_release(&test_lock);
    
    // Give thread time to wake up and check
    sleep(1);
    
    // Now set the predicate and signal again
    ticketlock_acquire(&test_lock);
    printf("Setting is_ready=1 and signaling again\n");
    is_ready = 1;
    condition_variable_signal(&test_cv);
    ticketlock_release(&test_lock);
    
    // Wait for thread to finish
    pthread_join(thread, NULL);
    
    // Check results
    printf("Thread checked predicate %d times\n", spurious_checks);
    assert(shared_counter == 1);
    assert(spurious_checks >= 1);  // Should have checked at least once
    
    printf("Spurious wakeup test passed!\n");
}

// Test 6: Multiple wait/signal cycles
#define CYCLES 10

void* cycle_thread(void* arg) {
    int thread_id = *(int*)arg;
    int local_counter = 0;
    
    for (int i = 0; i < CYCLES; i++) {
        ticketlock_acquire(&test_lock);
        
        while (shared_counter != thread_id) {
            condition_variable_wait(&test_cv, &test_lock);
        }
        
        // Move to next thread's turn
        shared_counter = (shared_counter + 1) % NUM_THREADS;
        local_counter++;
        
        // Signal next thread
        condition_variable_signal(&test_cv);
        
        ticketlock_release(&test_lock);
    }
    
    // Verify correct number of cycles
    assert(local_counter == CYCLES);
    return NULL;
}

void test_multiple_cycles() {
    printf("\n===== Multiple Wait/Signal Cycles Test =====\n");
    
    // Initialize
    shared_counter = 0;  // Start with thread 0's turn
    ticketlock_init(&test_lock);
    condition_variable_init(&test_cv);
    
    // Create threads
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, cycle_thread, &thread_ids[i]);
    }
    
    // Wait for threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Multiple cycles test passed!\n");
}

// Utility functions for ticket lock
void ticketlock_init(ticket_lock* lock) {
    atomic_init(&lock->ticket, 0);
    atomic_init(&lock->current_ticket, 0);
}

void ticketlock_acquire(ticket_lock* lock) {
    int my_ticket = atomic_fetch_add(&lock->ticket, 1);
    while (atomic_load(&lock->current_ticket) != my_ticket) {
        sched_yield();
    }
}

void ticketlock_release(ticket_lock* lock) {
    atomic_fetch_add(&lock->current_ticket, 1);
}

int main() {
    printf("Starting condition variable tests...\n");
    
    // Basic tests
    test_basic_signal();
    test_broadcast();
    test_signal_no_waiters();
    test_broadcast_no_waiters();
    
    // Advanced tests
    test_spurious_wakeup();
    test_multiple_cycles();
    
    // Functional test
    test_producer_consumer();
    
    // Stress test
    test_stress();
    
    printf("\nAll tests passed!\n");
    return 0;
}