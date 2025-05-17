#include "local_storage.h"
#include <stdio.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

void acquire_lock(void);
void release_lock(void);


//Global Vars
tls_data_t g_tls[MAX_THREADS];
atomic_flag g_tls_lock = ATOMIC_FLAG_INIT; 

void init_storage(void) {
    
    acquire_lock();
    for(int i =0; i < MAX_THREADS; i++) {
        g_tls[i].thread_id = -1;
        g_tls[i].data = NULL;
    }
    release_lock();
} 

void tls_thread_alloc(void) {
    int64_t thread_id = pthread_self();
    acquire_lock();

    // check if the thread already has an entry
    for(int i = 0; i < MAX_THREADS; i++) {
        if(g_tls[i].thread_id == thread_id) {
            release_lock();
            return; // already allocated
        }
    }
    // find the empty entry
    int index_of_empty_slot = -1;
    for(int i = 0; i < MAX_THREADS; i++) {
        if(g_tls[i].thread_id == -1) {
            index_of_empty_slot = i;
            break;
        }
    }

    if(index_of_empty_slot == -1) {
        printf("thread [%ld] failed to initialize, not enough space\n", thread_id);
        release_lock();
        exit(1);
    }

    // allocate the entry
    g_tls[index_of_empty_slot].thread_id = thread_id;
    release_lock();
}

void* get_tls_data(void) {
    int64_t thread_id = pthread_self();
    void* result = NULL;

    acquire_lock();

    // Search for the thread's entry
    int found_entry = 0;

    for(int i = 0; i < MAX_THREADS; i++) {
        if(g_tls[i].thread_id == thread_id) {
            result = g_tls[i].data;
            found_entry = 1;
            break;
        }
    }

    release_lock();

    if (!found_entry) {
        printf("thread [%ld] hasn't been initialized in the TLS\n", thread_id);
        exit(2);
    }
    
    return result;
}


void set_tls_data(void* data) {
    int64_t thread_id = pthread_self();

    acquire_lock();

    int isFound = 0;
    for(int i=0; i < MAX_THREADS; i++) {
        if (g_tls[i].thread_id == thread_id) {
            g_tls[i].data = data;
            isFound = 1;
            break;
        }
    }

    release_lock();
    if (!isFound) {
        printf("thread [%ld] hasn't been initialized in the TLS\n", thread_id);
        exit(2);
    }
}

void tls_thread_free(void) {
    int64_t thread_id = pthread_self();

    acquire_lock();

    for(int i = 0; i < MAX_THREADS; i++) {
        if(g_tls[i].thread_id == thread_id) {
            g_tls[i].thread_id = -1;
            g_tls[i].data = NULL;
            break;
        }
    }
    release_lock();
}

//helper functions

void acquire_lock() {
    while (atomic_flag_test_and_set(&g_tls_lock)) {
        sched_yield();
    }
}

void release_lock() {
    atomic_flag_clear(&g_tls_lock);
}
