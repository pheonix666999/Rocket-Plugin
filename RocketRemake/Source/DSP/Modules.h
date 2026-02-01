#pragma once

#include "FxModule.h"
#include "ModMatrix.h"
#include <array>

// =============================================================================
// REVERB MODULE - Multiple algorithms (Hall, Plate, Room, Chamber)
// =============================================================================
class ReverbModule : public FxModule
{
public:
    ReverbModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);

private:
    juce::Reverb reverb;
    double sampleRate = 44100.0;
};

// =============================================================================
// DELAY MODULE (Dual Delay with independent L/R timing and sync)
// =============================================================================
class DelayModule : public FxModule
{
public:
    DelayModule(juce::AudioProcessorValueTreeState& state, int delayIndex = 1);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, int delayIndex);

private:
    int index;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 192000 };
    float sampleRate = 44100.0f;
    float fbL = 0.0f;
    float fbR = 0.0f;
    juce::dsp::IIR::Filter<float> hpFilterL, hpFilterR;
    juce::dsp::IIR::Filter<float> lpFilterL, lpFilterR;
};

// =============================================================================
// FILTER MODULE - HP/LP with adjustable slope (6/12/24/96 dB)
// =============================================================================
class FilterModule : public FxModule
{
public:
    enum class Type { LowPass, HighPass };
    FilterModule(juce::AudioProcessorValueTreeState& state, Type t, const juce::String& id);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& id, Type type);

private:
    Type type;
    using FilterStage = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    std::array<FilterStage, 16> filters; // Support up to 96dB/oct (16 stages x 6dB)
    float sampleRate = 44100.0f;
};

// =============================================================================
// FLANGER MODULE
// =============================================================================
class FlangerModule : public FxModule
{
public:
    FlangerModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 2048 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 2048 };
    float sampleRate = 44100.0f;
    float phase = 0.0f;
};

// =============================================================================
// PHASER MODULE
// =============================================================================
class PhaserModule : public FxModule
{
public:
    PhaserModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);

private:
    juce::dsp::Phaser<float> phaser;
};

// =============================================================================
// BITCRUSHER MODULE (Bit reduction + Downsampling)
// =============================================================================
class BitcrusherModule : public FxModule
{
public:
    BitcrusherModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);

private:
    int downsampleCounter = 0;
    float heldSampleL = 0.0f;
    float heldSampleR = 0.0f;
};

// =============================================================================
// DISTORTION MODULE (Multiple algorithms: Soft, Hard, Tape, Tube)
// =============================================================================
class DistortionModule : public FxModule
{
public:
    DistortionModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);

private:
    juce::dsp::Oversampling<float> oversampling { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };
};

// =============================================================================
// EQ MODULE (4-band parametric: Low, Mid, Mid-High, High)
// =============================================================================
class EQModule : public FxModule
{
public:
    EQModule(juce::AudioProcessorValueTreeState& state, const juce::String& id = "eq");
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& id = "eq");

private:
    using EQStage = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    juce::dsp::ProcessorChain<EQStage, EQStage, EQStage, EQStage> eq;
    float sampleRate = 44100.0f;
};

// =============================================================================
// TREMOLO / TRANS-GATE MODULE (with LFO and DAW sync)
// =============================================================================
class TremoloModule : public FxModule
{
public:
    TremoloModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);

private:
    float phase = 0.0f;
    float sampleRate = 44100.0f;
};

// =============================================================================
// RING MODULATOR MODULE
// =============================================================================
class RingModModule : public FxModule
{
public:
    RingModModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);

private:
    float phase = 0.0f;
    float sampleRate = 44100.0f;
};

// =============================================================================
// NOISE GENERATOR MODULE (for sweeps/risers)
// =============================================================================
class NoiseGenModule : public FxModule
{
public:
    NoiseGenModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);

private:
    juce::Random rng;
    juce::dsp::IIR::Filter<float> lpFilter, hpFilter;
    float sampleRate = 44100.0f;
};

// =============================================================================
// TONE GENERATOR MODULE (for risers/pitch sweeps)
// =============================================================================
class ToneGenModule : public FxModule
{
public:
    ToneGenModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;

    static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);

private:
    float phase = 0.0f;
    float sampleRate = 44100.0f;
};
