#include <stdio.h>
#include <pthread.h>
#include <time.h>    // For nanosleep
#include <stdlib.h>  // For rand and srand
#include <unistd.h>  // For usleep
#include "../task1/tas_semaphore.h"

// Global variables - we'll define them here
semaphore sem;
int shared_counter = 0;
int resources_consumed = 0;
int resource_array[10] = {0}; // For resource fairness test
int stop_flag = 0; // Flag to stop threads

// Helper for sleeping random intervals
void random_sleep(int max_ms) {
    usleep((rand() % max_ms) * 1000);
}

// Thread function for testing basic functionality
void* increment_counter(void* arg) {
    int iterations = *((int*)arg);
    
    for (int i = 0; i < iterations; i++) {
        semaphore_wait(&sem);
        // Critical section
        shared_counter++;
        semaphore_signal(&sem);
    }
    
    return NULL;
}

// Basic test
void test_basic_functionality() {
    printf("\n--- Test 1: Basic mutual exclusion ---\n");
    semaphore_init(&sem, 1); // Binary semaphore (mutex)
    
    pthread_t threads[2];
    int iterations = 100000;
    
    pthread_create(&threads[0], NULL, increment_counter, &iterations);
    pthread_create(&threads[1], NULL, increment_counter, &iterations);
    
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    
    printf("Final counter value: %d (expected: %d)\n", shared_counter, 2*iterations);
}

// Thread function for testing blocking behavior
void* consumer_thread(void* arg) {
    int id = *((int*)arg);
    for (int i = 0; i < 2; i++) {
        printf("Consumer %d: Waiting for resource...\n", id);
        semaphore_wait(&sem);
        printf("Consumer %d: Got resource %d\n", id, i+1);
        // Hold resource for a while
        usleep(100000); // 100ms
    }
    return NULL;
}

void* producer_thread(void* arg) {
    int id = *((int*)arg);
    for (int i = 0; i < 4; i++) {  
        printf("Producer %d: Creating resource %d\n", id, i+1);
        semaphore_signal(&sem);
        usleep(200000);
    }
    return NULL;
}

// Producer-consumer test
void test_producer_consumer() {
    printf("\n--- Test 2: Producer-Consumer ---\n");
    semaphore_init(&sem, 0); // Start with 0 resources
    
    pthread_t threads[3];
    int thread_ids[3] = {0, 1, 2};
    
    // Create two consumers
    pthread_create(&threads[0], NULL, consumer_thread, &thread_ids[0]);
    pthread_create(&threads[1], NULL, consumer_thread, &thread_ids[1]);
    
    // Give consumers time to start waiting
    usleep(50000); // 50ms
    
    // Create one producer
    pthread_create(&threads[2], NULL, producer_thread, &thread_ids[2]);
    
    // Wait for all threads to finish
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Producer-Consumer test completed\n");
}

int main() {
    // Seed random number generator
    srand(time(NULL));
    
    // Run tests
    test_basic_functionality();
    
    // Reset counter for next test
    shared_counter = 0;
    
    test_producer_consumer();
    
    return 0;
}