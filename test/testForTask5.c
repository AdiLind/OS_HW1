#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include "local_storage.h"  // Make sure this has all your declarations

#define NUM_THREADS 5

// Test accessing uninitialized TLS
void test_uninitialized_tls() {
    printf("Testing get_tls_data before allocation (should exit with code 2):\n");
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        get_tls_data();  // expected to fail
        printf("❌ get_tls_data didn't exit as expected\n");
        exit(1);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 2) {
            printf("✅ get_tls_data correctly exited with code 2\n");
        } else {
            printf("❌ get_tls_data didn't exit with code 2\n");
        }
    }
}

// Test accessing TLS after freeing
void test_after_free_tls() {
    printf("Testing set_tls_data after freeing (should exit with code 2):\n");
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        init_storage();
        tls_thread_alloc();
        int data = 42;
        tls_thread_free();
        set_tls_data(&data);  // expected to fail
        printf("❌ set_tls_data didn't exit as expected\n");
        exit(1);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 2) {
            printf("✅ set_tls_data correctly exited with code 2\n");
        } else {
            printf("❌ set_tls_data didn't exit with code 2\n");
        }
    }
}

void* thread_func(void* arg) {
    long tid = (long)arg;

    tls_thread_alloc();

    // Try allocating again (should do nothing)
    tls_thread_alloc();

    // Allocate some memory for test data
    int* my_data = malloc(sizeof(int));
    *my_data = 100 + tid;

    // Set TLS data
    set_tls_data(my_data);

    // Get TLS data and validate
    int* retrieved = (int*)get_tls_data();
    if (retrieved && *retrieved == *my_data) {
        printf("Thread %ld: data = %d ✅\n", tid, *retrieved);
    } else {
        printf("Thread %ld: ❌ Data mismatch or NULL\n", tid);
    }

    // Free TLS
    tls_thread_free();
    free(my_data);
    return NULL;
}

int main() {
    // First, test edge cases in isolated processes
    test_uninitialized_tls();
    test_after_free_tls();
    
    printf("\nTesting normal TLS functionality with multiple threads:\n");
    init_storage();  // Initialize global TLS array

    pthread_t threads[NUM_THREADS];
    for (long i = 0; i < NUM_THREADS; ++i) {
        pthread_create(&threads[i], NULL, thread_func, (void*)i);
        usleep(10000);  // Slight delay to help with interleaving
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    printf("All threads finished.\n");
    return 0;
}