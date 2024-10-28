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
pthread_cond_t updatedVar;
pthread_mutex_t updatedVarMutex;
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

// default periods:
// audio - not periodic; activated by recording device
// speed - 100 ms
// issues - 1 s
// direction - 400 ms
// rt display - sporadic; activated by rt var changes
// fft - 2 s
int periods[4];			// save thread periods

// Real time vars
speedVars speedValues;
issueVars issueValues;
directionVars directionValues;

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

			/* Receiving buffer full?Frequency(recordingDeviceId );
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
	printf("Speed detection thread running\n");

	usleep(THREAD_INIT_OFFSET);
	struct timespec ts, // thread next activation time (absolute)
				ta,             // activation time of current thread activation (absolute)
				tit,            // thread time from last execution,
				ta_ant,         // activation time of last instance (absolute),
				tp;             // Thread period

	int niter = 0;
	tp.tv_sec = periods[0]/NS_IN_SEC;
	tp.tv_nsec = periods[0]%NS_IN_SEC;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts = TsAdd(ts,tp);
    
    int N = BUF_SIZE;  // Number of samples
    float *fk;  // Array of frequencies
    float *Ak;  // Array of amplitudes for each frequency
    complex double *x;  // Array of complex values (samples and FFT output)
	float maxAmplitude = 0;
    
    printf("Number of samples is: %d\n", N);
    
    // Allocate memory for FFT
    x = (complex double *)malloc(N * sizeof(complex double));
    fk = (float *)malloc(N * sizeof(float));
    Ak = (float *)malloc(N * sizeof(float));
    
	while(1) {
		/* Wait until next cycle */
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,&ts,NULL);
		clock_gettime(CLOCK_MONOTONIC, &ta);            
		ts = TsAdd(ts,tp);              
		
		niter++; // Count number of activations

		// Get buffer
		buffer b = cab_getReadBuffer(&cab_buffer);

		// Copy buffer content
		for (int i = 0; i < N; i++) {
				x[i] = b.buf[i];
		}

		// Release buffer
		cab_releaseReadBuffer(&cab_buffer, b.index);

		// Compute the FFT
		fftCompute(x, N);
		
		// Get the amplitude for each frequency
		fftGetAmplitude(x, N, SAMP_FREQ, fk, Ak);
		
		// Find the most powerful frequency between 2 kHz and 5 kHz
		for (int k = 0; k <= N/2; k++) {
			if (fk[k] >= 2000 && fk[k] <= 5000) {
				if (Ak[k] > maxAmplitude) {
					maxAmplitude = Ak[k];
					speedValues.detectedSpeedFrequency = fk[k];
				}
			}
		}

		speedValues.detectedSpeed = frequency_to_speed(speedValues.detectedSpeedFrequency);
		
		// Signal for display thread 
		pthread_mutex_lock(&updatedVarMutex);
		pthread_cond_signal(&updatedVar);
		pthread_mutex_unlock(&updatedVarMutex);
    }     
    
    // Free resources
    free(x);
    free(fk);
    free(Ak);
    
    return NULL;
}


/* *************************
* Issue detection thread
* **************************/
void* Issue_thread(void* arg) {

	printf("Issue detection thread running\n");
    
    usleep(THREAD_INIT_OFFSET);
	struct timespec ts, // thread next activation time (absolute)
				ta,             // activation time of current thread activation (absolute)
				tit,            // thread time from last execution,
				ta_ant,         // activation time of last instance (absolute),
				tp;             // Thread period

	int niter = 0;
	tp.tv_sec = periods[1]/NS_IN_SEC;
	tp.tv_nsec = periods[1]%NS_IN_SEC;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts = TsAdd(ts,tp);
    
    int N = BUF_SIZE;  // Number of samples
    float *fk;  // Array of frequencies
    float *Ak;  // Array of amplitudes for each frequency
    complex double *x;  // Array of complex values (samples and FFT output)
	float maxAmplitude = 0;
	float detectedFrequency = 0;
    
    printf("Number of samples for issue detection: %d\n", N);
    
    // Allocate memory
    x = (complex double *)malloc(N * sizeof(complex double));
    fk = (float *)malloc(N * sizeof(float));
    Ak = (float *)malloc(N * sizeof(float));
    
	while (1) {
		/* Wait until next cycle */
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,&ts,NULL);
		clock_gettime(CLOCK_MONOTONIC, &ta);            
		ts = TsAdd(ts,tp);              
		
		niter++; // Count number of activations

		// Get buffer
		buffer b = cab_getReadBuffer(&cab_buffer);

		// Copy buffer content
		for (int i = 0; i < N; i++) {
				x[i] = b.buf[i];
		}

		// Release buffer
		cab_releaseReadBuffer(&cab_buffer, b.index);

		// Compute the FFT
		fftCompute(x, N);
		
		// Get the amplitude for each frequency
		fftGetAmplitude(x, N, SAMP_FREQ, fk, Ak);
		
		// Check for low-frequency components (<200 Hz) with amplitude > 20% of peak
		for (int k = 0; k <= N/2; k++) {
			if (fk[k] < 200) {
				if (Ak[k] > issueValues.maxIssueAmplitude)
					issueValues.maxIssueAmplitude = Ak[k];
			}
			if (Ak[k] > maxAmplitude)
				maxAmplitude = Ak[k];
		}

		issueValues.ratio = issueValues.maxIssueAmplitude / maxAmplitude;

		// Signal for display thread 
		pthread_mutex_lock(&updatedVarMutex);
		pthread_cond_signal(&updatedVar);
		pthread_mutex_unlock(&updatedVarMutex);
	}
    
    // Free resources
    free(x);
    free(fk);
    free(Ak);
    
    return NULL;
}

/* *************************
* Direction detection thread
* **************************/
void* Direction_thread(void* arg)
{
	printf("Direction detection thread running\n");
    
    usleep(THREAD_INIT_OFFSET);
	struct timespec ts, // thread next activation time (absolute)
				ta,             // activation time of current thread activation (absolute)
				tit,            // thread time from last execution,
				ta_ant,         // activation time of last instance (absolute),
				tp;             // Thread period

	int niter = 0;
	tp.tv_sec = periods[2]/NS_IN_SEC;
	tp.tv_nsec = periods[2]%NS_IN_SEC;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts = TsAdd(ts,tp);
    
    int N = BUF_SIZE;  // Number of samples
    float *fk;  // Array of frequencies
    float *Ak;  // Array of amplitudes for each frequency
    complex double *x;  // Array of complex values (samples and FFT output)
	float maxAmplitude = 0;
	float detectedFrequency = 0;
    
    printf("Number of samples for issue detection: %d\n", N);
    
    // Allocate memory
    x = (complex double *)malloc(N * sizeof(complex double));
    fk = (float *)malloc(N * sizeof(float));
    Ak = (float *)malloc(N * sizeof(float));
    
	while(1) {
		/* Wait until next cycle */
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,&ts,NULL);
		clock_gettime(CLOCK_MONOTONIC, &ta);            
		ts = TsAdd(ts,tp);              
		
		niter++; // Count number of activations

		// Get buffer
		buffer b = cab_getReadBuffer(&cab_buffer);

		// Copy buffer content
		for (int i = 0; i < N; i++) {
				x[i] = b.buf[i];
		}

		// Release buffer
		cab_releaseReadBuffer(&cab_buffer, b.index);

		// Compute the FFT
		fftCompute(x, N);
		
		// Get the amplitude for each frequency
		fftGetAmplitude(x, N, SAMP_FREQ, fk, Ak);
		
		// Find the most powerful frequency between 2 kHz and 5 kHz
		for (int k = 0; k <= N/2; k++) {
			if (fk[k] >= 2000 && fk[k] <= 5000) {
				if (Ak[k] > maxAmplitude) {
					maxAmplitude = Ak[k];
					detectedFrequency = fk[k];
				}
			}
		}

		// Get direction
		directionValues.forwards = detectDirection(maxAmplitude, directionValues.lastAmplitude, detectedFrequency,
											       directionValues.lastFrequency, speedValues.detectedSpeed);

		// Update vars
		directionValues.lastAmplitude = maxAmplitude;
		directionValues.lastFrequency = detectedFrequency;

		// Signal for display thread 
		pthread_mutex_lock(&updatedVarMutex);
		pthread_cond_signal(&updatedVar);
		pthread_mutex_unlock(&updatedVarMutex);
	}    
    
    // Free resources
    free(x);
    free(fk);
    free(Ak);
    
    return NULL;
}

/* *************************
* RT values display thread
* **************************/
void* Display_thread(void* arg)
{
    while (1) {
		// Wait for any RT var to be updated
		pthread_mutex_lock(&updatedVarMutex);
		pthread_cond_wait(&updatedVar, &updatedVarMutex);
		pthread_mutex_unlock(&updatedVarMutex);

		printf("Displaying real-time values...\n");

        printf("Detected speed frequency: %f Hz\n", speedValues.detectedSpeedFrequency);
		printf("Calculated speed: %f Hz\n", speedValues.detectedSpeed);
        
		printf("Max amplitude (for issues): %f\t ratio: %f\n", issueValues.maxIssueAmplitude, issueValues.ratio);
		if (issueValues.ratio > 0.2)
			printf("Warning: potential issue detected\n");
		else
			printf("No issues detected\n");

		if (directionValues.forwards)
			printf("Detected direction: forwards\n");
		else
			printf("Detected direction: backwards\n");
    }

    return NULL;
}

/* *************************
* FFT thread
* **************************/
void* FFT_thread(void* arg)
{
	printf("FFT thread running\n");

	usleep(THREAD_INIT_OFFSET);
	struct timespec ts, // thread next activation time (absolute)
				ta,             // activation time of current thread activation (absolute)
				tit,            // thread time from last execution,
				ta_ant,         // activation time of last instance (absolute),
				tp;             // Thread period

	int niter = 0;
	tp.tv_sec = periods[3]/NS_IN_SEC;
	tp.tv_nsec = periods[3]%NS_IN_SEC;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts = TsAdd(ts,tp);
    
    int N = BUF_SIZE;  // Number of samples
    float *fk;  // Array of frequencies
    float *Ak;  // Array of amplitudes for each frequency
    complex double *x;  // Array of complex values (samples and FFT output)

    printf("Number of samples is: %d\n", N);
    
    // Allocate memory for FFT
    x = (complex double *)malloc(N * sizeof(complex double));
    fk = (float *)malloc(N * sizeof(float));
    Ak = (float *)malloc(N * sizeof(float));

    while (1) {
		/* Wait until next cycle */
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,&ts,NULL);
		clock_gettime(CLOCK_MONOTONIC, &ta);            
		ts = TsAdd(ts,tp);              
		
		niter++; // Count number of activations

		// Get buffer
		buffer b = cab_getReadBuffer(&cab_buffer);

		// Copy buffer content
		for (int i = 0; i < N; i++) {
				x[i] = b.buf[i];
		}

		// Release buffer
		cab_releaseReadBuffer(&cab_buffer, b.index);

		// Compute the FFT
		fftCompute(x, N);
		
		// Get the amplitude for each frequency
		fftGetAmplitude(x, N, SAMP_FREQ, fk, Ak);

        // Print frequency and amplitude values for debugging
        for (int k = 0; k <= N/2; k++) {
            printf("Frequency: %f Hz, Amplitude: %f\n", fk[k], Ak[k]);
        }
		
		// TODO: display fft graphically
	}

	// Free resources
	free(x);
	free(fk);
	free(Ak);

	sleep(1);  

    return NULL;
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
	int idx = 1;
	int priorities[6];

	// Sound vars
	const char * deviceName;					/* Capture device name */
	int index;									/* Device index used to browse audio devices */
	int bytesPerSample;							/* Number of bytes each sample requires. Function of size of sample and # of channels */ 
	int bytesPerSecond;							/* Intuitive. bytes per sample sample * sampling frequency */
	printf("debug");
	// Check args
	if (argc > 1) {
		// -prio selected
		if (strcmp(argv[idx++], "-prio") == 0) {
			for(int i = 0; i < 6; i++, idx++) {
				priorities[i] = atoi(argv[idx]);
				if (priorities[i] == 0) {
					fprintf(stderr, "Error in arg %s\n", argv[idx]);
					usage(argc, argv);
					return 1;
				}
			}
		} else {
			priorities[0] = 10;		// Audio - lowest prio
			priorities[1] = 40;		// Speed
			priorities[2] = 60;		// Issues
			priorities[3] = 50;		// Direction
			priorities[4] = 30;		// RT vars display
			priorities[5] = 20;		// FFT display
		}

		// -period selected
		if (strcmp(argv[idx++], "-period") == 0) {
			for(int i = 0; i < 4; i++, idx++) {
				periods[i] = atoi(argv[idx]);
				if (periods[i] == 0) {
					fprintf(stderr, "Error in arg %s\n", argv[idx]);
					usage();
					return 1;
				}
			}
		} else {
			periods[0] = 100*1000*1000;		// 100 ms
			periods[1] = 1000*1000*1000;	// 1 s
			periods[2] = 400*1000*1000;		// 400 ms
			periods[3] = 2000*1000*1000;	// 2 s
		}

		if (idx < argc) {
			fprintf(stderr, "Error in arg %s\n", argv[idx]);
			usage();
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
    parm1.sched_priority = priorities[0];
    pthread_attr_setschedparam(&attr1, &parm1);

    pthread_attr_init(&attr2);
	pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);
    parm2.sched_priority = priorities[1];
    pthread_attr_setschedparam(&attr2, &parm2);

    pthread_attr_init(&attr3);
	pthread_attr_setinheritsched(&attr3, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr3, SCHED_FIFO);
    parm3.sched_priority = priorities[2];
    pthread_attr_setschedparam(&attr3, &parm3);

    pthread_attr_init(&attr4);
	pthread_attr_setinheritsched(&attr4, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr4, SCHED_FIFO);
    parm4.sched_priority = priorities[3];
    pthread_attr_setschedparam(&attr4, &parm4);

    pthread_attr_init(&attr5);
	pthread_attr_setinheritsched(&attr5, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr5, SCHED_FIFO);
    parm5.sched_priority = priorities[4];
    pthread_attr_setschedparam(&attr5, &parm5);

    pthread_attr_init(&attr6);
	pthread_attr_setinheritsched(&attr6, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr6, SCHED_FIFO);
    parm6.sched_priority = priorities[5];
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

void usage() {
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

float frequency_to_speed(float frequency_hz) {
    // Define frequency and speed bounds
    float min_freq = 2000.0, max_freq = 5000.0;  // Frequency range in Hz
    float min_speed = 20.0, max_speed = 120.0;   // Speed range in km/h
    
    // Constrain frequency within bounds for realism
    if (frequency_hz < min_freq) {
        frequency_hz = min_freq;
    } else if (frequency_hz > max_freq) {
        frequency_hz = max_freq;
    }
    
    // Linear interpolation to map frequency to speed
    float speed_kmh = min_speed + (frequency_hz - min_freq) * (max_speed - min_speed) / (max_freq - min_freq);
    
    return speed_kmh;
}

// 1 -> forwards; 0 -> backwards
// Conditions:
//   speed > 40 -> forwards
//   increasing frequency && increasing amplitude -> forwards
//   decreasing or same frequency && low amplitude variation -> backwards
int detectDirection(float curAmplitude, float lastAmplitude, float curFrequency, float lastFrequency, float speed) {
	if (speed > 40)
		return 1;
	if (curFrequency > lastFrequency) {
		if (curAmplitude > lastAmplitude)
			return 1;
	} else {
		if (relativeDiff(curAmplitude, lastAmplitude) < 0.05)
			return 0;
	}
	return 1;
}

float relativeDiff(float a, float b) {
	if (a >= b) {
		return a/b - 1;
	}
	return b/a - 1;
}

