#pragma once

#include "FxModule.h"
#include <array>

class ReverbModule : public FxModule
{
public:
    ReverbModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;
private:
    juce::dsp::Reverb reverb;
    juce::dsp::Convolution convolution;
    double sampleRate = 44100.0;
};

class DelayModule : public FxModule
{
public:
    DelayModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;
private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 192000 };
    float sampleRate = 44100.0f;
    float fbLpL = 0.0f;
    float fbLpR = 0.0f;
};

class FilterModule : public FxModule
{
public:
    enum class Type { LowPass, HighPass };
    FilterModule(juce::AudioProcessorValueTreeState& state, Type t, const juce::String& id);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;
private:
    Type type;
    using FilterStage = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    std::array<FilterStage, 16> filters;
    float sampleRate = 44100.0f;
};

class FlangerModule : public FxModule
{
public:
    FlangerModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;
private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 2048 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 2048 };
    float sampleRate = 44100.0f;
    float phase = 0.0f;
};

class PhaserModule : public FxModule
{
public:
    PhaserModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;
private:
    juce::dsp::Phaser<float> phaser;
};

class BitcrusherModule : public FxModule
{
public:
    BitcrusherModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;
private:
    int downsampleCounter = 0;
    float heldSampleL = 0.0f;
    float heldSampleR = 0.0f;
};

class DistortionModule : public FxModule
{
public:
    DistortionModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;
};

class EQModule : public FxModule
{
public:
    EQModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;
private:
    using EQStage = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    juce::dsp::ProcessorChain<EQStage, EQStage, EQStage, EQStage> eq;
    float sampleRate = 44100.0f;
};

class TremoloModule : public FxModule
{
public:
    TremoloModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;
private:
    float phase = 0.0f;
    float sampleRate = 44100.0f;
};

class RingModModule : public FxModule
{
public:
    RingModModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;
private:
    float phase = 0.0f;
    float sampleRate = 44100.0f;
};

class NoiseGenModule : public FxModule
{
public:
    NoiseGenModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;
private:
    juce::Random rng;
};

class ToneGenModule : public FxModule
{
public:
    ToneGenModule(juce::AudioProcessorValueTreeState& state);
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport) override;
private:
    float phase = 0.0f;
    float sampleRate = 44100.0f;
};
