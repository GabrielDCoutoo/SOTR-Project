#ifndef _RTSOUNDS_H
#define _RTSOUNDS_H
#define _GNU_SOURCE             /* Must precede #include <sched.h> for sched_setaffinity */ 
#define __USE_MISC

/* App specific defines */
#define NS_IN_SEC 1000000000L
#define DEFAULT_PRIO 50				// Default (fixed) thread priority  
#define BUF_SIZE 4096
#define NTASKS 6
#define THREAD_INIT_OFFSET 1000000	// Initial offset (i.e. delay) of rt thread
#define MONO 1 					/* Sample and play in mono (1 channel) */
#define SAMP_FREQ 44100			/* Sampling frequency used by audio device */
#define FORMAT AUDIO_U16		/* Format of each sample (signed, unsigned, 8,16 bits, int/float, ...) */
#define ABUFSIZE_SAMPLES 4096	/* Audio buffer size in sample FRAMES (total samples divided by channel count) */
#define COF 10000

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
#include <SDL.h>
#include <complex.h>
#include <SDL_stdinc.h>

typedef struct {
    uint16_t buf[BUF_SIZE];
    uint8_t nusers;
	uint8_t index;
	pthread_mutex_t bufMutex;
} buffer;

typedef struct {
    buffer buflist[NTASKS+1];
    uint8_t last_write;
} cab;

struct  timespec TsAdd(struct  timespec  ts1, struct  timespec  ts2);
struct  timespec TsSub(struct  timespec  ts1, struct  timespec  ts2);

buffer cab_getWriteBuffer(cab* c);
buffer cab_getReadBuffer(cab* c);
void cab_releaseWriteBuffer(cab* c, uint8_t index);
void cab_releaseReadBuffer(cab* c, uint8_t index);
void usage(int argc, char* argv[]);
void init_cab(cab *cab_obj);
void audioRecordingCallback(void* userdata, Uint8* stream, int len);
void filterLP(uint32_t cof, uint32_t sampleFreq, uint8_t * buffer, uint32_t nSamples);

#endif
