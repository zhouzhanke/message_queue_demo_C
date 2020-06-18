
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "op_timer.h"

/* Function: create_timer
 * Creates a timer object and returns it.
 * Returns NULL on failure.
 */
timer *create_timer() {
    int status;

    /* Allocate a timer object. */
    timer *t = (timer *) malloc(sizeof(struct timespec));
    if (t == NULL)
        return NULL;

    /* Get the current time.  CLOCK_REALTIME is only guaranteed clock */
    status = clock_gettime(CLOCK_REALTIME, t);
    if (status == -1) {
        free(t);
        return NULL;
        }

    return t;
    }

/* Function: timer_check
 * Returns number of milliseconds since t was last checked (or created if
 * it has never been checked yet.
 * Returns -1 if t==NULL or other error occurs
 */
double timer_check(timer *t) {
    timer new_time;
    double difference;

    /* get the new time */
    int status = clock_gettime(CLOCK_REALTIME, &new_time);
    if (status == -1) 
        return -1;

    /* Determine the difference since the last timer value in milliseconds. */
    difference = ((double) (new_time.tv_sec - t->tv_sec) * 1000) + 
                 (((double) (new_time.tv_nsec - t->tv_nsec)) / 1000000);

    /* Replace the timer value with the new one */
    memcpy(t, &new_time, sizeof(timer));

    return difference;
    }
    
