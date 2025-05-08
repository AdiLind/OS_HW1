#include "tl_semaphore.h"
#include <sched.h>

/*
 * TODO: Implement semaphore_init for the Ticket Lock semaphore.
 */
void semaphore_init(semaphore* sem, int initial_value) {
    // TODO: Define the structure and initialize the semaphore.
    atomic_init(&sem->value, initial_value);
    atomic_init(&sem->ticket, 0);
    atomic_init(&sem->cur_ticket, 0);
    
}

/*
 * TODO: Implement semaphore_wait using the Ticket Lock mechanism.
 */


 
 void semaphore_wait(semaphore* sem) {
    // TODO: Obtain a ticket and wait until it is your turn; then decrement the semaphore value.
    int my_ticket = atomic_fetch_add(&sem->ticket, 1);
    
    // Wait until it's my turn
    while (atomic_load(&sem->cur_ticket) != my_ticket) {
        sched_yield();
    }
    
    // Now it's my turn to try to decrement the semaphore
    // Wait until the value is positive
    while (1) {
        int expected = atomic_load(&sem->value);
        if (expected > 0) {
            // Try to decrement the value
            if (atomic_compare_exchange_strong(&sem->value, &expected, expected - 1)) {
                // Successfully decremented, release the ticket lock and return
                atomic_fetch_add(&sem->cur_ticket, 1);
                return;
            }
        }else {
            // Value is <= 0, release the ticket lock and get a new ticket
            atomic_fetch_add(&sem->cur_ticket, 1);
            
            // Wait for value to become positive before getting a new ticket
            while (atomic_load(&sem->value) <= 0) {
                sched_yield();
            }
            
            // Get a new ticket
            my_ticket = atomic_fetch_add(&sem->ticket, 1);
            
            // Wait for my turn again
            while (atomic_load(&sem->cur_ticket) != my_ticket) {
                sched_yield();
            }
            
        }
    }
 }


 

/*
 * TODO: Implement semaphore_signal using the Ticket Lock mechanism.
 */
void semaphore_signal(semaphore* sem) {
    // TODO: Increment the semaphore value and allow the next ticket holder to proceed.
    int my_ticket = atomic_fetch_add(&sem->ticket, 1);

    //wait for my turn
    while(atomic_load(&sem->cur_ticket) != my_ticket)
    {
        sched_yield();
    }
    atomic_fetch_add(&sem->value, 1); 
    atomic_fetch_add(&sem->cur_ticket, 1); // release the lock

}
