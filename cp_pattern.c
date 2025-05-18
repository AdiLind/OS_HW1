#include "cp_pattern.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include "cond_var.h"
#include "queue.h"

#define DEBUG 0
#define DEBUG_PRINT(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, "[DEBUG] " fmt, ##__VA_ARGS__); } while (0)

#define MAX_NUMBER 1000000

// global vars
static queue_t shared_consumer_queue; // Shared queue between producers and consumers
static atomic_bool is_all_producers_done = false;
static atomic_bool is_all_numbers_produced = false;
static condition_variable production_complete_signal; //signaling when we produce all numbers
static atomic_flag sync_print_lock = ATOMIC_FLAG_INIT;
static bool* generated_numbers; // indicate is this number has been generated
static pthread_t* producer_threads_IDs;
static pthread_t* consumer_threads_IDs; 
static atomic_bool stop_consumers_flag = false; 
static int num_producers;
static int num_consumers;
static atomic_int next_number_to_generate = ATOMIC_VAR_INIT(0);
static atomic_int num_generated = ATOMIC_VAR_INIT(0);

//////////// static functions  /////////////////
static void* producer_thread(void* arg)
{
    long thread_id = (long)arg;
    int max_number = MAX_NUMBER; // 999,999
    int total_numbers = MAX_NUMBER + 1;  

    while (!atomic_load(&is_all_numbers_produced)) {
        //the next number to generate
        int num = atomic_fetch_add(&next_number_to_generate, 1);
        
        //we've reached the limit -> mark as done 
        if (num >= max_number) {
            atomic_store(&is_all_numbers_produced, true);
            condition_variable_broadcast(&production_complete_signal);
            break;
        }
        
        int count = atomic_fetch_add(&num_generated, 1) + 1;
        if (count >= total_numbers) {
            atomic_store(&is_all_numbers_produced, true);
            condition_variable_broadcast(&production_complete_signal);
            break;
        }
        
        // We found a new number, print and enqueue
        char msg[100];
        snprintf(msg, sizeof(msg), "Producer %ld produced number %d", thread_id, num);
        print_msg(msg);
        enqueue(&shared_consumer_queue, num);
    }
    
    DEBUG_PRINT("Producer %ld exiting\n", thread_id);
    return NULL;
}

static void* consumer_thread (void* arg){
    long thread_id = (long) arg;
    while(!atomic_load(&stop_consumers_flag)) {
        bool all_producers_done = atomic_load(&is_all_producers_done);
        int num = dequeue(&shared_consumer_queue, &all_producers_done, &stop_consumers_flag);

        if(num == -1 || atomic_load(&stop_consumers_flag)) {
            break; // no more items
        }

        bool is_divisible_by_6 = (num % 6 == 0);

        // we got a number from the queue then we will print it
        char msg[100];
        snprintf(msg, sizeof(msg), "Consumer %ld checked %d. Is it divisible by 6? %s", 
                 thread_id, num, is_divisible_by_6 ? "True" : "False");
        print_msg(msg);
    }
    return NULL;
}



/*
 * TODO: Implement start_consumers_producers.
 * This function should:
 *  - Print the configuration (number of consumers, producers, seed).
 *  - Seed the random number generator using srand().
 *  - Create producer and consumer threads.
 */
void start_consumers_producers(int consumers, int producers, int seed) {
    // TODO: Print configuration and start threads.
    printf("Number of consumers: %d\n", consumers);
    printf("Number of producers: %d\n", producers);
    printf("Seed: %d\n", seed);

    num_consumers = consumers;
    num_producers = producers;
    srand(seed);
    queue_init(&shared_consumer_queue);
    condition_variable_init(&production_complete_signal);
    // intialize the objects
    generated_numbers = (bool*)malloc(QUEUE_SIZE * sizeof(bool));
    producer_threads_IDs = (pthread_t*)malloc(num_producers * sizeof(pthread_t));
    consumer_threads_IDs = (pthread_t*)malloc(num_consumers * sizeof(pthread_t));

    // check if all the object created successfully
    if(!generated_numbers || !producer_threads_IDs || !consumer_threads_IDs) {
        free(producer_threads_IDs);
        free(consumer_threads_IDs);
        free(generated_numbers);
        fprintf(stderr,"Failed to allocate memory for an object, exiting...\n");
        exit(1);
    }

    //reset flags
    atomic_store(&is_all_producers_done, false);
    atomic_store(&is_all_numbers_produced, false);
    atomic_store(&stop_consumers_flag, false);
    atomic_store(&next_number_to_generate, 0);
    atomic_store(&num_generated, 0);

    //creating the producer threads
    for(int i = 0; i < producers; i++) {
        pthread_create(&producer_threads_IDs[i], NULL, producer_thread, (void*)(long)i);
    }
    //creating the consumer threads
    for(int i = 0; i < consumers; i++) {
        pthread_create(&consumer_threads_IDs[i], NULL, consumer_thread, (void*)(long)i);
    }
    
}

/*
 * TODO: Implement stop_consumers to stop all consumers threads.
 */
void stop_consumers() {
    // TODO: Stop the consumer thread with the given id.
    atomic_store(&stop_consumers_flag, true); //signal to stop consumers
    queue_broadcast_not_empty(&shared_consumer_queue); // wake up all consumers

    for(int i = 0; i < num_consumers; i++) {
        DEBUG_PRINT("Joining consumer thread %d...\n", i);
        pthread_join(consumer_threads_IDs[i], NULL); 
        DEBUG_PRINT("Consumer thread %d joined\n", i);
    }

    free(consumer_threads_IDs);
    free(producer_threads_IDs);
    free(generated_numbers);
    DEBUG_PRINT("All consumers stopped\n");
}


void print_msg(const char* msg) {

    while (atomic_flag_test_and_set(&sync_print_lock)) {
        sched_yield();
    }

    printf("%s\n", msg);
    atomic_flag_clear(&sync_print_lock);
}

/*
 * TODO: Implement wait_until_producers_produced_all_numbers 
 * The function should wait until all numbers between 0 and 1,000,000 have been produced.
 */
void wait_until_producers_produced_all_numbers() {
    // TODO: Wait until production of numbers (0 to 1,000,000) is complete.

    DEBUG_PRINT("Waiting for producers to complete...\n");

    ticket_lock condition_var_lock;
    atomic_init(&condition_var_lock.current_ticket, 0);
    atomic_init(&condition_var_lock.ticket, 0);

    // acquire the lock
    int ticket = atomic_fetch_add(&condition_var_lock.ticket, 1);
    while (atomic_load(&condition_var_lock.current_ticket) != ticket) {
        sched_yield();
    }

    // wait until all numbers are produced
    while (!atomic_load(&is_all_numbers_produced)) {
        DEBUG_PRINT("Still waiting for producers to finish production...\n");
        condition_variable_wait(&production_complete_signal, &condition_var_lock);
    }

    // mark the end of production
    atomic_store(&is_all_producers_done, true);
    atomic_fetch_add(&condition_var_lock.current_ticket, 1); // release the lock

    DEBUG_PRINT("Joining producer threads...\n");
    // wait until all producers are done
    for(int i = 0; i < num_producers; i++) {
        DEBUG_PRINT("Joining producer thread %d...\n", i);  // Debug print for each thread
        pthread_join(producer_threads_IDs[i], NULL);
        DEBUG_PRINT("Producer thread %d joined\n", i);  // Debug print after join
    }
    DEBUG_PRINT("All producers completed\n");
}

/*
 * TODO: Implement wait_consumers_queue_empty to wait until queue is empty, 
 * if queue is already empty - return immediately without waiting.
 */
void wait_consumers_queue_empty() {
    // TODO: Return non-zero if the consumer queue is empty.
    if(is_queue_is_empty(&shared_consumer_queue)) {
        return;
    }

    // wait until the queue is empty
    while (!is_queue_is_empty(&shared_consumer_queue)) {
        sched_yield();
    }
}

/*
 * TODO: Implement a main function that controls the producer-consumer process
 */
int main(int argc, char* argv[]) {
    // TODO: Parse arguments.
    // TODO: Start producer-consumer process.
    // TODO: Wait for threads to finish and clean up resources.

    //validate the input
    if(argc != 4) {
        fprintf(stderr, "Usage: %s <num_consumers> <num_producers> <seed>\n", argv[0]);
        return 1;
    }

    //parse the input
    int num_consumers = atoi(argv[1]);
    int num_producers = atoi(argv[2]);
    int seed = atoi(argv[3]);

    //start the producer-consumer process
    start_consumers_producers(num_consumers, num_producers, seed);
    wait_until_producers_produced_all_numbers();
    wait_consumers_queue_empty();
    stop_consumers();

    return 0;
}
