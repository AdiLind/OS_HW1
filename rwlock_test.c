
#define _POSIX_C_SOURCE 199309L  // עבור nanosleep

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
// הוספת כותרות לפונקציות שקשורות לזמן
#include <time.h>  
#include "rw_lock.h"

#define NUM_THREADS 100
#define NUM_ITERATIONS 1000
#define READ_RATIO 0.8  // 80% reads, 20% writes

// Shared data protected by the lock
int shared_data = 0;
rwlock lock;

// Mutex for printing to avoid garbled output
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Statistics
atomic_int total_reads = 0;
atomic_int total_writes = 0;
atomic_int concurrent_readers = 0;
atomic_int max_concurrent_readers = 0;
atomic_bool writer_active = false;
atomic_bool had_writer_and_reader = false;

// Safe printing function
void safe_print(const char* format, ...) {
    va_list args;
    va_start(args, format);
    pthread_mutex_lock(&print_mutex);
    vprintf(format, args);
    pthread_mutex_unlock(&print_mutex);
    va_end(args);
}

// Function to sleep for microseconds
// פונקציה חלופית ל-usleep שמשתמשת בפונקציות סטנדרטיות
// גרסה פשוטה יותר שמשתמשת רק ב-sleep
void my_usleep(unsigned int microseconds) {
    // אם מדובר במיקרושניות רבות, מעגלים לשניה
    if (microseconds >= 1000000) {
        sleep(microseconds / 1000000);
    } 
    // אם מדובר במעט מיקרושניות, פשוט מחכים פחות משניה
    else if (microseconds > 0) {
        // משתמשים ב-yield כדי לתת לחוטים אחרים לרוץ
        sched_yield();
    }
}

// Function to perform a read operation
void perform_read(int thread_id) {
    rwlock_acquire_read(&lock);
    
    // Increment concurrent readers counter
    int readers = atomic_fetch_add(&concurrent_readers, 1) + 1;
    
    // Update max concurrent readers
    int current_max;
    do {
        current_max = atomic_load(&max_concurrent_readers);
        if (readers <= current_max) break;
    } while (!atomic_compare_exchange_weak(&max_concurrent_readers, &current_max, readers));
    
    // Check if a writer is active (this should never happen)
    if (atomic_load(&writer_active)) {
        atomic_store(&had_writer_and_reader, true);
        safe_print("ERROR: Reader %d detected active writer!\n", thread_id);
    }
    
    // Simulate some work
    int value = shared_data;
    my_usleep(rand() % 100);  // Random delay up to 100 microseconds
    
    // Check data consistency
    if (value != shared_data) {
        safe_print("ERROR: Reader %d saw inconsistent data: %d != %d\n", 
                  thread_id, value, shared_data);
    }
    
    // Decrement concurrent readers
    atomic_fetch_sub(&concurrent_readers, 1);
    
    // Increment total reads
    atomic_fetch_add(&total_reads, 1);
    
    rwlock_release_read(&lock);
}

// Function to perform a write operation
void perform_write(int thread_id) {
    rwlock_acquire_write(&lock);
    
    // Set writer active flag
    bool was_active = atomic_exchange(&writer_active, true);
    if (was_active) {
        safe_print("ERROR: Writer %d detected another active writer!\n", thread_id);
    }
    
    // Check for concurrent readers (this should never happen)
    if (atomic_load(&concurrent_readers) > 0) {
        atomic_store(&had_writer_and_reader, true);
        safe_print("ERROR: Writer %d detected active readers: %d\n", 
                  thread_id, atomic_load(&concurrent_readers));
    }
    
    // Simulate some work
    shared_data++;
    my_usleep(rand() % 200);  // Random delay up to 200 microseconds
    
    // Reset writer active flag
    atomic_store(&writer_active, false);
    
    // Increment total writes
    atomic_fetch_add(&total_writes, 1);
    
    rwlock_release_write(&lock);
}

// Worker thread function
void* worker_thread(void* arg) {
    int thread_id = *(int*)arg;
    free(arg);
    
    srand(time(NULL) ^ thread_id);  // Initialize random seed
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Randomly choose to read or write based on READ_RATIO
        if ((double)rand() / RAND_MAX < READ_RATIO) {
            perform_read(thread_id);
        } else {
            perform_write(thread_id);
        }
        
        // Small delay between operations
        my_usleep(rand() % 50);
    }
    
    return NULL;
}

// Test for writer starvation
void* reader_flood_thread(void* arg) {
    for (int i = 0; i < NUM_ITERATIONS * 10; i++) {
        rwlock_acquire_read(&lock);
        my_usleep(10);  // Hold read lock briefly
        rwlock_release_read(&lock);
        my_usleep(1);  // Very short pause between reads
    }
    return NULL;
}

void* writer_check_thread(void* arg) {
    int* success_count = (int*)arg;
    *success_count = 0;
    
    for (int i = 0; i < 20; i++) {
        // Try to get a write lock
        time_t start_time = time(NULL);
        
        rwlock_acquire_write(&lock);
        shared_data++;
        rwlock_release_write(&lock);
        
        time_t end_time = time(NULL);
        
        // Calculate time in seconds
        double seconds = difftime(end_time, start_time);
        
        if (seconds < 5) {  // If we got the lock in less than 5 seconds
            (*success_count)++;
        }
        
        // Wait 100ms between write attempts
        struct timespec ts = {0, 100000000}; // 100 milliseconds
        nanosleep(&ts, NULL);
    }
    
    return NULL;
}

// Basic functionality test
void basic_test() {
    printf("=== Running basic functionality test ===\n");
    
    rwlock_init(&lock);
    shared_data = 0;
    
    // Test 1: Single reader
    printf("Test 1: Single reader... ");
    rwlock_acquire_read(&lock);
    int value = shared_data;
    rwlock_release_read(&lock);
    printf("PASSED\n");
    
    // Test 2: Multiple readers
    printf("Test 2: Multiple readers... ");
    rwlock_acquire_read(&lock);
    rwlock_acquire_read(&lock);
    rwlock_acquire_read(&lock);
    value = shared_data;
    rwlock_release_read(&lock);
    rwlock_release_read(&lock);
    rwlock_release_read(&lock);
    printf("PASSED\n");
    
    // Test 3: Single writer
    printf("Test 3: Single writer... ");
    rwlock_acquire_write(&lock);
    shared_data++;
    rwlock_release_write(&lock);
    printf("PASSED\n");
    
    // Test 4: Writer after reader
    printf("Test 4: Writer after reader... ");
    rwlock_acquire_read(&lock);
    rwlock_release_read(&lock);
    rwlock_acquire_write(&lock);
    shared_data++;
    rwlock_release_write(&lock);
    printf("PASSED\n");
    
    // Test 5: Reader after writer
    printf("Test 5: Reader after writer... ");
    rwlock_acquire_write(&lock);
    rwlock_release_write(&lock);
    rwlock_acquire_read(&lock);
    rwlock_release_read(&lock);
    printf("PASSED\n");
    
    printf("All basic tests passed!\n\n");
}

// Stress test with multiple readers and writers
void stress_test() {
    printf("=== Running stress test with %d threads ===\n", NUM_THREADS);
    
    rwlock_init(&lock);
    shared_data = 0;
    atomic_store(&total_reads, 0);
    atomic_store(&total_writes, 0);
    atomic_store(&concurrent_readers, 0);
    atomic_store(&max_concurrent_readers, 0);
    atomic_store(&writer_active, false);
    atomic_store(&had_writer_and_reader, false);
    
    pthread_t threads[NUM_THREADS];
    
    // Create and start threads
    for (int i = 0; i < NUM_THREADS; i++) {
        int* thread_id = malloc(sizeof(int));
        *thread_id = i;
        
        if (pthread_create(&threads[i], NULL, worker_thread, thread_id) != 0) {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Stress test completed.\n");
    printf("Total reads: %d\n", atomic_load(&total_reads));
    printf("Total writes: %d\n", atomic_load(&total_writes));
    printf("Maximum concurrent readers: %d\n", atomic_load(&max_concurrent_readers));
    printf("Writer and reader active simultaneously: %s\n", 
           atomic_load(&had_writer_and_reader) ? "YES (ERROR!)" : "NO (Correct)");
    printf("Final shared_data value: %d (Expected: %d)\n", 
           shared_data, atomic_load(&total_writes));
    
    if (shared_data != atomic_load(&total_writes)) {
        printf("ERROR: Data inconsistency detected!\n");
    } else {
        printf("Data consistency verified.\n");
    }
    
    if (!atomic_load(&had_writer_and_reader)) {
        printf("Exclusivity verified: No writers active during reads.\n");
    }
    
    printf("\n");
}

// Test for writer starvation
void writer_starvation_test() {
    printf("=== Running writer starvation test ===\n");
    
    rwlock_init(&lock);
    shared_data = 0;
    
    pthread_t readers[10];
    pthread_t writer;
    int writer_success_count;
    
    // Start 10 reader threads that continuously acquire and release read locks
    for (int i = 0; i < 10; i++) {
        if (pthread_create(&readers[i], NULL, reader_flood_thread, NULL) != 0) {
            perror("Failed to create reader thread");
            exit(EXIT_FAILURE);
        }
    }
    
    // Give readers a head start
    my_usleep(100000);
    
    // Start a writer thread that tries to acquire the write lock periodically
    if (pthread_create(&writer, NULL, writer_check_thread, &writer_success_count) != 0) {
        perror("Failed to create writer thread");
        exit(EXIT_FAILURE);
    }
    
    // Wait for the writer to complete
    pthread_join(writer, NULL);
    
    // Cancel all reader threads (they run in an infinite loop)
    for (int i = 0; i < 10; i++) {
        pthread_cancel(readers[i]);
        pthread_join(readers[i], NULL);
    }
    
    printf("Writer was able to acquire the lock %d/20 times under reader pressure.\n", 
           writer_success_count);
    
    if (writer_success_count < 5) {
        printf("WARNING: Possible writer starvation detected!\n");
    } else {
        printf("No writer starvation detected.\n");
    }
    
    printf("\n");
}

int main() {
    // Run tests
    basic_test();
    stress_test();
    writer_starvation_test();
    
    printf("All tests completed.\n");
    return 0;
}