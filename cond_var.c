#include "cond_var.h"
#include <sched.h>

/*
 * TODO: Implement condition_variable_init.
 */
void condition_variable_init(condition_variable* cv) {
    atomic_init(&cv->waiters_counter, 0);
    atomic_flag_clear(&cv->internal_mutex_lock);
    atomic_init(&cv->signal_sequence, 0);
}

/*
 * TODO: Implement condition_variable_wait.
 */
void condition_variable_wait(condition_variable* cv, ticket_lock* ext_lock) {
    // TODO: Increase waiter count, release ext_lock, wait until signaled, then reacquire ext_lock.

    //get the current sequence number
    while(atomic_flag_test_and_set(&cv->internal_mutex_lock)); // Acquire the internal mutex lock
    {
        sched_yield();
    }
    int my_seq = atomic_load(&cv->signal_sequence); // Get the current sequence number
    
    atomic_fetch_add(&cv->waiters_counter, 1); // Increment the waiters counter
    atomic_flag_clear(&cv ->internal_mutex_lock); 

    atomic_fetch_add(&ext_lock->current_ticket, 1); // Release the external lock

    //wait until sequence number isnt equal to the current sequence number
    int spin_count = 0;
    while(atomic_load(&cv->signal_sequence) == my_seq)
    {
        if ( spin_count++ > 1000) {
            sched_yield(); // Yield the CPU to other threads
            spin_count = 0; 
        }
    }

    // Reacquire the external lock
    int my_ticket = atomic_fetch_add(&ext_lock->ticket, 1);

    // Busy wait until it's my turn
    while(atomic_load(&ext_lock->current_ticket) != my_ticket) {
        sched_yield(); 
    }
}

/*
 * TODO: Implement condition_variable_signal.
 */
void condition_variable_signal(condition_variable* cv) {
    // TODO: Signal one waiting thread.

    // acquire the lock
    while(atomic_flag_test_and_set(&cv->internal_mutex_lock)); 
    {
        sched_yield();
    }

    // signal one thread
    if(atomic_load(&cv->waiters_counter) >0)
    {
        atomic_fetch_sub(&cv->waiters_counter, 1); // Decrement the waiters counter
        atomic_fetch_add(&cv->signal_sequence, 1); // Increment the signal sequence
    }
    atomic_flag_clear(&cv->internal_mutex_lock); // Release the lock

}

/*
 * TODO: Implement condition_variable_broadcast.
 */
void condition_variable_broadcast(condition_variable* cv) {
    // TODO: Signal all waiting threads.

    // acquire the lock
    while(atomic_flag_test_and_set(&cv->internal_mutex_lock)); 
    {
        sched_yield();
    }
    // signal all threads - broadcast
    if(atomic_load(&cv->waiters_counter) > 0)
    {
        int waiters = atomic_load(&cv->waiters_counter);
        atomic_store(&cv->waiters_counter, 0); // Reset the waiters counter
        atomic_fetch_add(&cv->signal_sequence, 1); // Increment the signal sequence in order to wake up all threads
    }

    atomic_flag_clear(&cv->internal_mutex_lock); // Release the lock

}
