#define _GNU_SOURCE             /* Must precede #include <sched.h> for sched_setaffinity */ 
#define __USE_MISC

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <sched.h> //sched_setscheduler
#include <pthread.h>
#include <errno.h>
#include <signal.h> // Timers
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <math.h>
#include "fft/fft.h"

/* ***********************************************
* App specific defines
* ***********************************************/
#define NS_IN_SEC 1000000000L

#define DEFAULT_PRIO 50				// Default (fixed) thread priority  

#define THREAD_INIT_OFFSET 1000000	// Initial offset (i.e. delay) of rt thread

/* ***********************************************
* Prototypes
* ***********************************************/
typedef struct {
    uint32_t * const buffer;
    int head;
    int tail;
    const int maxlen;
} cab_buf;
int cab_buf_push(cab_buf *c, uint32_t data);
int cab_buf_pop(cab_buf *c, uint32_t *data);

struct  timespec TsAdd(struct  timespec  ts1, struct  timespec  ts2);
struct  timespec TsSub(struct  timespec  ts1, struct  timespec  ts2);

/* ***********************************************
* Global variables
* ***********************************************/
uint32_t cab_data[200];      // change size if necessary
cab_buf cab = {
    .buffer = cab_data,
    .head = 0,
    .tail = 0,
    .maxlen = 200
};

/* *************************
* Speed detection thread
* **************************/
void* Thread_1_code(void* arg)
{
    
}


/* *************************
* Issue detection thread
* **************************/
void* Thread_2_code(void* arg) {
}


/* *************************
* Direction detection thread
* **************************/
void* Thread_3_code(void* arg)
{

}

/* *************************
* RT values display thread
* **************************/
void* Thread_4_code(void* arg)
{

}

/* *************************
* FFT thread
* **************************/
void* Thread_5_code(void* arg)
{

}

/* *************************
* main()
* **************************/
int main(int argc, char *argv[]) {

}

/* ***********************************************
* Auxiliary functions 
* ************************************************/

// Adds two timespect variables
struct  timespec  TsAdd(struct  timespec  ts1, struct  timespec  ts2){
	
	struct  timespec  tr;
	
	// Add the two timespec variables
		tr.tv_sec = ts1.tv_sec + ts2.tv_sec ;
		tr.tv_nsec = ts1.tv_nsec + ts2.tv_nsec ;
	// Check for nsec overflow	
	if (tr.tv_nsec >= NS_IN_SEC) {
			tr.tv_sec++ ;
		tr.tv_nsec = tr.tv_nsec - NS_IN_SEC ;
		}

	return (tr) ;
}

// Subtracts two timespect variables
struct  timespec  TsSub (struct  timespec  ts1, struct  timespec  ts2) {
  struct  timespec  tr;

  // Subtract second arg from first one 
  if ((ts1.tv_sec < ts2.tv_sec) || ((ts1.tv_sec == ts2.tv_sec) && (ts1.tv_nsec <= ts2.tv_nsec))) {
	// Result would be negative. Return 0
	tr.tv_sec = tr.tv_nsec = 0;  
  } else {						
	// If T1 > T2, proceed 
		tr.tv_sec = ts1.tv_sec - ts2.tv_sec ;
		if (ts1.tv_nsec < ts2.tv_nsec) {
			tr.tv_nsec = ts1.tv_nsec + NS_IN_SEC - ts2.tv_nsec ;
			tr.tv_sec-- ;				
		} else {
			tr.tv_nsec = ts1.tv_nsec - ts2.tv_nsec ;
		}
	}

	return (tr) ;
}

/* cab push || overwrites old information*/
int cab_buf_push(cab_buf *c, uint32_t data)
{
    int next;

    next = c->head + 1;
    if (next >= c->maxlen)
        next = 0;

    // buffer full -> discard tail
    if (next == c->tail) {
        int aux = c->tail + 1;
        if (aux >= c->maxlen)
            aux = 0;
        c->tail = aux;
    }   

    c->buffer[c->head] = data;
    c->head = next;
    return 0;
}

int cab_buf_pop(cab_buf *c, uint32_t *data)
{
    int next;

    if (c->head == c->tail)
        return -1;

    next = c->tail + 1;
    if(next >= c->maxlen)
        next = 0;

    *data = c->buffer[c->tail];
    c->tail = next;
    return 0;
}