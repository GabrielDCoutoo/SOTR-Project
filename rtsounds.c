#define _GNU_SOURCE             /* Must precede #include <sched.h> for sched_setaffinity */ 
#define __USE_MISC

#include "rtsounds.h"

/* ***********************************************
* App specific defines
* ***********************************************/

/* ***********************************************
* Prototypes
* ***********************************************/

/* ***********************************************
* Global variables
* ***********************************************/
cab cab_buffer;
SDL_AudioDeviceID recordingDeviceId = 0; 	/* Structure with ID of recording device */
Uint8 * gRecordingBuffer = NULL;
int gRecordingDeviceCount = 0;
SDL_AudioSpec gReceivedRecordingSpec;
Uint32 gBufferBytePosition = 0;
Uint32 gBufferByteMaxPosition = 0;
Uint32 gBufferByteSize = 0;
const int MAX_RECORDING_DEVICES = 10;
const int MAX_RECORDING_SECONDS = 5;
const int RECORDING_BUFFER_SECONDS = MAX_RECORDING_SECONDS + 1;

int periods[6];			// save thread periods

/* *************************
* Audio recording / LP filter thread
* **************************/
void* Audio_thread(void* arg)
{
	buffer write_buffer; 
	printf("Recording\n");
    while(1) {
		
		/* Set index to the beginning of buffer */
		gBufferBytePosition = 0;

		/* After being open devices have callback processing blocked (paused_on active), to allow configuration without glitches */
		/* Devices must be unpaused to allow callback processing */
		SDL_PauseAudioDevice(recordingDeviceId, SDL_FALSE ); /* Args are SDL device id and pause_on */
		
		/* Wait until recording buffer full */
		while(1)
		{
			/* Lock callback. Prevents the following code to not concur with callback function */
			SDL_LockAudioDevice(recordingDeviceId);

			/* Receiving buffer full? */
			if(gBufferBytePosition > gBufferByteMaxPosition)
			{
				/* Stop recording audio */
				SDL_PauseAudioDevice(recordingDeviceId, SDL_TRUE );
				SDL_UnlockAudioDevice(recordingDeviceId );
				break;
			}

			/* Buffer not yet full? Keep trying ... */
			SDL_UnlockAudioDevice( recordingDeviceId );
		}

		filterLP(COF, SAMP_FREQ, gRecordingBuffer, gBufferByteMaxPosition/sizeof(uint16_t)); 
		write_buffer = cab_getWriteBuffer(&cab_buffer);
		memcpy(write_buffer.buf, (uint8_t *)gRecordingBuffer, gBufferByteMaxPosition*sizeof(uint16_t));	
		cab_releaseWriteBuffer(&cab_buffer, write_buffer.index);
    }

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
	int idx = 2;
	int priorities[6];

	// Sound vars
	const char * deviceName;					/* Capture device name */
	int index;									/* Device index used to browse audio devices */
	int bytesPerSample;							/* Number of bytes each sample requires. Function of size of sample and # of channels */ 
	int bytesPerSecond;							/* Intuitive. bytes per sample sample * sampling frequency */

	// Check args
	if (argc > 1) {
		// -prio selected
		if (strcmp(argv[idx++], "-prio") == 0) {
			for(int i = 0; i < 6; i++, idx++) {
				priorities[i] = atoi(argv[idx]);
				if (priorities[i] == 0) {
					fprintf(stderr, "Error in arg %s\n", argv[idx]);
					usage(argc, argv);
				}
			}
		} else {
			for (int i = 0; i < 6; i++) {
				priorities[i] = DEFAULT_PRIO;
			}
		}

		// -period selected
		if (strcmp(argv[idx++], "-period") == 0) {
			for(int i = 0; i < 6; i++, idx++) {
				periods[i] = atoi(argv[idx]);
				if (periods[i] == 0) {
					fprintf(stderr, "Error in arg %s\n", argv[idx]);
					usage(argc, argv);
				}
			}
		}
	}
	
	/* SDL Init */
	if(SDL_Init(SDL_INIT_AUDIO) < 0)
	{
		printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
		return 1;
	}

	/* *************************************
	 * Get and open recording device 
	 ************************************* */
	SDL_AudioSpec desiredRecordingSpec;
	/* Defined in SDL_audio.h */
	SDL_zero(desiredRecordingSpec);				/* Init struct with default values */
	desiredRecordingSpec.freq = SAMP_FREQ;		/* Samples per second */
	desiredRecordingSpec.format = FORMAT;		/* Sampling format */
	desiredRecordingSpec.channels = MONO;		/* 1 - mono; 2 stereo */
	desiredRecordingSpec.samples = ABUFSIZE_SAMPLES;		/* Audio buffer size in sample FRAMES (total samples divided by channel count) */
	desiredRecordingSpec.callback = audioRecordingCallback;

	/* Get number of recording devices */
	gRecordingDeviceCount = SDL_GetNumAudioDevices(SDL_TRUE);		/* Argument is "iscapture": 0 to request playback device, !0 for recording device */

	if(gRecordingDeviceCount < 1)
	{
		printf( "Unable to get audio capture device! SDL Error: %s\n", SDL_GetError() );
		return 0;
	}
	
	/* and lists them */
	for(int i = 0; i < gRecordingDeviceCount; ++i)
	{
		//Get capture device name
		deviceName = SDL_GetAudioDeviceName(i, SDL_TRUE);/* Arguments are "index" and "iscapture"*/
		printf("%d - %s\n", i, deviceName);
	}

	/* If device index supplied as arg, use it, otherwise, ask the user */
	if(idx < argc) {
		index = atoi(argv[1]);		
	} else {
		/* allow the user to select the recording device */
		printf("Choose audio\n");
		scanf("%d", &index);
	}
	
	if(index < 0 || index >= gRecordingDeviceCount) {
		printf( "Invalid device ID. Must be between 0 and %d\n", gRecordingDeviceCount-1 );
		return 0;
	} else {
		printf( "Using audio capture device %d - %s\n", index, deviceName );
	}

	/* and open it */
	recordingDeviceId = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(index, SDL_TRUE), SDL_TRUE, &desiredRecordingSpec, &gReceivedRecordingSpec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
	
	/* if device failed to open terminate */
	if(recordingDeviceId == 0)
	{
		//Report error
		printf("Failed to open recording device! SDL Error: %s", SDL_GetError() );
		return 1;
	}

	bytesPerSample = gReceivedRecordingSpec.channels * (SDL_AUDIO_BITSIZE(gReceivedRecordingSpec.format) / 8);
	bytesPerSecond = gReceivedRecordingSpec.freq * bytesPerSample;
	gBufferByteSize = RECORDING_BUFFER_SECONDS * bytesPerSecond;

	/* Calculate max buffer use - some additional space to allow for extra samples*/
	/* Detection of buffer use is made form device-driver callback, so can be a biffer overrun if some */
	/* leeway is not added */ 
	gBufferByteMaxPosition = MAX_RECORDING_SECONDS * bytesPerSecond;

	/* Allocate and initialize record buffer */
	gRecordingBuffer = (uint8_t *)malloc(gBufferByteSize);
	memset(gRecordingBuffer, 0, gBufferByteSize);

    // Initialize CAB
	init_cab(&cab_buffer);

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

	while(1);
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
        if (c->buflist[i].nusers == 0) {
			pthread_mutex_lock(&c->buflist[i].bufMutex);
            return c->buflist[i];
        }
    }
}

buffer cab_getReadBuffer(cab* c) {
	// wait for last read mutex to unlock
	while(pthread_mutex_trylock(&c->buflist[c->last_write].bufMutex));
	pthread_mutex_unlock(&c->buflist[c->last_write].bufMutex);

    c->buflist[c->last_write].nusers += 1;
    return c->buflist[c->last_write];
}

void cab_releaseWriteBuffer(cab* c, uint8_t index) {
    c->last_write = index;
	pthread_mutex_unlock(&c->buflist[index].bufMutex);
}

void cab_releaseReadBuffer(cab* c, uint8_t index) {
    c->buflist[index].nusers--;
}

/* // Copies data from audio stream to circular buffer
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
} */

void usage(int argc, char* argv[]) {
    printf("Usage: ./rtsounds [-prio LPPrio SpeedPrio IssuePrio DirectionPrio DisplayPrio RTPrio]\n[-period LPP SpeedP IssueP DirectionP DisplayP RTP]\n[audioDevice]\n");
}


void init_cab(cab *cab_obj) {
    // Initialize last_write
    cab_obj->last_write = 0;

    // Initialize each buffer in buflist
    for (int i = 0; i < NTASKS + 1; i++) {
        memset(cab_obj->buflist[i].buf, 0, sizeof(cab_obj->buflist[i].buf)); // Clear the buffer
        cab_obj->buflist[i].nusers = 0; // Initialize nusers to 0
		cab_obj->buflist[i].index = i;
        pthread_mutex_init(&cab_obj->buflist[i].bufMutex, NULL); // Initialize the mutex
    }
}

void audioRecordingCallback(void* userdata, Uint8* stream, int len )
{
	/* Copy bytes acquired from audio stream */
	memcpy(&gRecordingBuffer[ gBufferBytePosition ], stream, len);

	/* Update buffer pointer */
	gBufferBytePosition += len;
}

void filterLP(uint32_t cof, uint32_t sampleFreq, uint8_t * buffer, uint32_t nSamples)
{					
	
	int i;
	
	uint16_t * procBuffer; 	/* Temporary buffer */
	uint16_t * origBuffer; 	/* Pointer to original buffer, with right sample type (UINT16 in the case) */
	
	float alfa, beta; 
		
	/* Compute alfa and beta multipliers */
	alfa = (2 * M_PI / sampleFreq * cof ) / ( (2 * M_PI / sampleFreq * cof ) + 1 );
	beta = 1-alfa;
	
	
	/* Get pointer to buffer of the right type */
	origBuffer = (uint16_t *)buffer;
	
	/* allocate temporary buffer and init it */
	procBuffer = (uint16_t *)malloc(nSamples*sizeof(uint16_t));		
	memset(procBuffer,0, nSamples*sizeof(uint16_t));
	        
	/* Apply the filter */		
	for(i = 1; i < nSamples; i++) {				
		procBuffer[i] = alfa * origBuffer[i] + beta * procBuffer[i-1];		
	}
	
	/* Move data to the original (playback) buffer */
	memcpy(buffer, (uint8_t *)procBuffer, nSamples*sizeof(uint16_t));	
	
	/* Release resources */
	free(procBuffer);	
	
	return;
}
