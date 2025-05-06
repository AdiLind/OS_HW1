#ifndef CP_PATTERN_H
#define CP_PATTERN_H

/*
 * Starts the consumers and producers, and passes the seed to the producers.
 */
void start_consumers_producers(int consumers, int producers, int seed);

/*
 * Stops the consumers.
 */
void stop_consumers();

/*
 * Prints a message synchronously to avoid overlapping output.
 */
void print_msg(const char* msg);

/*
 * Waits until all numbers between 0 and 1,000,000 have been produced.
 */
void wait_until_producers_produced_all_numbers();

/*
 * wait until queue is empty, if queue is already empty - return immediately without waiting.
 */
void  wait_consumers_queue_empty();

#endif // CP_PATTERN_H
