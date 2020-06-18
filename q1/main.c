#include <stdlib.h>                                                                                          
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#include "monte_carlo.h"
#include "op_timer.h"

#define SLEEP_TIME 0.1*1e6
#define MAX_QUEUE 2
#define MAX_WORKER 10
#define REQ_QUEUE "/req_queue"
#define RES_QUEUE "/res_queue"

#define N_RANGES 5
#define N_FUNCTIONS 4
#define N_TASKS (N_RANGES * N_FUNCTIONS)
#define N_POINTS 5000000

static int is_sigusr1 = 0;

/* Function to set the is_interrupted flag.
 * This should be called when an interrupt signal is sent
 */
void sigint_received(int signum) {
    switch (signum) {
    case SIGUSR1:
        //        printf("Received SIGUSR1\n");
        is_sigusr1 = 1;
        break;
    }
}

// message structure for request
typedef struct message_request {
    int id;
    double min;
    double max;
    double (*function)(double);
} REQ;

// message structure for response
typedef struct message_response {
    int id;
    double result;
    double time;
} RES;

/* Structure to represent a task - the integration of a function over a range */
typedef struct compute_task_struct {
    int id;
    double x_min;
    double x_max;
    double (*function)(double);
    double result;
} compute_task;

/* Function: f1
 * Returns f1(x) = cos(x)
 */
static double f1(double x) {
    return cos(x);
}

/* Function: f2
 * Returns f2(x) = x^2 + 2x + 1
 */
static double f2(double x) {
    return x*x + 2*x + 1;
}

/* Function: f3
 * Returns f3(x) = 3
 */
static double f3(double x) {
    return 3;
}

/* Function: f4
 * Returns f4(x) = 10 - x
 */
static double f4(double x) {
    return 10 - x;
}

int main(void)
{
    int i, j, n_workers;
    pid_t pid[MAX_WORKER];
    double compute_times[MAX_WORKER] = {0};
    double x_min[N_RANGES] = { 0, 0, 0, 1, 0 };
    double x_max[N_RANGES] = { 1, 2, 3, 10, M_PI };
    double (*functions[N_FUNCTIONS])(double) = { f1, f2, f3, f4 };
    
    /* Set up interrupt handler */
    struct sigaction sigint_handler;
    
    sigint_handler.sa_handler = sigint_received;
    sigemptyset(&sigint_handler.sa_mask);
    sigint_handler.sa_flags = 0;
    
    if (sigaction(SIGUSR1, &sigint_handler, NULL) != 0) {
        fprintf(stderr, "ERROR: fail to detect signal SIGUSR1\n");
        exit(EXIT_FAILURE);
    }
    
    /* Let's create an array of tasks to distribute over threads.
     * Each entry respresents integrating one function over one interval.
     */
    compute_task tasks[N_TASKS];
    
    /* Fill the task array with each combination of range and function */
    for (i = 0; i < N_RANGES; i++) {
        for (j = 0; j < N_FUNCTIONS; j++) {
            int task_id = i * N_FUNCTIONS + j;
            tasks[task_id].id = task_id;
            tasks[task_id].x_min = x_min[i];
            tasks[task_id].x_max = x_max[i];
            tasks[task_id].function = functions[j];
        }
    }
    
    // create message queue
    mqd_t mq_request, mq_response; 
    struct mq_attr attr;
    
    // buffer for message struct
    REQ msg_request;
    RES msg_response;
    
    attr.mq_flags = 0;
    attr.mq_curmsgs = 0;
    attr.mq_maxmsg = MAX_QUEUE;
    
    // clean up if exit
    mq_unlink(REQ_QUEUE);
    mq_unlink(RES_QUEUE);
    
    // open message queue
    attr.mq_msgsize = sizeof (struct message_request);
    mq_request = mq_open(REQ_QUEUE, O_CREAT | O_WRONLY | O_NONBLOCK, S_IRUSR | S_IWUSR, &attr);
    if (mq_request == -1) {
        perror("fail to open request message queue");
        exit(EXIT_FAILURE);
    }
    
    // open response message queue to read 
    attr.mq_msgsize = sizeof (struct message_response);
    mq_response = mq_open(RES_QUEUE, O_CREAT | O_RDONLY | O_NONBLOCK, S_IRUSR | S_IWUSR, &attr);
    if (mq_response == -1) {
        perror("fail to open response message queue");
        exit(EXIT_FAILURE);
    }
    
    /* For each number of worker process n to be tested, compute the integrals, and
     * print their results
     */
    for (n_workers = 1; n_workers <= MAX_WORKER; n_workers++) {
        timer *thread_timer;
        double time_taken;
        
        printf("Threads: %d\n", n_workers);
        
        // fork all the child and set to listen to message queue
        for (int var = 0; var < n_workers; ++var) {
            //            fflush(stdout);
            pid[var] = fork();
            if (pid[var] == -1) {
                perror("fail to gork");
                exit(EXIT_FAILURE);
            }
            if (pid[var] == 0) {
                // child...
                // open request message queue to read
                mqd_t mq_request_child = mq_open(REQ_QUEUE, O_RDONLY | O_NONBLOCK);
                if (mq_request_child == -1) {
                    perror("fail to open request message queue");
                    exit(EXIT_FAILURE);
                }
                
                // open response message queue to write
                mqd_t mq_response_child = mq_open(RES_QUEUE, O_WRONLY | O_NONBLOCK);
                if (mq_response_child == -1) {
                    perror("fail to open response message queue");
                    exit(EXIT_FAILURE);
                }
                
                // listen to request
                REQ msg_request_child;
                RES msg_response_child;
                
                while (1) {
                    ssize_t read = mq_receive(mq_request_child, (char *) &msg_request_child, sizeof (struct message_request), NULL);
                    if(read == -1) {
                        usleep(SLEEP_TIME); // time delay
                        //                        perror("child read fail");
                    }
                    if (read > 0 && msg_request_child.min < msg_request_child.max) {
                        // calculate and timing
                        thread_timer = create_timer();
                        if (thread_timer == NULL) {
                            perror("Error creating timer");
                            exit(EXIT_FAILURE);
                        }
                        
                        // integrate
                        double result = mc_integrate_1d(msg_request_child.function, N_POINTS, msg_request_child.min, msg_request_child.max);
                        
                        time_taken = timer_check(thread_timer);
                        if ((int)time_taken == -1) {
                            perror("Error reading timer");
                            exit(EXIT_FAILURE);
                        }
                        
                        // assamble result and send
                        msg_response_child.id = msg_request_child.id;
                        msg_response_child.result = result;
                        msg_response_child.time = time_taken;
                        while (mq_send(mq_response_child, (char *)&msg_response_child, sizeof (struct message_response), 0) == -1) {
                            usleep(SLEEP_TIME); // time delay
                        }
                        
                        // clean up
                        free(thread_timer);
                        
                        if (is_sigusr1 == 1) {
                            mq_close(mq_request_child);
                            mq_close(mq_response_child);
                            exit(EXIT_SUCCESS);
                        }
                    }
                }
            }
        }
        // parent...
        int task_count = 0;
        int response_count = 0;
        while (1) {
            // listen to response message
            ssize_t read = mq_receive(mq_response, (char *)&msg_response, sizeof (struct message_response), NULL);
            if (read == -1) {
                //                perror("main read fail");
            }
            if (read > 0) {
                compute_times[n_workers - 1] += msg_response.time;
                tasks[msg_response.id].result = msg_response.result;
                response_count++;
            }
            
            // assign request message 
            if (task_count < N_TASKS) {
                msg_request.id = tasks[task_count].id;
                msg_request.function = tasks[task_count].function;
                msg_request.min = tasks[task_count].x_min;
                msg_request.max = tasks[task_count].x_max;
                
                // send request message
                if (mq_send(mq_request, (char *) &msg_request, sizeof (struct message_request), 0) == -1) {
                    //                                    perror("main send fail");
                }
                else {
                    task_count++;
                }
            }
            usleep(SLEEP_TIME); // time delay
            
            // kill all child after all task been done
            if (response_count >= N_TASKS) {
                for (int c = 0; c < n_workers; c++) {
                    kill(pid[c], SIGUSR1);
                }
                //                kill(-1, SIGUSR1);
                if (waitpid(-1, NULL, WNOHANG) == -1){
                    perror("child fail");
                }
                // reset signal flag for next loop
                is_sigusr1 = 0;
                break;
            }
        }
        /* Print the table of results, as comma-separated data */
        printf("f1,f2,f3,f4\n");
        for (i = 0; i < N_RANGES; i++) {
            for (j = 0; j < N_FUNCTIONS; j++) {
                int task_id = i * N_FUNCTIONS + j;
                double result = tasks[task_id].result;
                printf("%lf%c", result, j == N_FUNCTIONS-1 ? '\n' : ',');
            }
        }
    }
    
    
    /* Now, print out the table of time taken when computing the integrals
     * for each number of threads.
     */
    printf("Threads,Time\n");
    
    for (n_workers = 1; n_workers <= MAX_WORKER; n_workers++)
        printf("%d,%lf\n", n_workers, compute_times[n_workers-1]);
    
    // clean up
    if (wait(NULL) == -1) {
        perror("fail to wait child");
        exit(EXIT_FAILURE);
    }
    mq_close(mq_request);
    mq_close(mq_response);
    mq_unlink(REQ_QUEUE);
    mq_unlink(RES_QUEUE);
    
    exit(EXIT_SUCCESS);
}
