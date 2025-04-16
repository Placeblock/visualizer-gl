#version 430 core

#define M_PI 3.1415926535897932384626433832795
#define FRAMES_PER_BUFFER 1024
#define BINS (FRAMES_PER_BUFFER/2+1)
#define LOG_BANDS 128

uniform lowp float time;
uniform lowp float amplitude;
uniform lowp vec2 res;

layout(std430, binding = 0) buffer fft {
    vec2 bins[BINS];
};
layout(std430, binding = 1) buffer log_fft {
    float log_bands[LOG_BANDS];
};

layout(location = 0) out vec4 diffuseColor;

void main() {
    float xFraction = float(gl_FragCoord.x) / res.x;
    float yFraction = float(gl_FragCoord.y) / res.y;
    int binIndex = int(floor((xFraction)*LOG_BANDS));
    float band = log2(log_bands[binIndex]*amplitude*(pow(xFraction*2+0.5, 2)))+pow(amplitude*5+1, 3);
    band = log(log_bands[binIndex]+1)/4; // Only 
    band = band*(xFraction+0.2*2);
    band = band/2;
    band = band * pow(amplitude*2+1.5, 2);
    //bin = bin*(xFraction*2+0.5);
    //vec2 bin = log_bins[binIndex].xy;
    //lowp float magnitude = sqrt(bin.x*bin.x + bin.y*bin.y) / FRAMES_PER_BUFFER;
    //lowp float displayMag = magnitude * res.y * 35;
    
    //bool lit = bool(gl_FragCoord.y < displayMag);
    bool lit = bool(gl_FragCoord.y < band*res.y);

    //diffuseColor = vec4(band, lit ? gl_FragCoord.y/res.y : 0, lit ? gl_FragCoord.y/res.y : 0, 1.0);
    diffuseColor = vec4(lit ? 1 : 0, lit ? yFraction : 0, lit ? yFraction : 0, 1.0);
}