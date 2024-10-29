#include "rtsounds.h"
#include <stdio.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdint.h>
#include <complex.h>

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
int periods[4];

// Real time vars
speedVars speedValues;
issueVars issueValues;
directionVars directionValues;

/* *************************
* Audio recording / LP filter thread
* **************************/
void* Audio_thread(void* arg)
{
    buffer* write_buffer; 
    printf("Recording\n");

    while(1) {
        gBufferBytePosition = 0;
        SDL_PauseAudioDevice(recordingDeviceId, SDL_FALSE);

        // Wait until recording buffer is full
        while(1) {
            SDL_LockAudioDevice(recordingDeviceId);
            if (gBufferBytePosition >= gBufferByteMaxPosition) {
                SDL_PauseAudioDevice(recordingDeviceId, SDL_TRUE);
                SDL_UnlockAudioDevice(recordingDeviceId);
                break;
            }
            SDL_UnlockAudioDevice(recordingDeviceId);
        }

        // Apply low-pass filter
        filterLP(COF, SAMP_FREQ, gRecordingBuffer, gBufferByteMaxPosition / sizeof(uint16_t));

        // Write to circular buffer
        write_buffer = cab_getWriteBuffer(&cab_buffer);
        if (write_buffer) {
            memcpy(write_buffer->buf, gRecordingBuffer, gBufferByteMaxPosition);
            cab_releaseWriteBuffer(&cab_buffer, write_buffer->index);
        }
    }
}

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
    tp.tv_sec = periods[0] / NS_IN_SEC;
    tp.tv_nsec = periods[0] % NS_IN_SEC;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts = TsAdd(ts, tp);
    
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
    
    while (1) {
        /* Wait until next cycle */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
        clock_gettime(CLOCK_MONOTONIC, &ta);
        ts = TsAdd(ts, tp);
        
        niter++; // Count number of activations

        // Obter o buffer do CAB
        buffer* b = cab_getReadBuffer(&cab_buffer);

        // Transferir os dados de áudio do buffer de gravação para o buffer de FFT
        printf("Transferring data from recording buffer to FFT buffer...\n");
        for (int i = 0; i < N; i++) {
            // Convertendo cada valor de int16_t para complex double
            x[i] = (complex double)((int16_t)b->buf[i]);
        }

        // Verificar os primeiros valores do buffer de FFT
        printf("Input data for FFT (First 10 Samples):\n");
        for (int i = 0; i < 10 && i < N; i++) {
            printf("x[%d] = %.2f + %.2fi\n", i, creal(x[i]), cimag(x[i]));
        }

        // Liberar o buffer
        cab_releaseReadBuffer(&cab_buffer, b->index);

        // Compute the FFT
        fftCompute(x, N);
        
        // Get the amplitude for each frequency
        fftGetAmplitude(x, N, SAMP_FREQ, fk, Ak);
        
        // Find the most powerful frequency between 2 kHz and 5 kHz
        for (int k = 0; k <= N / 2; k++) {
            if (fk[k] >= 2000 && fk[k] <= 5000) {
                if (Ak[k] > maxAmplitude) {
                    maxAmplitude = Ak[k];
                    speedValues.detectedSpeedFrequency = fk[k];
                }
            }
        }

        speedValues.detectedSpeed = frequency_to_speed(speedValues.detectedSpeedFrequency);
        
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

		// Obter o buffer do CAB
		buffer* b = cab_getReadBuffer(&cab_buffer);

		// Transferir os dados de áudio do buffer de gravação para o buffer de FFT
		printf("Transferring data from recording buffer to FFT buffer...\n");
		for (int i = 0; i < N; i++) {
			// Convertendo cada valor de int16_t para complex double
			x[i] = (complex double)((int16_t)b->buf[i]);
		}

		// Verificar os primeiros valores do buffer de FFT
		printf("Input data for FFT (First 10 Samples):\n");
		for (int i = 0; i < 10 && i < N; i++) {
			printf("x[%d] = %.2f + %.2fi\n", i, creal(x[i]), cimag(x[i]));
		}

		// Liberar o buffer
		cab_releaseReadBuffer(&cab_buffer, b->index);

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

		pthread_mutex_lock(&updatedVarMutex);  // Adicione um lock antes de acessar o buffer
		pthread_cond_signal(&updatedVar);
		// Copiar dados para o buffer FFT
		for (int i = 0; i < N; i++) {
			x[i] = (complex double)((int16_t)b->buf[i]);
		}

		pthread_mutex_unlock(&updatedVarMutex);  // 
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
		buffer* b = cab_getReadBuffer(&cab_buffer);

		// Copy buffer content
		for (int i = 0; i < N; i++) {
				x[i] = b->buf[i];
		}

		// Release buffer
		cab_releaseReadBuffer(&cab_buffer, b->index);

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

		pthread_mutex_lock(&updatedVarMutex);  // Adicione um lock antes de acessar o buffer
		pthread_cond_signal(&updatedVar);
		// Copiar dados para o buffer FFT
		for (int i = 0; i < N; i++) {
			x[i] = (complex double)((int16_t)b->buf[i]);
		}

		pthread_mutex_unlock(&updatedVarMutex);  // 
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
        printf("Detected direction: %s\n", directionValues.forwards ? "forwards" : "backwards");
    }
    return NULL;
}

/* Define thread_data_t if not already defined */
typedef struct {
    cab *cab_t;  // Pointer to cab structure holding audio buffers
    int index_read;  // Index to read from the buffer array
} thread_data_t;

void fft_AF(thread_data_t *data, FILE *file) {
    // Obtain the buffer using the provided index
    buffer *buf = data->cab_t->buflist[data->index_read]; // Correctly get the buffer from buflist

    // Ensure the buffer is not NULL
    if (buf == NULL) {
        return;
    }

    uint8_t *gRecordingBuffer = buf->buf;  // Access the actual buffer inside the buffer structure

    // Adapt parameters as per your requirements
    int N = 0;
    int samp_duration_ms = 100;  // Adjust sample duration if needed
    int samp_freq = SAMP_FREQ / 100;  // Adjust sampling frequency as needed
    uint16_t *sampleVector = (uint16_t *)gRecordingBuffer;

    float *fk;  // Frequencies
    float *Ak;  // Amplitudes
    complex double *x;  // FFT input/output array

    // Get the vector size, ensuring it is a power of two
    for (N = 1; pow(2, N) < samp_freq; N++);
    N--;
    N = (int)pow(2, N);

    // Allocate memory for samples and vectors
    x = (complex double *)malloc(N * sizeof(complex double));
    fk = (float *)malloc(N * sizeof(float));
    Ak = (float *)malloc(N * sizeof(float));

    // Copy samples to complex input vector `x`
    for (int k = 0; k < N; k++) {
        x[k] = (complex double)sampleVector[k];
    }

    // Compute FFT
    fftCompute(x, N);

    // Get amplitudes
    fftGetAmplitude(x, N, SAMP_FREQ, fk, Ak);

    // Print frequencies and amplitudes to file or console for debugging
    for (int k = 1; k < N / 2; k++) {
        fprintf(file, "%f %f\n", fk[k], Ak[k]);
    }

    // Free allocated memory
    free(x);
    free(fk);
    free(Ak);
}




/* *************************
* main()
* **************************/
pthread_t thread1, thread2, thread3, thread4, thread5, thread6;
struct sched_param parm1, parm2, parm3, parm4, parm5, parm6;
pthread_attr_t attr1, attr2, attr3, attr4, attr5, attr6;

int priorities[6];

void usage() {
    printf("Usage: ./rtsounds -prio [p1 p2 p3 p4 p5 p6]\n");
}

void cleanup() {
    SDL_CloseAudioDevice(recordingDeviceId);
    SDL_Quit();
}

void handle_signal(int signal) {
    cleanup();
    exit(0);
}

int main(int argc, char *argv[]) {
    // General vars
    int err;
    // Inicialize as estruturas sched_param para evitar valores não inicializados
    struct sched_param parm1 = {0};
    struct sched_param parm2 = {0};
    struct sched_param parm3 = {0};
    struct sched_param parm4 = {0};
    struct sched_param parm5 = {0};
    struct sched_param parm6 = {0};
    // Atribua as prioridades após inicializar os parâmetros
    unsigned char* procname1 = "AudioThread";
    unsigned char* procname2 = "SpeedThread";
    unsigned char* procname3 = "IssueThread";
    unsigned char* procname4 = "DirectionThread";
    unsigned char* procname5 = "DisplayThread";
    unsigned char* procname6 = "FFTThread";
    int idx = 1;

    // Sound vars
    const char *deviceName;
    int index;
    int bytesPerSample;
    int bytesPerSecond;

  // Definir prioridades padrão ou a partir dos argumentos
if (argc > 1) {
    // Lê as prioridades passadas nos argumentos
    if (strcmp(argv[idx++], "-prio") == 0) {
        for (int i = 0; i < 6; i++, idx++) {
            priorities[i] = atoi(argv[idx]);
            if (priorities[i] == 0) {
                fprintf(stderr, "Erro no argumento %s\n", argv[idx]);
                usage(argc, argv);
                return 1;
            }
        }
    } else {
        priorities[0] = 10;  // Audio - lowest priority
        priorities[1] = 40;  // Speed
        priorities[2] = 60;  // Issues
        priorities[3] = 50;  // Direction
        priorities[4] = 30;  // RT vars display
        priorities[5] = 20;  // FFT display
    }
} else {
    // Prioridades padrão se nenhum argumento for fornecido
    priorities[0] = 10;  // Audio - lowest priority
    priorities[1] = 40;  // Speed
    priorities[2] = 60;  // Issues
    priorities[3] = 50;  // Direction
    priorities[4] = 30;  // RT vars display
    priorities[5] = 20;  // FFT display
}

// Obter limites de prioridade
int max_priority = sched_get_priority_max(SCHED_FIFO);
int min_priority = sched_get_priority_min(SCHED_FIFO);

// Verificar se as prioridades estão no intervalo permitido
for (int i = 0; i < 6; i++) {
    if (priorities[i] < min_priority || priorities[i] > max_priority) {
        fprintf(stderr, "Erro: Prioridade %d fora do intervalo permitido (%d - %d)\n", 
                priorities[i], min_priority, max_priority);
        return 1;
    }
}
    // SDL Init
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        return 1;
    }
    atexit(cleanup);

    // Get and open recording device
    SDL_AudioSpec desiredRecordingSpec;
    SDL_zero(desiredRecordingSpec);
    desiredRecordingSpec.freq = SAMP_FREQ;
    desiredRecordingSpec.format = FORMAT;
    desiredRecordingSpec.channels = MONO;
    desiredRecordingSpec.samples = ABUFSIZE_SAMPLES;
    desiredRecordingSpec.callback = audioRecordingCallback;

    gRecordingDeviceCount = SDL_GetNumAudioDevices(SDL_TRUE);
    if (gRecordingDeviceCount < 1) {
        printf("Unable to get audio capture device! SDL Error: %s\n", SDL_GetError());
        return 0;
    }

    for(int i = 0; i < gRecordingDeviceCount; ++i) {
        deviceName = SDL_GetAudioDeviceName(i, SDL_TRUE);
        printf("%d - %s\n", i, deviceName);
    }

    if (idx < argc) {
        index = atoi(argv[1]);
    } else {
        printf("Choose audio\n");
        scanf("%d", &index);
    }

    if (index < 0 || index >= gRecordingDeviceCount) {
        printf("Invalid device ID. Must be between 0 and %d\n", gRecordingDeviceCount - 1);
        return 0;
    } else {
        printf("Using audio capture device %d - %s\n", index, deviceName);
    }

    recordingDeviceId = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(index, SDL_TRUE), SDL_TRUE, &desiredRecordingSpec, &gReceivedRecordingSpec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (recordingDeviceId == 0) {
        printf("Failed to open recording device! SDL Error: %s", SDL_GetError());
        return 1;
    }

    bytesPerSample = gReceivedRecordingSpec.channels * (SDL_AUDIO_BITSIZE(gReceivedRecordingSpec.format) / 8);
    bytesPerSecond = gReceivedRecordingSpec.freq * bytesPerSample;
    gBufferByteSize = RECORDING_BUFFER_SECONDS * bytesPerSecond;
    gBufferByteMaxPosition = MAX_RECORDING_SECONDS * bytesPerSecond;

    gRecordingBuffer = (uint8_t *)malloc(gBufferByteSize);
    memset(gRecordingBuffer, 0, gBufferByteSize);


    init_cab(&cab_buffer);
    printf("CAB Buffer Initialization:\n");
    for (int i = 0; i < NTASKS + 1; i++) {
        printf("Buffer %d, nusers: %d, Index: %d\n", i, cab_buffer.buflist[i]->nusers, cab_buffer.buflist[i]->index);
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Initialize thread attributes and create threads
    pthread_attr_init(&attr1);
    pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr1, SCHED_OTHER);

    parm1.sched_priority = priorities[0];
    pthread_attr_setschedparam(&attr1, &parm1);
   // Inicializa as threads
    err = pthread_create(&thread1, &attr1, Audio_thread, &procname1);
    if (err != 0) {
        printf("\nErro ao criar Thread [%s]", strerror(err));
        return 1;
    }

    err = pthread_create(&thread2, &attr2, Speed_thread, &procname2);
    if (err != 0) {
        printf("\nErro ao criar Thread [%s]", strerror(err));
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

	
	 pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    pthread_join(thread4, NULL);
    pthread_join(thread5, NULL);
  
	
	cleanup();
    // Repeat for other threads
    pthread_attr_init(&attr2);
    pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);
    parm2.sched_priority = priorities[1];
    pthread_attr_setschedparam(&attr2, &parm2);
    err = pthread_create(&thread2, &attr2, Speed_thread, &procname2);
    if (err != 0) {
        printf("\n\r Error creating Thread [%s]", strerror(err));
        return 1;
    }

    // Continue similarly for thread3, thread4, thread5, thread6

    while (1);

    return 0;
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
buffer* cab_getWriteBuffer(cab* c) {
    for (int i = 0; i < NTASKS + 1; i++) {
        if (c->buflist[i]->nusers == 0) {
            pthread_mutex_lock(&c->buflist[i]->bufMutex);
            printf("Obtained Write Buffer Index: %d\n", c->buflist[i]->index); // Use c->buflist[i]
            return c->buflist[i];
        }
    }
    return NULL;
}

buffer* cab_getReadBuffer(cab* c) {
    while (pthread_mutex_trylock(&c->buflist[c->last_write]->bufMutex));

    c->buflist[c->last_write]->nusers += 1;
    pthread_mutex_unlock(&c->buflist[c->last_write]->bufMutex);

    printf("Obtained Read Buffer Index: %d\n", c->buflist[c->last_write]->index); // Use c->buflist[c->last_write]
    return c->buflist[c->last_write];
}


void cab_releaseWriteBuffer(cab* c, uint8_t index) {
    c->last_write = index;
	pthread_mutex_unlock(&c->buflist[index]->bufMutex);
	
}

void cab_releaseReadBuffer(cab* c, uint8_t index) {
    c->buflist[index]->nusers--;
	
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



void init_cab(cab *c) {
    for (int i = 0; i < NTASKS + 1; i++) {
        c->buflist[i] = malloc(sizeof(buffer));
        if (c->buflist[i] != NULL) {
            c->buflist[i]->nusers = 0;
            c->buflist[i]->index = 0;
            pthread_mutex_init(&(c->buflist[i]->bufMutex), NULL);
        }
    }
    c->last_write = 0;
}

void audioRecordingCallback(void* userdata, Uint8* stream, int len) {
    memcpy(&gRecordingBuffer[gBufferBytePosition], stream, len);

    // Print a few values from stream to verify data capture
	printf("Recording Buffer Snapshot (First 10 Samples):\n");
    for (int i = 0; i < 10 && i < len; i += 2) {
        printf("Audio Data Sample %d: %d\n", i / 2, ((int16_t*)stream)[i / 2]);
    }

    gBufferBytePosition += len;
}
void filterLP(uint32_t cof, uint32_t sampleFreq, uint8_t * buffer, uint32_t nSamples) {					
    int i;
    uint16_t *procBuffer; 
    uint16_t *origBuffer; 
    
    // Setup filter
    float alfa, beta;
    alfa = (2 * M_PI / sampleFreq * cof ) / ( (2 * M_PI / sampleFreq * cof ) + 1 );
    beta = 1 - alfa;

    origBuffer = (uint16_t *)buffer;
    procBuffer = (uint16_t *)malloc(nSamples * sizeof(uint16_t));		
    memset(procBuffer, 0, nSamples * sizeof(uint16_t));

    // Original buffer print (Pre-filter)
    printf("Original Buffer (pre-filter):\n");
    for (int i = 0; i < 10; i++) {
        printf("Original Sample %d: %d\n", i, origBuffer[i]);
    }

    for(i = 1; i < nSamples; i++) {				
        procBuffer[i] = alfa * origBuffer[i] + beta * procBuffer[i - 1];
    }

    // Processed buffer print (Post-filter)
    printf("Processed Buffer (post-filter):\n");
    for (int i = 0; i < 10; i++) {
        printf("Processed Sample %d: %d\n", i, procBuffer[i]);
    }

    memcpy(buffer, (uint8_t *)procBuffer, nSamples * sizeof(uint16_t));	
    free(procBuffer);
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

