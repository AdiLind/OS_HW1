#include "rw_lock.h"

/*
 * TODO: Implement rwlock_init.
 */
void rwlock_init(rwlock* lock) {
    // TODO: Initialize the lock structure.
}

/*
 * TODO: Implement rwlock_acquire_read.
 */
void rwlock_acquire_read(rwlock* lock) {
    // TODO: Allow multiple readers while ensuring no writer is active.
}

/*
 * TODO: Implement rwlock_release_read.
 */
void rwlock_release_read(rwlock* lock) {
    // TODO: Decrement the reader count.
}

/*
 * TODO: Implement rwlock_acquire_write.
 */
void rwlock_acquire_write(rwlock* lock) {
    // TODO: Ensure exclusive access for writing.
}

/*
 * TODO: Implement rwlock_release_write.
 */
void rwlock_release_write(rwlock* lock) {
    // TODO: Release the write lock.
}
