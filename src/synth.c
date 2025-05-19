#include "synth.h"

int KeyboardKeyToMIDI(KeyboardKey key, int octave)
{
    int midiNote;
    switch (key)
    {
    case KEY_A:
        midiNote = 24; // C4
        break;
    case KEY_W:
        midiNote = 25;
        break;
    case KEY_S:
        midiNote = 26;
        break;
    case KEY_E:
        midiNote = 27;
        break;
    case KEY_D:
        midiNote = 28;
        break;
    case KEY_F:
        midiNote = 29;
        break;
    case KEY_T:
        midiNote = 30;
        break;
    case KEY_G:
        midiNote = 31;
        break;
    case KEY_Y:
        midiNote = 32;
        break;
    case KEY_H:
        midiNote = 33; // A4 = 440 Hz
        break;
    case KEY_U:
        midiNote = 34;
        break;
    case KEY_J:
        midiNote = 35;
        break;
    case KEY_K:
        midiNote = 36; // C5
        break;
    case KEY_O:
        midiNote = 37;
        break;
    case KEY_L:
        midiNote = 38;
        break;
    case KEY_P:
        midiNote = 39;
        break;
    case KEY_SEMICOLON:
        midiNote = 40;
        break;
    case KEY_APOSTROPHE:
        midiNote = 41;
        break;
    default:
        return -1;
    }

    return midiNote + (12 * octave);
}

float MIDIToFrequency(int midiNote)
{
    return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
}

int AllocateVoice(Synthesizer *synth)
{
    for (int i = 0; i < 32; i++)
    {
        if (!(synth->voiceMask & (1U << i))) // voice not in use
        {
            synth->voiceMask |= (1U << i); // mark as used
            return i;                      
        }
    }
    return -1;
}
int FreeVoice(Synthesizer *synth, int index)
{
    if (index >= 0 && index < 32)
        synth->voiceMask &= ~(1U << index); // clear bit
}

void NoteOn(Synthesizer *synth, KeyboardKey key, float velocity)
{
    int index = AllocateVoice(synth);
    if (index == -1)
        return;

    Voice *voice = &synth->voices[index];

    voice->keyboardKey = key;
    voice->midiNote = KeyboardKeyToMIDI(key, synth->octave);
    if(voice->midiNote < 0)
        return;
    voice->frequency = MIDIToFrequency(voice->midiNote);
    voice->waveIdx = 0.0f;
    voice->modIdx = 0.0f;
    voice->isNoteOn = true;

    voice->volumeADSRValue = 0.0f;
    voice->volumeADSRState = ATTACK;

    voice->fmADSRValue = 0.0f;
    voice->fmADSRState = ATTACK;

    voice->filterADSRValue = 0.0f;
    voice->filterADSRState = ATTACK;

    voice->lpf.cutoff = synth->cutoff;
    voice->lpf.resonance = synth->resonance;
    voice->lpf.buf0 = voice->lpf.buf1 = 0.0f;
}

void NoteOff(Synthesizer *synth, int midiNote)
{
    for (int i = 0; i < 32; i++)
    {
        if ((synth->voiceMask & (1U << i)) && synth->voices[i].isNoteOn && synth->voices[i].midiNote == midiNote)
        {
            Voice *voice = &synth->voices[i];
            voice->isNoteOn = false;
            voice->volumeADSRState = RELEASE;
            voice->fmADSRState = RELEASE;
            voice->filterADSRState = RELEASE;
        }
    }
}

void HandleADSR(float *value, ADSRState *state, ADSR *adsr)
{
    switch (*state)
    {
    case IDLE:
        break;
    case ATTACK:
        *value += (1.0f / (adsr->attackRate * SAMPLERATE));
        if (*value >= 1.0f)
        {
            *value = 1.0f;
            *state = DECAY;
        }
        break;
    case DECAY:
        *value -= ((1.0f - adsr->sustain) / (adsr->decayRate * SAMPLERATE));
        if (*value <= adsr->sustain)
        {
            *value = adsr->sustain;
            *state = SUSTAIN;
        }
        break;
    case SUSTAIN:
        break;
    case RELEASE:
        *value -= *value / (adsr->releaseRate * SAMPLERATE);
        if (*value < OFF_THRESHOLD)
        {
               
            *value = 0.0f;
            *state = IDLE;
        }
    }
}