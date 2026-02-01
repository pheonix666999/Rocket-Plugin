#pragma once

#include <JuceHeader.h>

class DemoFxChain
{
public:
    explicit DemoFxChain(juce::AudioProcessorValueTreeState& state);

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void process(juce::AudioBuffer<float>& buffer,
                 juce::AudioPlayHead* playHead);

private:
    juce::AudioProcessorValueTreeState& apvts;

    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    int numChannels = 2;

    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;

    // ---------- Helpers ----------
    static float dbToLin(float db) { return juce::Decibels::decibelsToGain(db); }

    // ---------- Filters / EQ ----------
    struct Eq4
    {
        void prepare(double sr, int ch, int maxSamples);
        void reset();
        void process(juce::AudioBuffer<float>& buffer);

        void setEnabled(bool e) { enabled = e; }
        void setCuts(float lowCutHz, float highCutHz);
        void setBand(int bandIndex0, float freqHz, float gainDb, float q);

        bool enabled = true;
        double sampleRate = 44100.0;
        int numChannels = 2;

        std::array<juce::dsp::IIR::Filter<float>, 6> filtersL;
        std::array<juce::dsp::IIR::Filter<float>, 6> filtersR;
        std::array<juce::dsp::IIR::Coefficients<float>::Ptr, 6> coeffs;
    };

    Eq4 preEq;
    Eq4 postEq;

    // ---------- Compressor ----------
    struct Comp
    {
        void prepare(double sr, int maxSamples, int ch);
        void reset();
        void process(juce::AudioBuffer<float>& buffer);

        void setEnabled(bool e) { enabled = e; }
        void setParams(float thresholdDb, float ratio, float attackMs, float releaseMs);
        void setInOutGain(float inDb, float outDb);

        bool enabled = false;
        juce::dsp::Compressor<float> comp;
        float inGain = 1.0f;
        float outGain = 1.0f;
    };

    Comp preComp;
    Comp postComp;

    // ---------- De-esser (simple HF compressor) ----------
    struct DeEsser
    {
        void prepare(double sr, int maxSamples, int ch);
        void reset();
        void process(juce::AudioBuffer<float>& buffer);

        void setEnabled(bool e) { enabled = e; }
        void setParams(float freqHz, float thresholdDb);

        bool enabled = false;
        double sampleRate = 44100.0;

        juce::dsp::IIR::Filter<float> hpL, hpR;
        juce::dsp::IIR::Coefficients<float>::Ptr hpCoef;
        juce::dsp::Compressor<float> comp;
    };

    DeEsser deesser;

    // ---------- Delay ----------
    struct Delay
    {
        void prepare(double sr, int maxSamples);
        void reset();
        void process(juce::AudioBuffer<float>& buffer,
                     double bpm);

        void setEnabled(bool e) { enabled = e; }
        void setParams(int type, bool sync, int rhythm, float timeMs, float feedback, float mix,
                       float hpHz, float lpHz, float lfoRate, float lfoDepth);

        bool enabled = true;
        double sampleRate = 44100.0;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dl { 192000 };
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dr { 192000 };

        juce::dsp::IIR::Filter<float> hpL, hpR, lpL, lpR;
        juce::dsp::IIR::Coefficients<float>::Ptr hpCoef, lpCoef;

        float phase = 0.0f;
        float fbStateL = 0.0f;
        float fbStateR = 0.0f;

        int type = 0; // 0=digital,1=pingpong,2=tape
        bool sync = false;
        int rhythm = 2;
        float timeMs = 250.0f;
        float feedback = 0.3f;
        float mix = 0.0f;
        float hpHz = 20.0f;
        float lpHz = 20000.0f;
        float lfoRate = 0.0f;
        float lfoDepth = 0.0f;
    };

    Delay delay1;
    Delay delay2;

    // ---------- Mod FX ----------
    struct Flanger
    {
        void prepare(double sr, int maxSamples);
        void reset();
        void process(juce::AudioBuffer<float>& buffer);

        void setEnabled(bool e) { enabled = e; }
        void setParams(float rateHz, float intensity, float feedback, float mix);

        bool enabled = true;
        double sampleRate = 44100.0;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dl { 4096 };
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dr { 4096 };
        float phase = 0.0f;
        float rateHz = 0.25f;
        float intensity = 0.0f;
        float feedback = 0.0f;
        float mix = 0.0f;
    };

    struct Phaser
    {
        void prepare(double sr, int maxSamples, int ch);
        void reset();
        void process(juce::AudioBuffer<float>& buffer);

        void setEnabled(bool e) { enabled = e; }
        void setParams(float rateHz, float intensity, float depth, float mix);

        bool enabled = true;
        juce::dsp::Phaser<float> phaser;
        float rateHz = 0.2f;
        float intensity = 0.0f;
        float depth = 0.25f;
        float mix = 0.0f;
    };

    Flanger flanger;
    Phaser phaser;

    // ---------- Distortion ----------
    struct Distortion
    {
        void prepare(double sr, int maxSamples, int ch);
        void reset();
        void process(juce::AudioBuffer<float>& buffer);

        void setEnabled(bool e) { enabled = e; }
        void setParams(float drive1, float drive2, float mix1, float mix2);

        bool enabled = true;
        float drive1 = 1.0f;
        float drive2 = 1.0f;
        float mix1 = 0.0f;
        float mix2 = 0.0f;
    };

    Distortion distortion;

    // ---------- Bit crush ----------
    struct BitCrush
    {
        void prepare(double sr, int maxSamples, int ch);
        void reset();
        void process(juce::AudioBuffer<float>& buffer);

        void setParams(float depth, float freq, float hard, float mix);

        float depth = 0.0f;
        float freq = 1.0f;
        float hard = 0.0f;
        float mix = 0.0f;

        int counter = 0;
        float heldL = 0.0f;
        float heldR = 0.0f;
    };

    BitCrush bitcrush;

    // ---------- Reverb ----------
    struct Reverb
    {
        void prepare(double sr, int maxSamples, int ch);
        void reset();
        void process(juce::AudioBuffer<float>& buffer);

        void setEnabled(bool e) { enabled = e; }
        void setParams(int type, float decaySeconds, float predelayMs, float mix);

        bool enabled = true;
        double sampleRate = 44100.0;

        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelay { 48000 };
        juce::dsp::Reverb reverb;
        juce::dsp::Convolution convolution;

        int type = 0;
        float decaySeconds = 0.5f;
        float predelayMs = 0.0f;
        float mix = 0.0f;
    };

    Reverb reverb;

    // ---------- Pitch shifter (simple dual-delay shifter) ----------
    struct PitchShifter
    {
        void prepare(double sr, int maxSamples, int ch);
        void reset();
        void process(juce::AudioBuffer<float>& buffer);

        void setEnabled(bool e) { enabled = e; }
        void setParams(float semitones, float mix);

        bool enabled = false;
        double sampleRate = 44100.0;
        int numChannels = 2;

        juce::AudioBuffer<float> ring;
        int writePos = 0;
        int ringSize = 0;

        float semitones = 0.0f;
        float mix = 0.0f;
        float readPosA = 0.0f;
        float readPosB = 0.0f;

        float speed = 1.0f;
    };

    PitchShifter pitch;

    // ---------- Chain helpers ----------
    double lastBpm = 120.0;
    static double getBpmOrDefault(juce::AudioPlayHead* playHead, double fallback);
};
