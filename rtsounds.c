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
#include "simpleRecPlay.c"

/* ***********************************************
* App specific defines
* ***********************************************/
#define NS_IN_SEC 1000000000L

#define DEFAULT_PRIO 50				// Default (fixed) thread priority  
#define BUF_SIZE 4096
#define NTASKS 6

#define THREAD_INIT_OFFSET 1000000	// Initial offset (i.e. delay) of rt thread

/* ***********************************************
* Prototypes
* ***********************************************/
typedef struct {
    buffer buflist[NTASKS+1];
    uint8_t last_write;
} cab;

typedef struct {
    uint16_t buf[BUF_SIZE];
    uint8_t nusers;
} buffer;

struct  timespec TsAdd(struct  timespec  ts1, struct  timespec  ts2);
struct  timespec TsSub(struct  timespec  ts1, struct  timespec  ts2);

/* ***********************************************
* Global variables
* ***********************************************/

/* *************************
* Audio recording / LP filter thread
* **************************/
void* Audio_thread(void* arg)
{
}

/* *************************
* Speed detection thread
* **************************/
void* Speed_thread(void* arg)
{
    
}


/* *************************
* Issue detection thread
* **************************/
void* Issue_thread(void* arg) {
}


/* *************************
* Direction detection thread
* **************************/
void* Direction_thread(void* arg)
{

}

/* *************************
* RT values display thread
* **************************/
void* Display_thread(void* arg)
{

}

/* *************************
* FFT thread
* **************************/
void* FFT_thread(void* arg)
{

}


/* *************************
* main()
* **************************/
int main(int argc, char *argv[]) {
    // General vars
    int err;
    unsigned char* procname1 = "AudioThread";
	unsigned char* procname2 = "SpeedThread";
	unsigned char* procname3 = "IssueThread";
    unsigned char* procname4 = "DirectionThread";
	unsigned char* procname5 = "DisplayThread";
	unsigned char* procname6 = "FFTThread";

    // Initialize CABs


    pthread_t thread1, thread2, thread3, thread4, thread5, thread6;
	struct sched_param parm1, parm2, parm3, parm4, parm5, parm6; 
	pthread_attr_t attr1, attr2, attr3, attr4, attr5, attr6;
	cpu_set_t cpuset_test; // To check process affinity

    // Initialize threads; change priority later
    pthread_attr_init(&attr1);
	pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);
    parm1.sched_priority = DEFAULT_PRIO;
    pthread_attr_setschedparam(&attr1, &parm1);

    pthread_attr_init(&attr2);
	pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);
    parm2.sched_priority = DEFAULT_PRIO;
    pthread_attr_setschedparam(&attr2, &parm2);

    pthread_attr_init(&attr3);
	pthread_attr_setinheritsched(&attr3, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr3, SCHED_FIFO);
    parm3.sched_priority = DEFAULT_PRIO;
    pthread_attr_setschedparam(&attr3, &parm3);

    pthread_attr_init(&attr4);
	pthread_attr_setinheritsched(&attr4, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr4, SCHED_FIFO);
    parm4.sched_priority = DEFAULT_PRIO;
    pthread_attr_setschedparam(&attr4, &parm4);

    pthread_attr_init(&attr5);
	pthread_attr_setinheritsched(&attr5, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr5, SCHED_FIFO);
    parm5.sched_priority = DEFAULT_PRIO;
    pthread_attr_setschedparam(&attr5, &parm5);

    pthread_attr_init(&attr6);
	pthread_attr_setinheritsched(&attr6, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr6, SCHED_FIFO);
    parm6.sched_priority = DEFAULT_PRIO;
    pthread_attr_setschedparam(&attr6, &parm6);

    /* Lock memory */
	mlockall(MCL_CURRENT | MCL_FUTURE);

    err=pthread_create(&thread1, &attr1, Audio_thread, &procname1);
	if(err != 0) {
		printf("\n\r Error creating Thread [%s]", strerror(err));
		return 1;
	}

	err=pthread_create(&thread2, &attr2, Speed_thread, &procname2);
	if(err != 0) {
		printf("\n\r Error creating Thread [%s]", strerror(err));
		return 1;
	}

	err=pthread_create(&thread3, &attr3, Issue_thread, &procname3);
	if(err != 0) {
		printf("\n\r Error creating Thread [%s]", strerror(err));
		return 1;
	}

    err=pthread_create(&thread4, &attr4, Direction_thread, &procname4);
	if(err != 0) {
		printf("\n\r Error creating Thread [%s]", strerror(err));
		return 1;
	}

	err=pthread_create(&thread5, &attr5, Display_thread, &procname5);
	if(err != 0) {
		printf("\n\r Error creating Thread [%s]", strerror(err));
		return 1;
	}

	err=pthread_create(&thread6, &attr6, FFT_thread, &procname6);
	if(err != 0) {
		printf("\n\r Error creating Thread [%s]", strerror(err));
		return 1;
	}



    while(1) {
        
    }
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

buffer cab_getWriteBuffer(cab* c) {
    for (int i = 0; i < NTASKS+1; i++) {
        if (c->buflist[i]->nusers == 0) {
            return c->buflist[i];
        }
    }
}

buffer cab_getReadBuffer(cab* c) {
    c->buflist[c->last_write]->nusers += 1;
    return c->buflist[c->last_write];
}

void cab_releaseWriteBuffer(cab* c, uint8_t index) {
    c->last_write = index;
}

void cab_releaseReadBuffer(cab* c, uint8_t index) {
    c->buflist[index]->nusers--;
}

// Copies data from audio stream to circular buffer
void audioRecordingCallback(void* userdata, uint16_t* stream, int len )
{
    int space;
    if (cab.tail > cab.head)
        space = cab.tail - cab.head;
    else
        space = cab.maxlen - cab.head + cab.tail;

    if (len > space) {
        cab.tail = (cab.tail + len - space) % cab.maxlen;
    }

    int newWritePos = (cab.head + len) % cab.maxlen;
    if (cab.head + len > cab.maxlen) {
        memcpy(cab.buffer[cab.head], stream, cab.maxlen - cab.head);
        cab.head = 0;
    }

    memcpy(cab.buffer[cab.head], stream, newWritePos-cab.head);
    cab.head = newWritePos;
}

void usage(int argc, char* argv[]) {
    printf("Usage: ./rtsounds [-prio LPPrio SpeedPrio IssuePrio DirectionPrio DisplayPrio RTPrio]\n[-period LPP SpeedP IssueP DirectionP DisplayP RTP]\n");
}