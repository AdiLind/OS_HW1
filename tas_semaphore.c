#include "tas_semaphore.h"
#include <stdatomic.h>
#include <sched.h>



/*
 * TODO: Implement semaphore_init using a TAS spinlock.
 */
void semaphore_init(semaphore* sem, int initial_value) {
    atomic_store(&sem->value, initial_value);
    atomic_flag_clear(&sem->lock); // Initialize the lock to unlocked state
}

/*
 * TODO: Implement semaphore_wait using the TAS spinlock mechanism.
 */
void semaphore_wait(semaphore* sem) {
    int expected;
    
    // Keep trying until we successfully decrement a positive value
    do {
        // Wait until the value is positive
        while (atomic_load(&sem->value) <= 0) {
            sched_yield(); // Give other threads a chance to run
        }
        
        // Try to acquire the spinlock
        while (atomic_flag_test_and_set(&sem->lock)) {
            sched_yield(); 
        }
        
        // Check if value is still positive
        expected = atomic_load(&sem->value);
        if (expected > 0) {
            atomic_store(&sem->value, expected - 1);
            atomic_flag_clear(&sem->lock);
            return; // Successfully decremented, we can return
        }
        
        // Value is not positive, release lock and try again
        atomic_flag_clear(&sem->lock);
    } while (1);
}

/*
 * TODO: Implement semaphore_signal using the TAS spinlock mechanism.
 */
void semaphore_signal(semaphore* sem) {
    // TODO: Acquire the spinlock, increment the semaphore value, then release the spinlock.
    while(atomic_flag_test_and_set(&sem->lock)) {
        // spin until the lock is acquired
        sched_yield(); 
    }
    // Critical section
    atomic_fetch_add(&sem->value, 1);
    atomic_flag_clear(&sem->lock); 
}
