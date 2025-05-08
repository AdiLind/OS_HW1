#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include "tl_semaphore.h"

// For tracking test results
typedef struct {
    int passed;
    int failed;
} test_results;

test_results results = {0, 0};

// Global variables
semaphore sem;
atomic_int shared_counter = 0;
atomic_int active_threads = 0;

// Helper function to format time for benchmarking
void print_time_diff(struct timespec start, struct timespec end, const char *label) {
    double time_taken = (end.tv_sec - start.tv_sec) * 1e9;
    time_taken = (time_taken + (end.tv_nsec - start.tv_nsec)) * 1e-9;
    printf("[TIME] %s: %.6f seconds\n", label, time_taken);
}

// Thread function for basic functionality test
void* increment_counter(void* arg) {
    int iterations = *((int*)arg);
    int success_count = 0;
    
    for (int i = 0; i < iterations; i++) {
        semaphore_wait(&sem);
        // Critical section
        int current = atomic_load(&shared_counter);
        // Simulate some work
        usleep(1);
        atomic_store(&shared_counter, current + 1);
        success_count++;
        semaphore_signal(&sem);
    }
    
    printf("Thread %lu completed %d/%d iterations\n", 
           pthread_self(), success_count, iterations);
    return NULL;
}

// Edge case: test with initial value 0
bool test_initial_zero() {
    printf("\n--- Test: Initial value 0 ---\n");
    semaphore_init(&sem, 0);
    
    pthread_t thread1, thread2;
    bool success = true;
    
    // First thread should block
    printf("Starting thread 1 (should block)...\n");
    pthread_create(&thread1, NULL, increment_counter, &(int){1});
    
    // Wait a bit to ensure thread1 is blocked
    usleep(100000);
    
    // Check if counter is still 0
    if (atomic_load(&shared_counter) != 0) {
        printf("❌ FAIL: Counter was incremented when it should be blocked!\n");
        success = false;
    } else {
        printf("✓ Thread is blocked as expected\n");
    }
    
    // Signal to let thread1 proceed
    printf("Signaling semaphore to unblock thread...\n");
    semaphore_signal(&sem);
    
    // Wait for thread1 to complete
    pthread_join(thread1, NULL);
    
    // Check if counter is now 1
    if (atomic_load(&shared_counter) != 1) {
        printf("❌ FAIL: Counter is %d, expected 1\n", atomic_load(&shared_counter));
        success = false;
    } else {
        printf("✓ Counter is 1 as expected\n");
    }
    
    return success;
}

// Test basic mutex functionality
bool test_mutex() {
    printf("\n--- Test: Mutex functionality ---\n");
    atomic_store(&shared_counter, 0);
    semaphore_init(&sem, 1);
    
    pthread_t threads[5];
    int iterations = 1000;
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < 5; i++) {
        pthread_create(&threads[i], NULL, increment_counter, &iterations);
    }
    
    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    print_time_diff(start, end, "Mutex test with 5 threads doing 1000 iterations each");
    
    bool success = true;
    if (atomic_load(&shared_counter) != 5 * iterations) {
        printf("❌ FAIL: Counter is %d, expected %d\n", 
               atomic_load(&shared_counter), 5 * iterations);
        success = false;
    } else {
        printf("✓ PASS: Counter is %d as expected\n", 
               atomic_load(&shared_counter));
    }
    
    return success;
}

// Thread function for counting semaphore test
void* resource_user(void* arg) {
    int id = *((int*)arg);
    int iterations = 10;
    
    atomic_fetch_add(&active_threads, 1);
    
    for (int i = 0; i < iterations; i++) {
        printf("Thread %d: Waiting for resource...\n", id);
        semaphore_wait(&sem);
        
        printf("Thread %d: Got resource, active threads: %d\n", 
               id, atomic_load(&active_threads));
        
        // Verify that we don't exceed the resource limit
        int current_active = atomic_load(&active_threads);
        int sem_value = 3; // Initial semaphore value (resources)
        if (current_active > sem_value) {
            printf("❌ CRITICAL ERROR: Too many active threads (%d)!\n", current_active);
        }
        
        // Simulate using the resource
        usleep(10000 + rand() % 20000);
        
        printf("Thread %d: Releasing resource\n", id);
        semaphore_signal(&sem);
        
        // Small delay between iterations
        usleep(5000);
    }
    
    atomic_fetch_sub(&active_threads, 1);
    return NULL;
}

// Test counting semaphore functionality
bool test_counting_semaphore() {
    printf("\n--- Test: Counting semaphore ---\n");
    atomic_store(&active_threads, 0);
    
    // Create a semaphore with 3 resources
    int resource_count = 3;
    semaphore_init(&sem, resource_count);
    
    pthread_t threads[10];
    int thread_ids[10];
    
    for (int i = 0; i < 10; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, resource_user, &thread_ids[i]);
    }
    
    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("✓ Counting semaphore test completed\n");
    return true;
}

// Thread function for fairness test
void* fairness_test_thread(void* arg) {
    int id = *((int*)arg);
    
    printf("Thread %d: Waiting for the semaphore...\n", id);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    semaphore_wait(&sem);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    print_time_diff(start, end, "Wait time for thread");
    
    printf("Thread %d: Got the semaphore!\n", id);
    
    // Hold the semaphore for a moment
    usleep(100000);
    
    printf("Thread %d: Releasing the semaphore\n", id);
    semaphore_signal(&sem);
    
    return NULL;
}

// Test fairness (FIFO property of ticket lock)
bool test_fairness() {
    printf("\n--- Test: Fairness (FIFO property) ---\n");
    printf("This test checks if threads are served in the order they request the semaphore\n");
    
    // Initialize the semaphore with 0 permits
    semaphore_init(&sem, 0);
    
    pthread_t threads[5];
    int thread_ids[5];
    
    // Start 5 threads that will all block
    for (int i = 0; i < 5; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, fairness_test_thread, &thread_ids[i]);
        
        // Small delay to ensure threads start in order
        usleep(50000);
    }
    
    // Give the threads time to queue up
    usleep(200000);
    
    printf("Main: All threads should be waiting now, releasing them one by one...\n");
    
    // Signal 5 times to release all threads
    for (int i = 0; i < 5; i++) {
        printf("Main: Signaling semaphore (%d of 5)\n", i+1);
        semaphore_signal(&sem);
        usleep(200000); // Give thread time to print
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("✓ Fairness test completed. Check the order of thread acquisition above.\n");
    printf("If thread IDs appear in order (0, 1, 2, 3, 4), the lock has good fairness properties.\n");
    
    return true;
}

// Stress test with many threads
bool stress_test() {
    printf("\n--- Stress Test: Many threads ---\n");
    atomic_store(&shared_counter, 0);
    
    // Create a binary semaphore
    semaphore_init(&sem, 1);
    
    // Create a lot of threads
    int num_threads = 50;
    pthread_t threads[num_threads];
    int iterations = 1000;
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, increment_counter, &iterations);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    print_time_diff(start, end, "Stress test");
    
    bool success = true;
    if (atomic_load(&shared_counter) != num_threads * iterations) {
        printf("❌ FAIL: Counter is %d, expected %d\n", 
               atomic_load(&shared_counter), num_threads * iterations);
        success = false;
    } else {
        printf("✓ PASS: Counter is %d as expected\n", 
               atomic_load(&shared_counter));
    }
    
    return success;
}

// Edge case: multiple wait and signal operations
void* multiple_ops_thread(void* arg) {
    int id = *((int*)arg);
    
    // Multiple wait operations
    printf("Thread %d: Doing 3 consecutive wait operations\n", id);
    for (int i = 0; i < 3; i++) {
        semaphore_wait(&sem);
        printf("Thread %d: Got semaphore permit %d\n", id, i+1);
    }
    
    printf("Thread %d: Holding 3 permits, sleeping briefly...\n", id);
    usleep(100000);
    
    // Multiple signal operations
    printf("Thread %d: Doing 3 consecutive signal operations\n", id);
    for (int i = 0; i < 3; i++) {
        semaphore_signal(&sem);
        printf("Thread %d: Released semaphore permit %d\n", id, i+1);
    }
    
    return NULL;
}

// Test multiple operations
bool test_multiple_operations() {
    printf("\n--- Test: Multiple wait/signal operations ---\n");
    
    // Initialize with 3 permits
    semaphore_init(&sem, 3);
    
    pthread_t thread1, thread2;
    int id1 = 1, id2 = 2;
    
    pthread_create(&thread1, NULL, multiple_ops_thread, &id1);
    
    // Give thread1 time to acquire all permits
    usleep(50000);
    
    pthread_create(&thread2, NULL, multiple_ops_thread, &id2);
    
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    
    printf("✓ Multiple operations test completed\n");
    return true;
}

// Run all tests
void run_tests() {
    srand(time(NULL));
    printf("=== TICKET LOCK SEMAPHORE TEST SUITE ===\n");
    
    struct timespec overall_start, overall_end;
    clock_gettime(CLOCK_MONOTONIC, &overall_start);
    
    // Run tests and track results
    results.passed += test_initial_zero() ? 1 : 0;
    results.failed += test_initial_zero() ? 0 : 1;
    
    results.passed += test_mutex() ? 1 : 0;
    results.failed += test_mutex() ? 0 : 1;
    
    results.passed += test_counting_semaphore() ? 1 : 0;
    results.failed += test_counting_semaphore() ? 0 : 1;
    
    results.passed += test_fairness() ? 1 : 0;
    results.failed += test_fairness() ? 0 : 1;
    
    results.passed += test_multiple_operations() ? 1 : 0;
    results.failed += test_multiple_operations() ? 0 : 1;
    
    results.passed += stress_test() ? 1 : 0;
    results.failed += stress_test() ? 0 : 1;
    
    clock_gettime(CLOCK_MONOTONIC, &overall_end);
    print_time_diff(overall_start, overall_end, "Total test time");
    
    // Print summary
    printf("\n=== TEST SUMMARY ===\n");
    printf("Tests passed: %d\n", results.passed);
    printf("Tests failed: %d\n", results.failed);
    
    if (results.failed == 0) {
        printf("✅ ALL TESTS PASSED! Your implementation appears to be working correctly.\n");
    } else {
        printf("❌ SOME TESTS FAILED. Check the output above for details.\n");
    }
}

int main() {
    run_tests();
    return 0;
}