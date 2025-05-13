#include "rw_lock.h"
#include <sched.h>
#include <stdatomic.h>

/*
 * TODO: Implement rwlock_init.
 */
void rwlock_init(rwlock* lock) {
    atomic_init(&lock->readers_count, 0);
    atomic_init(&lock->is_writing, 0);
    atomic_init(&lock->waiting_writers, 0);
    atomic_flag_clear(&lock->lock);
}


// Allow multiple readers while ensuring no writer is active in the same time.
void rwlock_acquire_read(rwlock* lock) {
    
    //spin until the lock is free - acquire the lock
    while (atomic_flag_test_and_set(&lock->lock)) {
        sched_yield(); 
    }

    /*if there is a writer waiting or active, we need to wait for it to finish
    * we will let the writer priority over the readers in order to avoid starvation*/
    while(atomic_load(&lock->waiting_writers) > 0 || atomic_load(&lock->is_writing)) {
        atomic_flag_clear(&lock->lock); // release the lock
        sched_yield(); // yield to other threads
        // try to acquire the lock again
        while (atomic_flag_test_and_set(&lock->lock)) {
            sched_yield(); 
        }
    }

    // Increment the reader count
    atomic_fetch_add(&lock->readers_count, 1);
    atomic_flag_clear(&lock->lock); // release the lock

}

// Release the read lock and decrement the reader count
void rwlock_release_read(rwlock* lock) {
    
    atomic_fetch_sub(&lock->readers_count, 1);
}

// acquire the write lock which ensures exclusive access to the resource
void rwlock_acquire_write(rwlock* lock) {
    // increment the waiting writers count
    atomic_fetch_add(&lock->waiting_writers, 1);
    // spin until the lock is free - acquire the lock
    while (atomic_flag_test_and_set(&lock->lock)) {
        sched_yield(); 
    }

    // wait until there are no readers or writers
    while(atomic_load(&lock->readers_count) > 0 || atomic_load(&lock->is_writing))
    {
        // release the lock
        atomic_flag_clear(&lock->lock);
        sched_yield(); // yield to other threads
        // reacquire the lock again
        while (atomic_flag_test_and_set(&lock->lock)) {
            sched_yield(); 
        }
    }
    atomic_store(&lock->is_writing, 1); // set the write lock there is active writer
    atomic_fetch_sub(&lock->waiting_writers, 1); // decrement the waiting writers count
    atomic_flag_clear(&lock-> lock); // release the lock

}


void rwlock_release_write(rwlock* lock) {
    // TODO: Release the write lock.
    atomic_store(&lock->is_writing, 0); // release the write lock
}
