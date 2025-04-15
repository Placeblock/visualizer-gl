//
// Created by felix on 15.04.25.
//

#include <portaudio.h>
#include <fftw3.h>

#include "audio.hpp"

#include <cmath>

#include "colorcli.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

void Audio::initPortAudio() {
    
    if (const PaError err = Pa_Initialize(); err != paNoError) {
        throw std::runtime_error("Cannot initialize PortAudio: " + std::to_string(err));
    }

    const int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        Pa_Terminate();
        throw std::runtime_error("Cannot get Device Count.");
    }
    std::vector<PaDeviceIndex> inputDevices;
    for (int i = 0; i < numDevices; i++) {
        if (const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i); deviceInfo->maxInputChannels > 0) {
            inputDevices.push_back(i);
        }
    }

    printf("Found %s%zd%s devices: \n", CLI_RED, inputDevices.size(), CLI_RESET);
    for (size_t i = 0; i < inputDevices.size(); i++) {
        const PaDeviceIndex devIndex = inputDevices[i];
        printf(CLI_BLUE "%zd%s: %s%s%s\n", i+1, CLI_RESET, CLI_YELLOW, Pa_GetDeviceInfo(devIndex)->name, CLI_RESET);
    }
    printf("Select Device Numer: ");
    int selectedDeviceNr;
    std::cin >> selectedDeviceNr;

    this->deviceIndex = inputDevices[selectedDeviceNr-1];

    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(this->deviceIndex);
    printf("Selected Device %d: %s\n", selectedDeviceNr, deviceInfo->name);
}

void Audio::initFFTW() {
    this->result = static_cast<fftwf_complex *>(fftwf_malloc(sizeof(fftwf_complex) * (FFW_BANDS)));
}

void Audio::computeLogBands() {
    const float maxFreq = static_cast<float>(this->sampleRate) / 2;
    
    std::vector<float> bandEdges(LOG_BANDS+1);
    for (int i = 0; i <= LOG_BANDS; ++i) {
        const float fraction = static_cast<float>(i) / LOG_BANDS;
        bandEdges[i] = LOG_MIN_FREQ * std::pow(maxFreq / LOG_MIN_FREQ, fraction);
    }

    const float binHz = static_cast<float>(this->sampleRate) / FRAMES_PER_BUFFER;

    for (int band = 0; band < LOG_BANDS; ++band) {
        const float lowFreq = bandEdges[band];
        const float highFreq = bandEdges[band + 1];

        const int startBin = std::ceil(lowFreq / binHz);
        const int endBin = std::floor(highFreq / binHz);

        float sum = 0.0;
        int count = 0;

        for (int i = startBin; i <= endBin && i < FFW_BANDS; ++i) {
            const float magnitude = std::sqrt(this->result[i][0]*this->result[i][0]+this->result[i][1]*this->result[i][1]);
            sum += magnitude;
            ++count;
        }

        if (count > 0)
            (*this->logResult)[band] = sum / static_cast<float>(count); // or just sum, or max, depending on your goal
        else
            (*this->logResult)[band] = 0.0;
    }
}

void Audio::init() {
    this->initPortAudio();
    this->initFFTW();
}

void Audio::start() {
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(this->deviceIndex);
    
    PaStreamParameters inputParameters;
    inputParameters.device = this->deviceIndex;
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;

    this->sampleRate = deviceInfo->defaultSampleRate;
    
    if (const PaError err = Pa_OpenStream(&this->stream, &inputParameters, nullptr,
            sampleRate, FRAMES_PER_BUFFER, paClipOff,
        nullptr, nullptr); err != paNoError) {
        throw std::runtime_error("Cannot open Audio Stream: " + std::to_string(err));
    }

    if (const PaError err = Pa_StartStream(this->stream); err != paNoError) {
        this->stop();
        throw std::runtime_error("Cannot start Audio Stream: " + std::to_string(err));
    }

    float paBuffer[FRAMES_PER_BUFFER*deviceInfo->maxInputChannels];
    fftwf_plan plan = fftwf_plan_dft_r2c_1d(FRAMES_PER_BUFFER, paBuffer, this->result, FFTW_EXHAUSTIVE | FFTW_NO_BUFFERING | FFTW_NO_SLOW);

    while (this->running) {
        if (const PaError err = Pa_ReadStream(this->stream, paBuffer, FRAMES_PER_BUFFER)) {
            this->stop();
            throw std::runtime_error("Cannot read Audio Stream: " + std::to_string(err));
        }
        *this->amplitude = 0;
        for (int i = 0; i < FRAMES_PER_BUFFER; ++i) {
            *this->amplitude = std::max(*this->amplitude, std::abs(paBuffer[i]));
        }

        fftwf_execute(plan);

        this->computeLogBands();

        Pa_Sleep(10);
        //printf("Read %s%d%s Frames\n", CLI_GREEN, FRAMES_PER_BUFFER, CLI_RESET);
    }

    this->stop();
}

void Audio::stop() const {
    if (const PaError err = Pa_StopStream(this->stream); err != paNoError) {
        throw std::runtime_error("Cannot stop Audio Stream: " + std::to_string(err));
    }
    if (const PaError err = Pa_CloseStream(this->stream); err != paNoError) {
        throw std::runtime_error("Cannot close Audio Stream: " + std::to_string(err));
    }
    Pa_Terminate();

    fftwf_free(this->result);
}
