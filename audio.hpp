//
// Created by felix on 15.04.25.
//

#ifndef AUDIO_HPP
#define AUDIO_HPP

#define FRAMES_PER_BUFFER 2048
#define FFW_BANDS (FRAMES_PER_BUFFER/2+1)
#define LOG_BANDS 128
#define LOG_MIN_FREQ 20

#include <portaudio.h>
#include <fftw3.h>
#include <memory>
#include <vector>

class Audio {
    PaDeviceIndex deviceIndex = paNoDevice;
    PaStream *stream = nullptr;
    int sampleRate = 0;

    void initPortAudio();
    void initFFTW();

    void computeLogBands();
public:
    void init();
    void start();
    void stop() const;
    
    bool running = true;

    fftwf_complex* result = nullptr;
    std::shared_ptr<std::vector<float>> logResult = std::make_shared<std::vector<float>>(LOG_BANDS);
    float amplitude;
};



#endif //AUDIO_HPP
