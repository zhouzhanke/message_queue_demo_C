#ifndef _OP_TIMER_H_
#define _OP_TIMER_H_

/* timer is just an alias for a timespec value */
typedef struct timespec timer;

/* Function: create_timer
 * Creates a timer object and returns it.
 * Returns NULL on failure.
 */
timer *create_timer();

/* Function: timer_check
 * Returns number of milliseconds since t was last checked (or created if
 * it has never been checked yet.
 * Returns -1 if t==NULL
 */
double timer_check(timer *t);

#endif

