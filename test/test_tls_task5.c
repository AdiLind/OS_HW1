#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "local_storage.h"

// Colors for prettier output
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define RESET "\033[0m"

#define NUM_BASIC_THREADS 5
#define NUM_STRESS_THREADS 50
#define ITERATIONS_PER_THREAD 1000

// Global flag to track if tests passed
int all_tests_passed = 1;

// Test data structure
typedef struct {
    int value;
    double another_value;
    char text[20];
} test_data_t;

// Helper function to report test results
void report_test(const char* test_name, int result) {
    if (result) {
        printf("[%sPASS%s] %s\n", GREEN, RESET, test_name);
    } else {
        printf("[%sFAIL%s] %s\n", RED, RESET, test_name);
        all_tests_passed = 0;
    }
}

// Test 1: Basic functionality with a single thread
void test_basic_functionality() {
    printf("\n%s=== Test 1: Basic Functionality ===%s\n", YELLOW, RESET);
    
    // Initialize storage
    init_storage();
    
    // Allocate TLS for this thread
    tls_thread_alloc();
    
    // Verify data is initially NULL
    void* data = get_tls_data();
    report_test("Initial data is NULL", data == NULL);
    
    // Set data and verify
    int value = 42;
    set_tls_data(&value);
    data = get_tls_data();
    report_test("Data after set is correct", data != NULL && *(int*)data == 42);
    
    // Free TLS and re-allocate
    tls_thread_free();
    tls_thread_alloc();
    
    // Verify data is NULL after re-allocation
    data = get_tls_data();
    report_test("Data is NULL after free and re-allocate", data == NULL);
    
    // Clean up
    tls_thread_free();
}

// Test 2: Multiple threads basic functionality
void* thread_basic_func(void* arg) {
    long thread_num = (long)arg;
    int value = (int)thread_num * 100;
    
    // Allocate TLS for this thread
    tls_thread_alloc();
    
    // Set a unique value based on thread number
    set_tls_data(&value);
    
    // Get the data and verify it's what we set
    void* data = get_tls_data();
    int result = (data != NULL && *(int*)data == value);
    
    // Free the TLS
    tls_thread_free();
    
    return (void*)(long)result;
}

void test_multiple_threads_basic() {
    printf("\n%s=== Test 2: Multiple Threads Basic ===%s\n", YELLOW, RESET);
    
    // Initialize storage
    init_storage();
    
    pthread_t threads[NUM_BASIC_THREADS];
    int success = 1;
    
    // Create threads
    for (long i = 0; i < NUM_BASIC_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_basic_func, (void*)i);
    }
    
    // Join threads and check results
    for (long i = 0; i < NUM_BASIC_THREADS; i++) {
        void* result;
        pthread_join(threads[i], &result);
        if ((long)result == 0) {
            success = 0;
        }
    }
    
    report_test("All threads correctly stored and retrieved their values", success);
}

// Test 3: Test stack variable storage
void test_stack_variable_storage() {
    printf("\n%s=== Test 3: Stack Variable Storage ===%s\n", YELLOW, RESET);
    
    // Initialize storage
    init_storage();
    
    // Allocate TLS for this thread
    tls_thread_alloc();
    
    // Create a stack variable and store its pointer
    int stack_var = 123;
    set_tls_data(&stack_var);
    
    // Get the pointer and verify it points to our stack variable
    void* data = get_tls_data();
    report_test("Stack variable correctly stored and retrieved", 
        data != NULL && *(int*)data == 123);
    
    // Change the stack variable and verify the change is reflected
    stack_var = 456;
    data = get_tls_data();
    report_test("Updated stack variable correctly retrieved", 
        data != NULL && *(int*)data == 456);
    
    // Clean up
    tls_thread_free();
}

// Test 4: Test heap variable storage
void test_heap_variable_storage() {
    printf("\n%s=== Test 4: Heap Variable Storage ===%s\n", YELLOW, RESET);
    
    // Initialize storage
    init_storage();
    
    // Allocate TLS for this thread
    tls_thread_alloc();
    
    // Create a heap variable and store its pointer
    int* heap_var = malloc(sizeof(int));
    *heap_var = 789;
    set_tls_data(heap_var);
    
    // Get the pointer and verify it points to our heap variable
    void* data = get_tls_data();
    report_test("Heap variable correctly stored and retrieved", 
        data != NULL && *(int*)data == 789);
    
    // Change the heap variable and verify the change is reflected
    *heap_var = 987;
    data = get_tls_data();
    report_test("Updated heap variable correctly retrieved", 
        data != NULL && *(int*)data == 987);
    
    // Clean up
    free(heap_var);
    tls_thread_free();
}

// Test 5: Struct data type
void test_struct_data_type() {
    printf("\n%s=== Test 5: Struct Data Type ===%s\n", YELLOW, RESET);
    
    // Initialize storage
    init_storage();
    
    // Allocate TLS for this thread
    tls_thread_alloc();
    
    // Create a struct and store its pointer
    test_data_t test_struct = {
        .value = 42,
        .another_value = 3.14159,
    };
    snprintf(test_struct.text, sizeof(test_struct.text), "Hello TLS");
    
    set_tls_data(&test_struct);
    
    // Get the pointer and verify it points to our struct
    void* data = get_tls_data();
    test_data_t* retrieved = (test_data_t*)data;
    
    int struct_correct = (
        retrieved != NULL && 
        retrieved->value == 42 &&
        retrieved->another_value == 3.14159 &&
        strcmp(retrieved->text, "Hello TLS") == 0
    );
    
    report_test("Struct correctly stored and retrieved", struct_correct);
    
    // Clean up
    tls_thread_free();
}

// Test 6: Stress test with many threads
void* thread_stress_func(void* arg) {
    long thread_num = (long)arg;
    
    // Allocate TLS for this thread
    tls_thread_alloc();
    
    // Set and verify data many times
    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        int value = thread_num * 1000 + i;
        set_tls_data(&value);
        
        void* data = get_tls_data();
        if (data == NULL || *(int*)data != value) {
            tls_thread_free();
            return (void*)0; // Test failed
        }
        
        // Short sleep to encourage thread switching
        if (i % 100 == 0) {
            usleep(1);
        }
    }
    
    // Free the TLS
    tls_thread_free();
    
    return (void*)1; // Test passed
}

void test_stress() {
    printf("\n%s=== Test 6: Stress Test with Many Threads ===%s\n", YELLOW, RESET);
    
    // Initialize storage
    init_storage();
    
    pthread_t threads[NUM_STRESS_THREADS];
    int success = 1;
    
    // Create threads
    for (long i = 0; i < NUM_STRESS_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_stress_func, (void*)i);
    }
    
    // Join threads and check results
    for (long i = 0; i < NUM_STRESS_THREADS; i++) {
        void* result;
        pthread_join(threads[i], &result);
        if ((long)result == 0) {
            success = 0;
        }
    }
    
    report_test("Stress test with many threads", success);
}

// Helper function for thread allocation in the reallocation test
void* thread_allocate_func(void* arg) {
    tls_thread_alloc();
    return NULL;
}

// Helper function for thread reallocation in the reallocation test
void* thread_realloc_func(void* arg) {
    long thread_num = (long)arg;
    
    tls_thread_alloc();
    int value = (int)thread_num;
    set_tls_data(&value);
    
    void* data = get_tls_data();
    if (data == NULL || *(int*)data != value) {
        return (void*)0; // Test failed
    }
    
    return (void*)1; // Test passed
}

// Test 7: Test re-allocation after free
void test_reallocation() {
    printf("\n%s=== Test 7: Re-allocation After Free ===%s\n", YELLOW, RESET);
    
    // Initialize storage
    init_storage();
    
    // First round: allocate all slots
    pthread_t threads[MAX_THREADS];
    
    for (long i = 0; i < MAX_THREADS; i++) {
        // Create a thread that just allocates a TLS slot
        pthread_create(&threads[i], NULL, thread_allocate_func, NULL);
        
        // Wait for it to finish
        pthread_join(threads[i], NULL);
    }
    
    // Free half the slots
    for (int i = 0; i < MAX_THREADS/2; i++) {
        // Overwrite thread_id directly in the array to simulate different threads freeing
        g_tls[i].thread_id = -1;
        g_tls[i].data = NULL;
    }
    
    // Second round: allocate half the slots again
    int success = 1;
    for (long i = 0; i < MAX_THREADS/2; i++) {
        // Create a thread that allocates a TLS slot
        pthread_create(&threads[i], NULL, thread_realloc_func, (void*)i);
        
        // Wait for it to finish and check result
        void* result;
        pthread_join(threads[i], &result);
        if ((long)result == 0) {
            success = 0;
        }
    }
    
    report_test("Re-allocation after free", success);
}

// Edge case test: Out of slots
void test_out_of_slots() {
    printf("\n%s=== Test 8: Out of Slots (should print error message) ===%s\n", YELLOW, RESET);
    
    // Initialize storage
    init_storage();
    
    printf("The following error is expected:\n");
    
    // 1. Fill all MAX_THREADS slots
    for (int i = 0; i < MAX_THREADS; i++) {
        g_tls[i].thread_id = i + 1; // Use fake thread IDs
        g_tls[i].data = NULL;
    }
    
    // 2. Try to allocate one more - this should print an error and exit
    // Since this will exit the program, we'll fork a child process to test it
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process: try to allocate
        tls_thread_alloc();
        // If we get here, the function didn't exit as expected
        exit(1);
    } else {
        // Parent process: wait for child to exit
        int status;
        waitpid(pid, &status, 0);
        // Check if child exited with the expected error code (1)
        report_test("Out of slots exits with code 1", WEXITSTATUS(status) == 1);
    }
    
    // Restore g_tls to a clean state
    init_storage();
}

// Edge case test: Uninitialized thread
void test_uninitialized_thread() {
    printf("\n%s=== Test 9: Uninitialized Thread (should print error message) ===%s\n", YELLOW, RESET);
    
    // Initialize storage
    init_storage();
    
    printf("The following error is expected:\n");
    
    // 1. Do not allocate TLS for the current thread
    
    // 2. Try to get data - this should print an error and exit
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process: try to get data
        get_tls_data();
        // If we get here, the function didn't exit as expected
        exit(1);
    } else {
        // Parent process: wait for child to exit
        int status;
        waitpid(pid, &status, 0);
        // Check if child exited with the expected error code (2)
        report_test("Uninitialized thread get exits with code 2", WEXITSTATUS(status) == 2);
    }
    
    // Try with set_tls_data
    pid = fork();
    
    if (pid == 0) {
        // Child process: try to set data
        int value = 42;
        set_tls_data(&value);
        // If we get here, the function didn't exit as expected
        exit(1);
    } else {
        // Parent process: wait for child to exit
        int status;
        waitpid(pid, &status, 0);
        // Check if child exited with the expected error code (2)
        report_test("Uninitialized thread set exits with code 2", WEXITSTATUS(status) == 2);
    }
}

int main() {
    printf("%s=== Thread Local Storage Tests ===%s\n", YELLOW, RESET);
    
    // Run tests
    test_basic_functionality();
    test_multiple_threads_basic();
    test_stack_variable_storage();
    test_heap_variable_storage();
    test_struct_data_type();
    test_stress();
    test_reallocation();
    test_out_of_slots();
    test_uninitialized_thread();
    
    // Final report
    printf("\n%s=== Test Summary ===%s\n", YELLOW, RESET);
    if (all_tests_passed) {
        printf("%sAll tests PASSED!%s\n", GREEN, RESET);
    } else {
        printf("%sSome tests FAILED. See above for details.%s\n", RED, RESET);
    }
    
    return all_tests_passed ? 0 : 1;
}