/* ************************************************************ 
 * Paulo Pedreiras, pbrp@ua.pt 
 * 2024/Sept
 * 
 * C module to compute the Fast Fourier Transform (FFT) of an array
 * using the (recursive) Cooley-Tukey algorithm.  
 * ************************************************************/

#ifndef FFT_H
#define FFT_H
#define __USE_MISC
#define _GNU_SOURCE

#include <stdio.h>
#include <math.h>
#include <complex.h>
#include "fft.h"

/* *******************************************************************
 * Function to perform the FFT (recursive version) 
 * *******************************************************************/
void fftCompute(complex double *X, int N) {
    if (N <= 1) {
        return;  // Base case: FFT of size 1 is the same
    }

    // Split even and odd terms
    complex double even[N/2];
    complex double odd[N/2];

    // Debug: Verify if the input array is being split correctly
    printf("Splitting the input array into even and odd components:\n");
    for (int i = 0; i < N/2; i++) {
        even[i] = X[i * 2];
        odd[i] = X[i * 2 + 1];
        printf("Even[%d] = %.2f + %.2fi, Odd[%d] = %.2f + %.2fi\n", i, creal(even[i]), cimag(even[i]), i, creal(odd[i]), cimag(odd[i]));
    }

    // Recursively apply FFT to even and odd terms
    fftCompute(even, N/2);
    fftCompute(odd, N/2);

    // Combine results
    for (int k = 0; k < N/2; k++) {
        complex double t = cexp(-2.0 * I * M_PI * k / N) * odd[k];
        X[k]       = even[k] + t;
        X[k + N/2] = even[k] - t;
    }

    // Print FFT output for first 5 values as a debug check
    printf("FFT Output after combination (Complex):\n");
    for (int i = 0; i < 5 && i < N; i++) { // Ensure we don’t access out of bounds
        printf("FFT[%d] = %.2f + %.2fi\n", i, creal(X[i]), cimag(X[i]));
    }
}

/* **********************************************************
 *  Converts complex representation of FFT in amplitudes.
 *  Also generates the corresponding frequencies 
 * **********************************************************/
void fftGetAmplitude(complex double * X, int N, int fs, float * fk, float * Ak) {
    // Compute frequencies: from 0 (DC) to fs, over N bins
    for (int k = 0; k <= N / 2; k++) {
        fk[k] = (float)k * fs / N;
    }

    // Compute amplitudes
    Ak[0] = 1.0 / N * cabs(X[0]);  // DC component
    for (int k = 1; k < N / 2; k++) {
        Ak[k] = 2.0 / N * cabs(X[k]);
    }

    // Handle Nyquist component if N is even
    if (N % 2 == 0) {
        Ak[N / 2] = 1.0 / N * cabs(X[N / 2]);
    }

    // Debugging: print the computed amplitudes
    printf("Amplitudes after FFT:\n");
    for (int i = 0; i < 10 && i < N / 2; i++) {
        printf("Amplitude[%d]: %f\n", i, Ak[i]);
    }
}

/* ******************************************************
 *  Helper function to print complex arrays
 * ******************************************************/
void printComplexArray(complex double *X, int N) {
    for (int i = 0; i < N; i++) {
        printf("%g + %gi\n", creal(X[i]), cimag(X[i]));
    }
}

#endif
