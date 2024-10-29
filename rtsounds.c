#define _GNU_SOURCE /* Must precede #include <sched.h> for sched_setaffinity */ 
#define __USE_MISC
#define ABUFSIZE_SAMPLES 4096
#define NTASKS 6
#include "rtsounds.h"
#include <math.h>
#include <complex.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL.h>
#include <semaphore.h>


void save_audio_to_wav(const char* filename, Uint8* buffer, Uint32 buffer_size, int sample_rate);

/* ***********************************************
* Global variables
* ***********************************************/
cab cab_buffer;
SDL_AudioDeviceID recordingDeviceId = 0;  
Uint8 *gRecordingBuffer = NULL;
SDL_AudioSpec gReceivedRecordingSpec;
Uint32 gBufferBytePosition = 0, gBufferByteMaxPosition = 0, gBufferByteSize = 0;

volatile float detectedSpeedFrequency = 0.0, maxAmplitudeDetected = 0.0;

pthread_mutex_t updatedVarMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t updatedVar = PTHREAD_COND_INITIALIZER;
sem_t data_ready;  // Semaphore for managing data readines
/* *************************
* Thread Functions
* *************************/


void* Audio_thread(void* arg) {
    printf("Recording Thread Running\n");
    while (1) {
        gBufferBytePosition = 0;
        SDL_PauseAudioDevice(recordingDeviceId, SDL_FALSE);
        
        while (gBufferBytePosition < gBufferByteMaxPosition) {
            SDL_Delay(10);
        }
        
        SDL_PauseAudioDevice(recordingDeviceId, SDL_TRUE);
        
        filterLP(COF, SAMP_FREQ, gRecordingBuffer, gBufferByteMaxPosition / sizeof(uint16_t));
        save_audio_to_wav("output.wav", gRecordingBuffer, gBufferByteMaxPosition, gReceivedRecordingSpec.freq);
        
        sem_post(&data_ready);  // Signal that data is ready for processing
        
        printf("Áudio salvo em output.wav\n");
    }
}

void* Speed_thread(void* arg) {
    printf("Speed Detection Thread Running\n");
    
    // FFT parameters
    int N = pow(2, floor(log2(ABUFSIZE_SAMPLES)));
    if (N < 64) N = 64;
    
    complex double *x = malloc(N * sizeof(complex double));
    float *fk = malloc(N * sizeof(float));
    float *Ak = malloc(N * sizeof(float));
    
    float maxAmplitude = 0.0;
    float detectedFrequency = 0.0;

    while (1) {
        sem_wait(&data_ready);  // Wait until data is ready
        
        buffer* read_buffer = cab_getReadBuffer(&cab_buffer);
        if (read_buffer) {
            for (int k = 0; k < N; k++) {
                if (k < ABUFSIZE_SAMPLES) {
                    x[k] = (complex double)((float)read_buffer->buf[k]) / 32768.0;
                } else {
                    x[k] = 0.0;
                }
            }
            
            for (int i = 0; i < 10; i++) {  // Imprimir os primeiros 10 valores
                printf("FFT[%d]: %f + %fi\n", i, creal(x[i]), cimag(x[i]));
            }

            fftCompute(x, N);

            // Amplitude Calculation Debugging
            fftGetAmplitude(x, N, SAMP_FREQ, fk, Ak);
            for (int k = 0; k <= N / 2; k++) {
                printf("Frequency: %f Hz, Amplitude: %f\n", fk[k], Ak[k]);
            }

            // Atualizar `maxAmplitude` e `detectedFrequency`
            for (int k = 0; k <= N / 2; k++) {
                if (fk[k] >= 2000.0 && fk[k] <= 5000.0) {
                    if (Ak[k] > maxAmplitude) {
                        maxAmplitude = Ak[k];
                        detectedFrequency = fk[k];
                    }
                }
            }

            // Se não for encontrada amplitude na faixa esperada
            if (maxAmplitude == 0.0) {
                printf("Nenhuma amplitude significativa detectada na faixa de 2000-5000 Hz.\n");
            } else {
                printf("Frequência detectada: %.2f Hz, Amplitude: %.2f\n", detectedFrequency, maxAmplitude);
            }

            pthread_mutex_lock(&updatedVarMutex);
            detectedSpeedFrequency = detectedFrequency;
            maxAmplitudeDetected = maxAmplitude;
            pthread_mutex_unlock(&updatedVarMutex);
            
            cab_releaseReadBuffer(&cab_buffer, read_buffer->index);
        } else {
            SDL_Delay(10);
        }
    }

    free(x);
    free(fk);
    free(Ak);
}

/* *************************
* SDL Initialization Function
* *************************/
/* *************************
* SDL Initialization Function
* *************************/
int initialize_sdl_audio() {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec desiredRecordingSpec = {
        .freq = SAMP_FREQ, .format = FORMAT, .channels = MONO, .samples = ABUFSIZE_SAMPLES, .callback = audioRecordingCallback
    };

    int device_count = SDL_GetNumAudioDevices(SDL_TRUE);
    if (device_count < 1) {
        printf("Unable to get audio capture device! SDL Error: %s\n", SDL_GetError());
        return 1;
    }

    for (int i = 0; i < device_count; ++i) printf("%d - %s\n", i, SDL_GetAudioDeviceName(i, SDL_TRUE));

    int index;
    printf("Choose audio device: ");
    scanf("%d", &index);
    if (index < 0 || index >= device_count) {
        printf("Invalid device ID. Must be between 0 and %d\n", device_count - 1);
        return 1;
    }

    recordingDeviceId = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(index, SDL_TRUE), SDL_TRUE, &desiredRecordingSpec, &gReceivedRecordingSpec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (recordingDeviceId == 0) {
        printf("Failed to open recording device! SDL Error: %s\n", SDL_GetError());
        return 1;
    }

    int bytesPerSample = gReceivedRecordingSpec.channels * (SDL_AUDIO_BITSIZE(gReceivedRecordingSpec.format) / 8);
    int bytesPerSecond = gReceivedRecordingSpec.freq * bytesPerSample;
    gBufferByteSize = RECORDING_BUFFER_SECONDS * bytesPerSecond;
    gBufferByteMaxPosition = MAX_RECORDING_SECONDS * bytesPerSecond;

    return 0;
}
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
// Ensure `audioPlaybackCallback` is defined as follows:
void audioPlaybackCallback(void* userdata, Uint8* stream, int len) {
    // Implementation of the callback function
    memcpy(stream, &gRecordingBuffer[gBufferBytePosition], len);
    gBufferBytePosition += len;
}
/* *************************
* Main Function
* *************************/
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
            for(int i = 0; i < 6; i++, idx++) {
                priorities[i] = atoi(argv[idx]);
                if (priorities[i] == 0) {
                    fprintf(stderr, "Erro no argumento %s\n", argv[idx]);
                    usage();
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
    }

    int max_priority = sched_get_priority_max(SCHED_FIFO);
    int min_priority = sched_get_priority_min(SCHED_FIFO);

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

   
    void audioPlaybackCallback(void* userdata, Uint8* stream, int len);

    // Aumentar o tamanho do buffer de áudio
    // Update desired recording and playback specifications
    SDL_AudioSpec desiredRecordingSpec = {
        .freq = SAMP_FREQ,
        .format = FORMAT,
        .channels = MONO,
        .samples = ABUFSIZE_SAMPLES,
        .callback = audioRecordingCallback
    };
    SDL_AudioSpec desiredPlaybackSpec = {
        .freq = SAMP_FREQ,
        .format = FORMAT,
        .channels = MONO,
        .samples = ABUFSIZE_SAMPLES,
        .callback = audioPlaybackCallback
    };

    int gRecordingDeviceCount = SDL_GetNumAudioDevices(SDL_TRUE);
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
        printf("Buffer %d, nusers: %d, Index: %d\n", i, cab_buffer.buflist[i].nusers, cab_buffer.buflist[i].index);
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Initialize thread attributes and create threads
    pthread_attr_init(&attr1);
    pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);
    parm1.sched_priority = priorities[0];
    pthread_attr_setschedparam(&attr1, &parm1);
    err = pthread_create(&thread1, &attr1, Audio_thread, &procname1);
    if (err != 0) {
        printf("\n\r Error creating Thread [%s]", strerror(err));
        return 1;
    }

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
* Auxiliary Functions
* ************************************************/
buffer* cab_getWriteBuffer(cab* c) {
    for (int i = 0; i < NTASKS + 1; i++) {
        if (c->buflist[i].nusers == 0) {
            pthread_mutex_lock(&c->buflist[i].bufMutex);
            return &c->buflist[i];
        }
    }
    return NULL;  // Return NULL if no available buffer is found
}

buffer* cab_getReadBuffer(cab* c) {
    while (pthread_mutex_trylock(&c->buflist[c->last_write].bufMutex) != 0) {
        SDL_Delay(10);  // Add a small delay to reduce CPU usage
    }
    c->buflist[c->last_write].nusers += 1;
    pthread_mutex_unlock(&c->buflist[c->last_write].bufMutex);
    return &c->buflist[c->last_write];
}

void cab_releaseWriteBuffer(cab* c, uint8_t index) {
    pthread_mutex_unlock(&c->buflist[index].bufMutex);
    c->last_write = index;
}

void cab_releaseReadBuffer(cab* c, uint8_t index) {
    c->buflist[index].nusers--;
    pthread_mutex_unlock(&c->buflist[index].bufMutex);
}

void init_cab(cab *cab_obj) {
    cab_obj->last_write = 0;
    for (int i = 0; i < NTASKS + 1; i++) {
        memset(cab_obj->buflist[i].buf, 0, sizeof(cab_obj->buflist[i].buf));
        cab_obj->buflist[i].nusers = 0;
        cab_obj->buflist[i].index = i;
        pthread_mutex_init(&cab_obj->buflist[i].bufMutex, NULL);
    }
}

void audioRecordingCallback(void* userdata, Uint8* stream, int len) {
    if (gBufferBytePosition + len <= gBufferByteSize) {
        memcpy(&gRecordingBuffer[gBufferBytePosition], stream, len);
        gBufferBytePosition += len;

        // Salvar temporariamente os dados de áudio
        save_audio_to_wav("raw_output.wav", gRecordingBuffer, gBufferBytePosition, gReceivedRecordingSpec.freq);
        printf("Áudio salvo em raw_output.wav para verificação.\n");
    } else {
        printf("Buffer is full. Cannot write more data.\n");
    }
}

void filterLP(uint32_t cof, uint32_t sampleFreq, uint8_t *buffer, uint32_t nSamples) {
    uint16_t *procBuffer = malloc(nSamples * sizeof(uint16_t));
    uint16_t *origBuffer = (uint16_t *)buffer;
    float alfa = (2 * M_PI / sampleFreq * cof) / ((2 * M_PI / sampleFreq * cof) + 1);
    float beta = 1 - alfa;

    procBuffer[0] = origBuffer[0];
    for (int i = 1; i < nSamples; i++) {
        procBuffer[i] = alfa * origBuffer[i] + beta * procBuffer[i - 1];
    }

    memcpy(buffer, (uint8_t *)procBuffer, nSamples * sizeof(uint16_t));
    free(procBuffer);
}

void save_audio_to_wav(const char* filename, Uint8* buffer, Uint32 buffer_size, int sample_rate) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        printf("Error opening file for writing\n");
        return;
    }

    // Write the WAV header
    fwrite("RIFF", 1, 4, file);
    int32_t chunk_size = buffer_size + 36;
    fwrite(&chunk_size, 4, 1, file);
    fwrite("WAVE", 1, 4, file);
    fwrite("fmt ", 1, 4, file);

    int32_t subchunk1_size = 16;
    fwrite(&subchunk1_size, 4, 1, file);

    int16_t audio_format = 1; // PCM
    fwrite(&audio_format, 2, 1, file);
    int16_t num_channels = 1; // Mono
    fwrite(&num_channels, 2, 1, file);
    fwrite(&sample_rate, 4, 1, file);
    int32_t byte_rate = sample_rate * num_channels * sizeof(uint16_t);
    fwrite(&byte_rate, 4, 1, file);
    int16_t block_align = num_channels * sizeof(uint16_t);
    fwrite(&block_align, 2, 1, file);
    int16_t bits_per_sample = 16;
    fwrite(&bits_per_sample, 2, 1, file);

    fwrite("data", 1, 4, file);
    fwrite(&buffer_size, 4, 1, file);

    // Write the audio data
    fwrite(buffer, 1, buffer_size, file);

    fclose(file);
}

/* ***********************************************
 * Debug function: Prints the buffer contents - 8 bit samples
 * ***********************************************/
void printSamplesU8(uint8_t *buffer, int size) {
    printf("\nSamples (8-bit): \n");
    for (int i = 0; i < size; i++) {
        printf("%3u ", buffer[i]);
        if ((i + 1) % 20 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

/* ***********************************************
 * Debug function: Prints the buffer contents - 16 bit samples
 * ***********************************************/
void printSamplesU16(uint8_t *buffer, int nsamples) {
    printf("\nSamples (16-bit): \n");
    uint16_t *bufu16 = (uint16_t *)buffer;
    for (int i = 0; i < nsamples; i++) {
        printf("%5u ", bufu16[i]);
        if ((i + 1) % 20 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}
