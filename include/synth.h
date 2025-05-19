#ifndef SYNTH_H
#define SYNTH_H

#include "raylib.h"
#include "resource_dir.h"
#include <stdlib.h>
#include "raymath.h"
#include <math.h>
#include <string.h>


#define MAX_SAMPLES 128

#define SAMPLERATE 480000
#define OFF_THRESHOLD 0.001f
#define BIT(x)(1 << (x))

typedef enum
{
    IDLE,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE
} ADSRState;

typedef struct
{
    float attackRate;
    float decayRate;
    float sustain;
    float releaseRate;
} ADSR;

typedef struct 
{
    float cutoff;
    float resonance;
    float buf0, buf1;
} LowpassFilter;

typedef struct
{
    KeyboardKey keyboardKey;
    int midiNote;
    float frequency;
    float freqTimesTau;
    float carrierPhaseInc;
    float modulatorPhaseInc;

    float waveIdx;
    float modIdx;
    bool isNoteOn;

    float volumeADSRValue;
    ADSRState volumeADSRState;
    
    float fmADSRValue;
    ADSRState fmADSRState;
    
    float filterADSRValue;
    ADSRState filterADSRState;
    LowpassFilter lpf;

} Voice;

typedef struct
{
    float volume;
    float panning;
    float fmAmplitude;
    float fmRatioCoarse;
    float fmRatioFine;
    float cutoff;
    float resonance;
    float filterEnvelopeDepth;
    int octave;
    ADSR volumeEnvelope;
    ADSR fmEnvelope;
    ADSR filterEnvelope;
    Voice voices[32];
    unsigned int voiceMask;
} Synthesizer;

int KeyboardKeyToMIDI(KeyboardKey key, int octave);
int AllocateVoice(Synthesizer *synth);
int FreeVoice(Synthesizer *synth, int index);
void NoteOn(Synthesizer *synth, KeyboardKey key, float velocity);
void NoteOff(Synthesizer *synth, int midiNote);
void HandleADSR(float *value, ADSRState *state, ADSR *adsr);

#endif
