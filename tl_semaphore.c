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
void semaphore_wait(semaphore* sem) {
    // TODO: Obtain a ticket and wait until it is your turn; then decrement the semaphore value.
    while(1)
    {
        // wait until we get a positive value before we try to acquire the lock(get a ticket)
        while(atomic_load(&sem->value) <= 0)
        {
            // spin wait
            sched_yield();
        }
        // get a ticket
        int my_ticket = atomic_fetch_add(&sem->ticket, 1);
        // wait until i get my turn
        while(atomic_load(&sem->cur_ticket) != my_ticket)
        {
            sched_yield();
        }

        int curr_ticket_val = atomic_load(&sem->cur_ticket);
        // we have our turn, decrement the semaphore value if posible
        if(curr_ticket_val > 0)
        {
            atomic_store(&sem->value, curr_ticket_val - 1);
            atomic_fetch_add(&sem->cur_ticket, 1); // לשחרר את שולי (לשחרר את המנעול)
            return;
        }
        else{
            atomic_fetch_add(&sem->cur_ticket, 1);
        }
        
    }
}
*/

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
