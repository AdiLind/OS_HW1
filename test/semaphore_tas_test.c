#include <stdio.h>
#include <pthread.h>
#include "../task1/tas_semaphore.h"

// Global variables
semaphore sem;
int shared_counter = 0;

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

// Thread function for testing blocking behavior
void* consumer(void* arg) {
    for (int i = 0; i < 5; i++) {
        printf("Consumer: Waiting for resource...\n");
        semaphore_wait(&sem);
        printf("Consumer: Got resource %d\n", i+1);
    }
    return NULL;
}

void* producer(void* arg) {
    for (int i = 0; i < 5; i++) {
        printf("Producer: Creating resource %d\n", i+1);
        semaphore_signal(&sem);
        // Sleep to make the test more observable
        struct timespec ts = {0, 100000000}; // 100ms
        nanosleep(&ts, NULL);
    }
    return NULL;
}

int main() {
    // Test 1: Basic mutual exclusion
    printf("Test 1: Basic mutual exclusion\n");
    semaphore_init(&sem, 1); // Binary semaphore (mutex)
    
    pthread_t threads[2];
    int iterations = 100000;
    
    pthread_create(&threads[0], NULL, increment_counter, &iterations);
    pthread_create(&threads[1], NULL, increment_counter, &iterations);
    
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    
    printf("Final counter value: %d (expected: %d)\n", shared_counter, 2*iterations);
    
    // Test 2: Producer-Consumer
    printf("\nTest 2: Producer-Consumer\n");
    semaphore_init(&sem, 0); // Start with 0 resources
    
    pthread_create(&threads[0], NULL, consumer, NULL);
    
    // Small delay to ensure consumer starts first
    struct timespec ts = {0, 100000000}; // 100ms
    nanosleep(&ts, NULL);
    
    pthread_create(&threads[1], NULL, producer, NULL);
    
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    
    printf("Producer-Consumer test completed\n");
    
    return 0;
}