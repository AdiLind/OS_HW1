#include "tas_semaphore.h"
#include <stdatomic.h>
#include <sched.h>


typedef struct {
    atomic_int value;     
    atomic_flag lock;     
} semaphore;


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
    while(1) {
        // busy wait until the lock is acquired
        while(atomic_load(&sem->lock) <= 0) {
            // spin until the lock is free
            sched_yield(); 
        }

        while (atomic_flag_test_and_set(&sem->lock)) {
            // spin until the lock is acquired
            sched_yield(); 
        }
        // Critical section
        if(atomic_load(&sem->value) > 0) {
            atomic_fetch_sub(&sem->value, 1); // Decrement the semaphore value
            atomic_flag_clear(&sem->lock); // Release the lock
            return; // Exit the function
        }  
        
        atomic_flag_clear(&sem->lock);
        sched_yield();         
    }
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
