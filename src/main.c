
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "synth.h"

Synthesizer synth;

#define MAX_SAMPLES_PER_UPDATE 8
#define HALF_PI 1.5707963267948966
#define TAU 6.283185307179586f

int MAX(int a, int b) { return ((a) > (b) ? a : b); }
int MIN(int a, int b) { return ((a) < (b) ? a : b); }

static inline float FastSin(float x)
{
    x -= TAU * floorf((x + PI) / TAU);
    float sign = 1.0f - 2.0f * (x < 0.0f);
    x = fabsf(x);
    x = fminf(x, PI - x);
    float x2 = x * x;
    return sign * x * (1.0f - x2 * (1.0f / 6.0f) + x2 * x2 * (1.0f / 120.0f));
}

static inline float ProcessFilter(LowpassFilter *f, float input)
{
    float feedback = f->resonance * (1.0f - 0.15f * f->cutoff * f->cutoff);

    float buf0 = f->buf0;
    float buf1 = f->buf1;

    buf0 += f->cutoff * (input - buf0 + feedback * (buf0 - buf1));
    buf1 += f->cutoff * (buf0 - buf1);

    f->buf0 = buf0;
    f->buf1 = buf1;

    return buf1;
}

void AudioInputCallback(void * buffer, unsigned int frames)
{
    short *d = (short *)buffer;

    for (unsigned int i = 0; i < frames; i++)
    {
        float mix = 0.0f;
        for (int v = 0; v < 32; v++)
        {
            Voice *voice = &synth.voices[v];
            
            if(voice->volumeADSRState == IDLE)
            {
                continue;
            }
                
            HandleADSR(&voice->volumeADSRValue, &voice->volumeADSRState, &synth.volumeEnvelope);
            HandleADSR(&voice->fmADSRValue, &voice->fmADSRState, &synth.fmEnvelope);
            HandleADSR(&voice->filterADSRValue, &voice->filterADSRState, &synth.filterEnvelope);

            float freqTimesTau = TAU * voice->frequency;
            float carrierPhaseInc = freqTimesTau / SAMPLERATE;
            float modulatorPhaseInc = freqTimesTau * (synth.fmRatioCoarse + synth.fmRatioFine) / SAMPLERATE;

            float modSignal = FastSin(voice->modIdx) * voice->fmADSRValue * synth.fmAmplitude;
            float sample = FastSin(voice->waveIdx + modSignal);

            voice->waveIdx += carrierPhaseInc;
            voice->modIdx += modulatorPhaseInc;

            voice->waveIdx -= (voice->waveIdx >= TAU) * TAU;
            voice->modIdx -= (voice->modIdx >= TAU) * TAU;

            float cutoff = synth.cutoff + voice->filterADSRValue * synth.filterEnvelopeDepth;
            cutoff = Clamp(cutoff, 0.0f, 1.0f);

            voice->lpf.cutoff = cutoff;
            voice->lpf.resonance = synth.resonance;

            sample = ProcessFilter(&voice->lpf, sample);
            mix += (sample * voice->volumeADSRValue) * 0.1f;
        }
        d[i] = (short)(Clamp(mix, -1.0f, 1.0f) * SHRT_MAX);
    }
}


int main ()
{
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
	InitWindow(1280, 800, "rlSynth");
	
    SearchAndSetResourceDir("resources");
    SetTargetFPS(60);
    InitAudioDevice();
    SetAudioStreamBufferSizeDefault(MAX_SAMPLES_PER_UPDATE);
    AudioStream audioStream = LoadAudioStream(SAMPLERATE, 16, 1);
    SetAudioStreamCallback(audioStream, AudioInputCallback);
    PlayAudioStream(audioStream);
    SetAudioStreamVolume(audioStream, 0.7f);
    
    synth.volume = 0.6f;
    synth.octave = 4;
    synth.fmAmplitude = 8.0f;
    synth.fmRatioCoarse = 1.0f;
    synth.fmRatioFine = 0.0f;
    synth.volumeEnvelope = (ADSR)
    {
        .attackRate = 0.001f,
        .decayRate = 10.0f,
        .sustain = 0.5f,
        .releaseRate = 0.2f
    };
    synth.cutoff = 0.001f;
    synth.resonance = 1.7f;
    synth.filterEnvelopeDepth = 0.02f;
    synth.filterEnvelope = (ADSR)
    {
        .attackRate = 0.001f,
        .decayRate = 1.0f,
        .sustain = 0.1f,
        .releaseRate = 0.2f
    };
    synth.fmEnvelope = (ADSR)
    {
        .attackRate = 0.001f,
        .decayRate = 4.0f,
        .sustain = 0.1f,
        .releaseRate = 0.2f
    };

    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(RAYWHITE));

    while (!WindowShouldClose())
    {
        KeyboardKey key;
        while ((key = GetKeyPressed()) != 0)
        {
            NoteOn(&synth, key, 1.0f);
        }
        for (int i = 0; i < 32; i++)
        {
            Voice *voice = &synth.voices[i];

            if (voice->volumeADSRState == IDLE)
            {

                FreeVoice(&synth, i);
                continue;
            }

            if (IsKeyUp(voice->keyboardKey))
            {
                NoteOff(&synth, voice->midiNote);
            }
        }

        if (IsKeyPressed(KEY_X))
        {
            synth.octave = MIN(synth.octave + 1, 8);
        }
        else if (IsKeyPressed(KEY_Z))
        {
            synth.octave = MAX(synth.octave - 1, -2);
        }

        BeginDrawing();
		ClearBackground((Color){10,10,10,10});

        GuiSlider((Rectangle){190, 160, 200, 20}, "VCA Attack", TextFormat("%.2f", synth.volumeEnvelope.attackRate), &synth.volumeEnvelope.attackRate, 0.001f, 10.0f);
        GuiSlider((Rectangle){190, 200, 200, 20}, "VCA Decay", TextFormat("%.2f", synth.volumeEnvelope.decayRate), &synth.volumeEnvelope.decayRate, 0.01f, 10.0f);
        GuiSlider((Rectangle){190, 240, 200, 20}, "VCA Sustain", TextFormat("%.2f", synth.volumeEnvelope.sustain), &synth.volumeEnvelope.sustain, 0.0f, 1.0f);
        GuiSlider((Rectangle){190, 280, 200, 20}, "VCA Release", TextFormat("%.2f", synth.volumeEnvelope.releaseRate), &synth.volumeEnvelope.releaseRate, 0.01f, 1.0f);

        GuiSlider((Rectangle){190, 360, 200, 20}, "FLT Cutoff", TextFormat("%.2f", synth.cutoff * 10000), &synth.cutoff, 0.0f, 0.12f);
        GuiSlider((Rectangle){190, 400, 200, 20}, "FLT Resonance", TextFormat("%.2f", synth.resonance), &synth.resonance, 0.0f, 2.0f);

        GuiSlider((Rectangle){190, 480, 200, 20}, "FLT Env Depth", TextFormat("%.2f", synth.filterEnvelopeDepth), &synth.filterEnvelopeDepth, 0.0f, 0.1f);
        GuiSlider((Rectangle){190, 520, 200, 20}, "FLT Attack", TextFormat("%.2f", synth.filterEnvelope.attackRate), &synth.filterEnvelope.attackRate, 0.001f, 10.0f);
        GuiSlider((Rectangle){190, 560, 200, 20}, "FLT Decay", TextFormat("%.2f", synth.filterEnvelope.decayRate), &synth.filterEnvelope.decayRate, 0.01f, 10.0f);
        GuiSlider((Rectangle){190, 600, 200, 20}, "FLT Sustain", TextFormat("%.2f", synth.filterEnvelope.sustain), &synth.filterEnvelope.sustain, 0.0f, 1.0f);
        GuiSlider((Rectangle){190, 640, 200, 20}, "FLT Release", TextFormat("%.2f", synth.filterEnvelope.releaseRate), &synth.filterEnvelope.releaseRate, 0.01f, 1.0f);

        GuiSlider((Rectangle){GetScreenWidth() - 330, 360, 200, 20}, "FM Attack", TextFormat("%.2f", synth.fmEnvelope.attackRate), &synth.fmEnvelope.attackRate, 0.001f, 10.0f);
        GuiSlider((Rectangle){GetScreenWidth() - 330, 400, 200, 20}, "FM Decay", TextFormat("%.2f", synth.fmEnvelope.decayRate), &synth.fmEnvelope.decayRate, 0.01f, 10.0f);
        GuiSlider((Rectangle){GetScreenWidth() - 330, 440, 200, 20}, "FM Sustain", TextFormat("%.2f", synth.fmEnvelope.sustain), &synth.fmEnvelope.sustain, 0.0f, 1.0f);
        GuiSlider((Rectangle){GetScreenWidth() - 330, 480, 200, 20}, "FM Release", TextFormat("%.2f", synth.fmEnvelope.releaseRate), &synth.fmEnvelope.releaseRate, 0.01f, 1.0f);

        GuiSliderBar((Rectangle){GetScreenWidth() - 330, 160, 200, 20}, "FM Amplitude", TextFormat("%.2f", synth.fmAmplitude), &synth.fmAmplitude, 0.0f, 16.0f);
        GuiSliderBar((Rectangle){GetScreenWidth() - 330, 200, 200, 20}, "FM Coarse", TextFormat("%.2f", synth.fmRatioCoarse), &synth.fmRatioCoarse, 0.125f, 8.0f);
        GuiSliderBar((Rectangle){GetScreenWidth() - 330, 240, 200, 20}, "FM Fine", TextFormat("%.2f", synth.fmRatioFine * 1000), &synth.fmRatioFine, -0.1f, 0.1f);

        DrawText(TextFormat("Octave: %d", synth.octave), GetScreenWidth() - 310, 280, 20, WHITE);
        DrawText(TextFormat("Filt Env: %.4f", synth.voices[0].filterADSRValue), GetScreenWidth() - 310, 320, 20, WHITE);


        synth.fmRatioCoarse = (float)((int)(synth.fmRatioCoarse * 10)) / 10.0f;

        int yOffset = 1;

        for (int i = 0; i < 32; i++)
        {
            Voice *voice = &synth.voices[i];

            Color c = ColorLerp((Color){69, 69, 69, 255}, RED, voice->volumeADSRValue);

            DrawRectangle((GetScreenWidth() - 430) + (10 * yOffset), 40, 8, 8, c);
            // DrawText(TextFormat("%.4f, phaseinc: %.4f", voice->fmADSRValue, phaseInc), 140, 40 * yOffset, 20, WHITE);
            yOffset++;
        }
        EndDrawing();
    }
    UnloadAudioStream(audioStream);
    CloseAudioDevice();
    CloseWindow();
	return 0;
}
