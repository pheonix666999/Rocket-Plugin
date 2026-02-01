#include "Modules.h"
#include "ModMatrix.h"

// =============================================================================
// FxModule - getMix with modulation support
// =============================================================================
float FxModule::getMix(ModMatrix& modMatrix) const
{
    if (auto* p = apvts.getRawParameterValue(moduleID + "_mix"))
        return modMatrix.getModulatedParamValue(moduleID + "_mix", p->load());
    return 1.0f;
}

// =============================================================================
// REVERB MODULE
// =============================================================================
ReverbModule::ReverbModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "reverb", ModuleKind::Effect)
{
}

void ReverbModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    reverb.setSampleRate(spec.sampleRate);
    reverb.reset();
}

void ReverbModule::reset()
{
    reverb.reset();
}

void ReverbModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled()) return;

    const float mix = getMix(modMatrix);
    if (mix < 0.001f) return;

    // Get parameters
    float decay = 0.5f, predelay = 0.0f, tone = 0.5f;
    int algorithm = 0;

    if (auto* p = apvts.getRawParameterValue("reverb_decay"))
        decay = modMatrix.getModulatedParamValue("reverb_decay", p->load());
    if (auto* p = apvts.getRawParameterValue("reverb_predelay"))
        predelay = modMatrix.getModulatedParamValue("reverb_predelay", p->load());
    if (auto* p = apvts.getRawParameterValue("reverb_tone"))
        tone = modMatrix.getModulatedParamValue("reverb_tone", p->load());
    if (auto* p = apvts.getRawParameterValue("reverb_algorithm"))
        algorithm = juce::roundToInt(p->load());

    juce::Reverb::Parameters params;
    params.roomSize = juce::jlimit(0.0f, 1.0f, decay);
    params.damping = juce::jlimit(0.0f, 1.0f, 1.0f - tone);
    params.wetLevel = mix;
    params.dryLevel = 1.0f - mix;
    params.width = algorithm == 0 ? 1.0f : 0.7f; // Hall vs Plate
    params.freezeMode = 0.0f;

    reverb.setParameters(params);

    // Process
    if (buffer.getNumChannels() >= 2)
        reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
    else
        reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
}

void ReverbModule::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add(std::make_unique<juce::AudioParameterBool>("reverb_enabled", "Reverb Enabled", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("reverb_mix", "Reverb Mix", 0.0f, 1.0f, 0.3f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("reverb_decay", "Reverb Decay", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("reverb_predelay", "Reverb Predelay", 0.0f, 200.0f, 20.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("reverb_tone", "Reverb Tone", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterChoice>("reverb_algorithm", "Reverb Algorithm", 
        juce::StringArray{"Hall", "Plate", "Room", "Chamber"}, 0));
}

// =============================================================================
// DELAY MODULE
// =============================================================================
DelayModule::DelayModule(juce::AudioProcessorValueTreeState& state, int delayIndex)
    : FxModule(state, "delay" + juce::String(delayIndex), ModuleKind::Effect), index(delayIndex)
{
}

void DelayModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float)spec.sampleRate;
    delayL.prepare(spec);
    delayR.prepare(spec);
    delayL.setMaximumDelayInSamples(juce::roundToInt(sampleRate * 2.0f));
    delayR.setMaximumDelayInSamples(juce::roundToInt(sampleRate * 2.0f));

    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 80.0f);
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 12000.0f);
    hpFilterL.coefficients = hpCoeffs;
    hpFilterR.coefficients = hpCoeffs;
    lpFilterL.coefficients = lpCoeffs;
    lpFilterR.coefficients = lpCoeffs;
}

void DelayModule::reset()
{
    delayL.reset();
    delayR.reset();
    hpFilterL.reset();
    hpFilterR.reset();
    lpFilterL.reset();
    lpFilterR.reset();
    fbL = fbR = 0.0f;
}

void DelayModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport)
{
    if (!isEnabled()) return;

    const float mix = getMix(modMatrix);
    if (mix < 0.001f) return;

    const juce::String prefix = moduleID + "_";

    float timeL = 0.25f, timeR = 0.25f, feedback = 0.4f;
    bool sync = false;
    int rhythm = 2;

    if (auto* p = apvts.getRawParameterValue(prefix + "time"))
        timeL = timeR = modMatrix.getModulatedParamValue(prefix + "time", p->load());
    if (auto* p = apvts.getRawParameterValue(prefix + "feedback"))
        feedback = modMatrix.getModulatedParamValue(prefix + "feedback", p->load());
    if (auto* p = apvts.getRawParameterValue(prefix + "sync"))
        sync = p->load() > 0.5f;
    if (auto* p = apvts.getRawParameterValue(prefix + "rhythm"))
        rhythm = juce::roundToInt(p->load());

    // Calculate delay time in samples
    float delaySamplesL, delaySamplesR;
    if (sync && transport.bpm > 0.0)
    {
        static const float rhythmValues[] = { 0.0625f, 0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
        float beats = rhythmValues[juce::jlimit(0, 7, rhythm)];
        float secondsPerBeat = 60.0f / (float)transport.bpm;
        delaySamplesL = delaySamplesR = beats * secondsPerBeat * sampleRate;
    }
    else
    {
        delaySamplesL = delaySamplesR = timeL * sampleRate;
    }

    delaySamplesL = juce::jlimit(1.0f, sampleRate * 2.0f - 1.0f, delaySamplesL);
    delaySamplesR = juce::jlimit(1.0f, sampleRate * 2.0f - 1.0f, delaySamplesR);

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    delayL.setDelay(delaySamplesL);
    delayR.setDelay(delaySamplesR);

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = buffer.getSample(0, i);
        float inR = numChannels > 1 ? buffer.getSample(1, i) : inL;

        float delayedL = delayL.popSample(0);
        float delayedR = delayR.popSample(0);

        // Filter feedback
        float filteredFbL = lpFilterL.processSample(hpFilterL.processSample(delayedL * feedback));
        float filteredFbR = lpFilterR.processSample(hpFilterR.processSample(delayedR * feedback));

        delayL.pushSample(0, inL + filteredFbL);
        delayR.pushSample(0, inR + filteredFbR);

        buffer.setSample(0, i, inL * (1.0f - mix) + delayedL * mix);
        if (numChannels > 1)
            buffer.setSample(1, i, inR * (1.0f - mix) + delayedR * mix);
    }
}

void DelayModule::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, int delayIndex)
{
    juce::String prefix = "delay" + juce::String(delayIndex) + "_";
    juce::String name = "Delay " + juce::String(delayIndex);

    layout.add(std::make_unique<juce::AudioParameterBool>(prefix + "enabled", name + " Enabled", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "mix", name + " Mix", 0.0f, 1.0f, 0.3f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "time", name + " Time", 0.01f, 2.0f, 0.25f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "feedback", name + " Feedback", 0.0f, 0.95f, 0.4f));
    layout.add(std::make_unique<juce::AudioParameterBool>(prefix + "sync", name + " Sync", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(prefix + "rhythm", name + " Rhythm",
        juce::StringArray{"1/16", "1/8", "1/4", "1/2", "3/4", "1", "1.5", "2"}, 2));
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "hp", name + " HP", 20.0f, 2000.0f, 80.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "lp", name + " LP", 1000.0f, 20000.0f, 12000.0f));
}

// =============================================================================
// FILTER MODULE
// =============================================================================
FilterModule::FilterModule(juce::AudioProcessorValueTreeState& state, Type t, const juce::String& id)
    : FxModule(state, id, ModuleKind::Effect), type(t)
{
}

void FilterModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float)spec.sampleRate;
    for (auto& f : filters)
    {
        f.prepare(spec);
        f.reset();
    }
}

void FilterModule::reset()
{
    for (auto& f : filters)
        f.reset();
}

void FilterModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled()) return;

    const float mix = getMix(modMatrix);
    if (mix < 0.001f) return;

    float cutoff = 1000.0f;
    int slope = 2; // dB per octave index: 0=6dB, 1=12dB, 2=24dB, 3=96dB

    if (auto* p = apvts.getRawParameterValue(moduleID + "_cutoff"))
        cutoff = modMatrix.getModulatedParamValue(moduleID + "_cutoff", p->load());
    if (auto* p = apvts.getRawParameterValue(moduleID + "_slope"))
        slope = juce::roundToInt(p->load());

    cutoff = juce::jlimit(20.0f, 20000.0f, cutoff);

    // Calculate number of filter stages based on slope
    int stages = 1;
    switch (slope)
    {
        case 0: stages = 1; break;  // 6dB
        case 1: stages = 2; break;  // 12dB
        case 2: stages = 4; break;  // 24dB
        case 3: stages = 16; break; // 96dB
        default: stages = 4; break;
    }

    // Update filter coefficients
    juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> coeffs;
    if (type == Type::LowPass)
        coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoff);
    else
        coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, cutoff);

    for (int i = 0; i < stages; ++i)
        *filters[(size_t)i].state = *coeffs;

    // Store dry signal for mix
    juce::AudioBuffer<float> dryBuffer;
    if (mix < 0.999f)
    {
        dryBuffer.makeCopyOf(buffer);
    }

    // Process through filter stages
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    for (int i = 0; i < stages; ++i)
        filters[(size_t)i].process(context);

    // Apply mix
    if (mix < 0.999f)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* wet = buffer.getWritePointer(ch);
            const float* dry = dryBuffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                wet[i] = dry[i] * (1.0f - mix) + wet[i] * mix;
        }
    }
}

void FilterModule::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& id, Type type)
{
    juce::String name = type == Type::LowPass ? "LP Filter" : "HP Filter";
    layout.add(std::make_unique<juce::AudioParameterBool>(id + "_enabled", name + " Enabled", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id + "_mix", name + " Mix", 0.0f, 1.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(id + "_cutoff", name + " Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f), 
        type == Type::LowPass ? 20000.0f : 20.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(id + "_slope", name + " Slope",
        juce::StringArray{"6 dB", "12 dB", "24 dB", "96 dB"}, 2));
}

// =============================================================================
// FLANGER MODULE
// =============================================================================
FlangerModule::FlangerModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "flanger", ModuleKind::Effect)
{
}

void FlangerModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float)spec.sampleRate;
    delayL.prepare(spec);
    delayR.prepare(spec);
    delayL.setMaximumDelayInSamples(2048);
    delayR.setMaximumDelayInSamples(2048);
}

void FlangerModule::reset()
{
    delayL.reset();
    delayR.reset();
    phase = 0.0f;
}

void FlangerModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled()) return;

    const float mix = getMix(modMatrix);
    if (mix < 0.001f) return;

    float rate = 0.5f, depth = 0.5f, feedback = 0.5f;

    if (auto* p = apvts.getRawParameterValue("flanger_rate"))
        rate = modMatrix.getModulatedParamValue("flanger_rate", p->load());
    if (auto* p = apvts.getRawParameterValue("flanger_depth"))
        depth = modMatrix.getModulatedParamValue("flanger_depth", p->load());
    if (auto* p = apvts.getRawParameterValue("flanger_feedback"))
        feedback = modMatrix.getModulatedParamValue("flanger_feedback", p->load());

    const float phaseInc = rate / sampleRate;
    const float baseDelay = 1.0f; // ms
    const float modDepth = 7.0f * depth; // ms

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float lfo = 0.5f + 0.5f * std::sin(juce::MathConstants<float>::twoPi * phase);
        float delaySamples = (baseDelay + lfo * modDepth) * sampleRate / 1000.0f;

        delayL.setDelay(delaySamples);
        delayR.setDelay(delaySamples);

        float inL = buffer.getSample(0, i);
        float inR = numChannels > 1 ? buffer.getSample(1, i) : inL;

        float delayedL = delayL.popSample(0);
        float delayedR = delayR.popSample(0);

        delayL.pushSample(0, inL + delayedL * feedback);
        delayR.pushSample(0, inR + delayedR * feedback);

        buffer.setSample(0, i, inL * (1.0f - mix) + delayedL * mix);
        if (numChannels > 1)
            buffer.setSample(1, i, inR * (1.0f - mix) + delayedR * mix);

        phase += phaseInc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
}

void FlangerModule::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add(std::make_unique<juce::AudioParameterBool>("flanger_enabled", "Flanger Enabled", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("flanger_mix", "Flanger Mix", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("flanger_rate", "Flanger Rate", 0.1f, 10.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("flanger_depth", "Flanger Depth", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("flanger_feedback", "Flanger Feedback", 0.0f, 0.95f, 0.5f));
}

// =============================================================================
// PHASER MODULE
// =============================================================================
PhaserModule::PhaserModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "phaser", ModuleKind::Effect)
{
}

void PhaserModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    phaser.prepare(spec);
}

void PhaserModule::reset()
{
    phaser.reset();
}

void PhaserModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled()) return;

    const float mix = getMix(modMatrix);
    if (mix < 0.001f) return;

    float rate = 0.5f, depth = 0.5f, feedback = 0.5f;

    if (auto* p = apvts.getRawParameterValue("phaser_rate"))
        rate = modMatrix.getModulatedParamValue("phaser_rate", p->load());
    if (auto* p = apvts.getRawParameterValue("phaser_depth"))
        depth = modMatrix.getModulatedParamValue("phaser_depth", p->load());
    if (auto* p = apvts.getRawParameterValue("phaser_feedback"))
        feedback = modMatrix.getModulatedParamValue("phaser_feedback", p->load());

    phaser.setRate(rate);
    phaser.setDepth(depth);
    phaser.setFeedback(feedback);
    phaser.setMix(mix);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    phaser.process(context);
}

void PhaserModule::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add(std::make_unique<juce::AudioParameterBool>("phaser_enabled", "Phaser Enabled", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("phaser_mix", "Phaser Mix", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("phaser_rate", "Phaser Rate", 0.1f, 10.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("phaser_depth", "Phaser Depth", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("phaser_feedback", "Phaser Feedback", -0.95f, 0.95f, 0.5f));
}

// =============================================================================
// BITCRUSHER MODULE
// =============================================================================
BitcrusherModule::BitcrusherModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "bitcrush", ModuleKind::Effect)
{
}

void BitcrusherModule::prepare(const juce::dsp::ProcessSpec&)
{
    reset();
}

void BitcrusherModule::reset()
{
    downsampleCounter = 0;
    heldSampleL = heldSampleR = 0.0f;
}

void BitcrusherModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled()) return;

    const float mix = getMix(modMatrix);
    if (mix < 0.001f) return;

    float bits = 16.0f, downsample = 1.0f;

    if (auto* p = apvts.getRawParameterValue("bitcrush_bits"))
        bits = modMatrix.getModulatedParamValue("bitcrush_bits", p->load());
    if (auto* p = apvts.getRawParameterValue("bitcrush_downsample"))
        downsample = modMatrix.getModulatedParamValue("bitcrush_downsample", p->load());

    const int downsampleFactor = juce::jmax(1, juce::roundToInt(downsample));
    const float bitDepth = juce::jlimit(1.0f, 16.0f, bits);
    const float quantLevels = std::pow(2.0f, bitDepth);

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = buffer.getSample(0, i);
        float inR = numChannels > 1 ? buffer.getSample(1, i) : inL;

        // Downsample
        if (downsampleCounter == 0)
        {
            // Bit crush
            heldSampleL = std::round(inL * quantLevels) / quantLevels;
            heldSampleR = std::round(inR * quantLevels) / quantLevels;
        }

        downsampleCounter = (downsampleCounter + 1) % downsampleFactor;

        buffer.setSample(0, i, inL * (1.0f - mix) + heldSampleL * mix);
        if (numChannels > 1)
            buffer.setSample(1, i, inR * (1.0f - mix) + heldSampleR * mix);
    }
}

void BitcrusherModule::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add(std::make_unique<juce::AudioParameterBool>("bitcrush_enabled", "Bitcrush Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("bitcrush_mix", "Bitcrush Mix", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("bitcrush_bits", "Bitcrush Bits", 1.0f, 16.0f, 16.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("bitcrush_downsample", "Bitcrush Downsample", 1.0f, 32.0f, 1.0f));
}

// =============================================================================
// DISTORTION MODULE
// =============================================================================
DistortionModule::DistortionModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "distortion", ModuleKind::Effect)
{
}

void DistortionModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    oversampling.initProcessing(spec.maximumBlockSize);
}

void DistortionModule::reset()
{
    oversampling.reset();
}

void DistortionModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled()) return;

    const float mix = getMix(modMatrix);
    if (mix < 0.001f) return;

    float drive = 0.5f;
    int algorithm = 0;

    if (auto* p = apvts.getRawParameterValue("distortion_drive"))
        drive = modMatrix.getModulatedParamValue("distortion_drive", p->load());
    if (auto* p = apvts.getRawParameterValue("distortion_algorithm"))
        algorithm = juce::roundToInt(p->load());

    // Store dry signal
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    juce::dsp::AudioBlock<float> block(buffer);
    auto osBlock = oversampling.processSamplesUp(block);

    // Apply distortion algorithm
    const float gain = 1.0f + drive * 20.0f;

    for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
    {
        float* samples = osBlock.getChannelPointer(ch);
        for (size_t i = 0; i < osBlock.getNumSamples(); ++i)
        {
            float x = samples[i] * gain;

            switch (algorithm)
            {
                case 0: // Soft clip (tanh)
                    samples[i] = std::tanh(x);
                    break;
                case 1: // Hard clip
                    samples[i] = juce::jlimit(-1.0f, 1.0f, x);
                    break;
                case 2: // Tube-style
                    samples[i] = x / (1.0f + std::abs(x));
                    break;
                case 3: // Fuzz
                    samples[i] = std::tanh(std::sin(x * juce::MathConstants<float>::pi * 0.5f) * 2.0f);
                    break;
                default:
                    samples[i] = std::tanh(x);
            }
        }
    }

    oversampling.processSamplesDown(block);

    // Apply mix
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* wet = buffer.getWritePointer(ch);
        const float* dry = dryBuffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            wet[i] = dry[i] * (1.0f - mix) + wet[i] * mix;
    }
}

void DistortionModule::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add(std::make_unique<juce::AudioParameterBool>("distortion_enabled", "Distortion Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("distortion_mix", "Distortion Mix", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("distortion_drive", "Distortion Drive", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterChoice>("distortion_algorithm", "Distortion Type",
        juce::StringArray{"Soft", "Hard", "Tube", "Fuzz"}, 0));
}

// =============================================================================
// EQ MODULE
// =============================================================================
EQModule::EQModule(juce::AudioProcessorValueTreeState& state, const juce::String& id)
    : FxModule(state, id, ModuleKind::Effect)
{
}

void EQModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float)spec.sampleRate;
    eq.prepare(spec);
}

void EQModule::reset()
{
    eq.reset();
}

void EQModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled()) return;

    const juce::String prefix = moduleID + "_";

    // Low band (shelf)
    float lowFreq = 100.0f, lowGain = 0.0f;
    if (auto* p = apvts.getRawParameterValue(prefix + "low_freq"))
        lowFreq = modMatrix.getModulatedParamValue(prefix + "low_freq", p->load());
    if (auto* p = apvts.getRawParameterValue(prefix + "low_gain"))
        lowGain = modMatrix.getModulatedParamValue(prefix + "low_gain", p->load());

    // Mid band
    float midFreq = 500.0f, midGain = 0.0f, midQ = 1.0f;
    if (auto* p = apvts.getRawParameterValue(prefix + "mid_freq"))
        midFreq = modMatrix.getModulatedParamValue(prefix + "mid_freq", p->load());
    if (auto* p = apvts.getRawParameterValue(prefix + "mid_gain"))
        midGain = modMatrix.getModulatedParamValue(prefix + "mid_gain", p->load());
    if (auto* p = apvts.getRawParameterValue(prefix + "mid_q"))
        midQ = modMatrix.getModulatedParamValue(prefix + "mid_q", p->load());

    // Mid-High band
    float midHiFreq = 2000.0f, midHiGain = 0.0f, midHiQ = 1.0f;
    if (auto* p = apvts.getRawParameterValue(prefix + "midhi_freq"))
        midHiFreq = modMatrix.getModulatedParamValue(prefix + "midhi_freq", p->load());
    if (auto* p = apvts.getRawParameterValue(prefix + "midhi_gain"))
        midHiGain = modMatrix.getModulatedParamValue(prefix + "midhi_gain", p->load());
    if (auto* p = apvts.getRawParameterValue(prefix + "midhi_q"))
        midHiQ = modMatrix.getModulatedParamValue(prefix + "midhi_q", p->load());

    // High band (shelf)
    float highFreq = 8000.0f, highGain = 0.0f;
    if (auto* p = apvts.getRawParameterValue(prefix + "high_freq"))
        highFreq = modMatrix.getModulatedParamValue(prefix + "high_freq", p->load());
    if (auto* p = apvts.getRawParameterValue(prefix + "high_gain"))
        highGain = modMatrix.getModulatedParamValue(prefix + "high_gain", p->load());

    // Update coefficients
    *eq.get<0>().state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, lowFreq, 0.707f, juce::Decibels::decibelsToGain(lowGain));
    *eq.get<1>().state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, midFreq, midQ, juce::Decibels::decibelsToGain(midGain));
    *eq.get<2>().state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, midHiFreq, midHiQ, juce::Decibels::decibelsToGain(midHiGain));
    *eq.get<3>().state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, highFreq, 0.707f, juce::Decibels::decibelsToGain(highGain));

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    eq.process(context);
}

void EQModule::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& id)
{
    juce::String prefix = id + "_";
    juce::String name = id == "eq" ? "EQ" : id;

    layout.add(std::make_unique<juce::AudioParameterBool>(id + "_enabled", name + " Enabled", true));

    // Low band
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "low_freq", name + " Low Freq",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.5f), 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "low_gain", name + " Low Gain", -18.0f, 18.0f, 0.0f));

    // Mid band
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "mid_freq", name + " Mid Freq",
        juce::NormalisableRange<float>(100.0f, 4000.0f, 1.0f, 0.5f), 500.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "mid_gain", name + " Mid Gain", -18.0f, 18.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "mid_q", name + " Mid Q", 0.1f, 10.0f, 1.0f));

    // Mid-High band
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "midhi_freq", name + " Mid-Hi Freq",
        juce::NormalisableRange<float>(500.0f, 10000.0f, 1.0f, 0.5f), 2000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "midhi_gain", name + " Mid-Hi Gain", -18.0f, 18.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "midhi_q", name + " Mid-Hi Q", 0.1f, 10.0f, 1.0f));

    // High band
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "high_freq", name + " High Freq",
        juce::NormalisableRange<float>(2000.0f, 20000.0f, 1.0f, 0.5f), 8000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(prefix + "high_gain", name + " High Gain", -18.0f, 18.0f, 0.0f));
}

// =============================================================================
// TREMOLO MODULE
// =============================================================================
TremoloModule::TremoloModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "tremolo", ModuleKind::Effect)
{
}

void TremoloModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float)spec.sampleRate;
}

void TremoloModule::reset()
{
    phase = 0.0f;
}

void TremoloModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport)
{
    if (!isEnabled()) return;

    const float mix = getMix(modMatrix);
    if (mix < 0.001f) return;

    float rate = 4.0f, depth = 0.5f;
    bool sync = false;
    int rhythm = 2;
    int waveform = 0;

    if (auto* p = apvts.getRawParameterValue("tremolo_rate"))
        rate = modMatrix.getModulatedParamValue("tremolo_rate", p->load());
    if (auto* p = apvts.getRawParameterValue("tremolo_depth"))
        depth = modMatrix.getModulatedParamValue("tremolo_depth", p->load());
    if (auto* p = apvts.getRawParameterValue("tremolo_sync"))
        sync = p->load() > 0.5f;
    if (auto* p = apvts.getRawParameterValue("tremolo_rhythm"))
        rhythm = juce::roundToInt(p->load());
    if (auto* p = apvts.getRawParameterValue("tremolo_waveform"))
        waveform = juce::roundToInt(p->load());

    // Calculate frequency
    float freq = rate;
    if (sync && transport.bpm > 0.0)
    {
        static const float rhythmValues[] = { 0.0625f, 0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
        float beats = rhythmValues[juce::jlimit(0, 7, rhythm)];
        float secondsPerBeat = 60.0f / (float)transport.bpm;
        freq = 1.0f / (beats * secondsPerBeat);
    }

    const float phaseInc = freq / sampleRate;
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float lfo = 0.0f;
        switch (waveform)
        {
            case 0: // Sine
                lfo = 0.5f + 0.5f * std::sin(juce::MathConstants<float>::twoPi * phase);
                break;
            case 1: // Triangle
                lfo = 1.0f - std::abs(phase * 2.0f - 1.0f);
                break;
            case 2: // Square (gate)
                lfo = phase < 0.5f ? 1.0f : 0.0f;
                break;
            case 3: // Saw up
                lfo = phase;
                break;
            case 4: // Saw down
                lfo = 1.0f - phase;
                break;
            default:
                lfo = 0.5f + 0.5f * std::sin(juce::MathConstants<float>::twoPi * phase);
        }

        float modulation = 1.0f - depth * (1.0f - lfo);
        modulation = juce::jlimit(0.0f, 1.0f, modulation);
        float gain = (1.0f - mix) + mix * modulation;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample(ch, i, buffer.getSample(ch, i) * gain);

        phase += phaseInc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
}

void TremoloModule::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add(std::make_unique<juce::AudioParameterBool>("tremolo_enabled", "Tremolo Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tremolo_mix", "Tremolo Mix", 0.0f, 1.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tremolo_rate", "Tremolo Rate", 0.1f, 20.0f, 4.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tremolo_depth", "Tremolo Depth", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterBool>("tremolo_sync", "Tremolo Sync", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>("tremolo_rhythm", "Tremolo Rhythm",
        juce::StringArray{"1/16", "1/8", "1/4", "1/2", "3/4", "1", "1.5", "2"}, 2));
    layout.add(std::make_unique<juce::AudioParameterChoice>("tremolo_waveform", "Tremolo Waveform",
        juce::StringArray{"Sine", "Triangle", "Square", "Saw Up", "Saw Down"}, 0));
}

// =============================================================================
// RING MODULATOR MODULE
// =============================================================================
RingModModule::RingModModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "ringmod", ModuleKind::Effect)
{
}

void RingModModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float)spec.sampleRate;
}

void RingModModule::reset()
{
    phase = 0.0f;
}

void RingModModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled()) return;

    const float mix = getMix(modMatrix);
    if (mix < 0.001f) return;

    float freq = 440.0f;

    if (auto* p = apvts.getRawParameterValue("ringmod_freq"))
        freq = modMatrix.getModulatedParamValue("ringmod_freq", p->load());

    const float phaseInc = freq / sampleRate;
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float carrier = std::sin(juce::MathConstants<float>::twoPi * phase);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float in = buffer.getSample(ch, i);
            float modulated = in * carrier;
            buffer.setSample(ch, i, in * (1.0f - mix) + modulated * mix);
        }

        phase += phaseInc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
}

void RingModModule::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add(std::make_unique<juce::AudioParameterBool>("ringmod_enabled", "Ring Mod Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("ringmod_mix", "Ring Mod Mix", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("ringmod_freq", "Ring Mod Freq",
        juce::NormalisableRange<float>(20.0f, 5000.0f, 0.1f, 0.3f), 440.0f));
}

// =============================================================================
// NOISE GENERATOR MODULE
// =============================================================================
NoiseGenModule::NoiseGenModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "noisegen", ModuleKind::Generator)
{
}

void NoiseGenModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float)spec.sampleRate;
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 10000.0f);
    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 200.0f);
    lpFilter.coefficients = lpCoeffs;
    hpFilter.coefficients = hpCoeffs;
}

void NoiseGenModule::reset()
{
    lpFilter.reset();
    hpFilter.reset();
}

void NoiseGenModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled()) return;

    float gain = 0.0f, lpFreq = 10000.0f, hpFreq = 200.0f;

    if (auto* p = apvts.getRawParameterValue("noisegen_gain"))
        gain = modMatrix.getModulatedParamValue("noisegen_gain", p->load());
    if (auto* p = apvts.getRawParameterValue("noisegen_lp"))
        lpFreq = modMatrix.getModulatedParamValue("noisegen_lp", p->load());
    if (auto* p = apvts.getRawParameterValue("noisegen_hp"))
        hpFreq = modMatrix.getModulatedParamValue("noisegen_hp", p->load());

    if (gain < 0.001f) return;

    // Update filters
    lpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, juce::jlimit(200.0f, 20000.0f, lpFreq));
    hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, juce::jlimit(20.0f, 5000.0f, hpFreq));

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float noise = rng.nextFloat() * 2.0f - 1.0f;
        noise = lpFilter.processSample(hpFilter.processSample(noise));
        noise *= gain;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.addSample(ch, i, noise);
    }
}

void NoiseGenModule::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add(std::make_unique<juce::AudioParameterBool>("noisegen_enabled", "Noise Gen Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("noisegen_gain", "Noise Gain", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("noisegen_lp", "Noise LP",
        juce::NormalisableRange<float>(200.0f, 20000.0f, 1.0f, 0.3f), 10000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("noisegen_hp", "Noise HP",
        juce::NormalisableRange<float>(20.0f, 5000.0f, 1.0f, 0.3f), 200.0f));
}

// =============================================================================
// TONE GENERATOR MODULE
// =============================================================================
ToneGenModule::ToneGenModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "tonegen", ModuleKind::Generator)
{
}

void ToneGenModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float)spec.sampleRate;
}

void ToneGenModule::reset()
{
    phase = 0.0f;
}

void ToneGenModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled()) return;

    float gain = 0.0f, freq = 440.0f;
    int waveform = 0;

    if (auto* p = apvts.getRawParameterValue("tonegen_gain"))
        gain = modMatrix.getModulatedParamValue("tonegen_gain", p->load());
    if (auto* p = apvts.getRawParameterValue("tonegen_freq"))
        freq = modMatrix.getModulatedParamValue("tonegen_freq", p->load());
    if (auto* p = apvts.getRawParameterValue("tonegen_waveform"))
        waveform = juce::roundToInt(p->load());

    if (gain < 0.001f) return;

    const float phaseInc = freq / sampleRate;
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float sample = 0.0f;
        switch (waveform)
        {
            case 0: // Sine
                sample = std::sin(juce::MathConstants<float>::twoPi * phase);
                break;
            case 1: // Triangle
                sample = 2.0f * std::abs(phase * 2.0f - 1.0f) - 1.0f;
                break;
            case 2: // Saw
                sample = phase * 2.0f - 1.0f;
                break;
            case 3: // Square
                sample = phase < 0.5f ? 1.0f : -1.0f;
                break;
            default:
                sample = std::sin(juce::MathConstants<float>::twoPi * phase);
        }

        sample *= gain;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.addSample(ch, i, sample);

        phase += phaseInc;
        if (phase >= 1.0f) phase -= 1.0f;
    }
}

void ToneGenModule::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    layout.add(std::make_unique<juce::AudioParameterBool>("tonegen_enabled", "Tone Gen Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tonegen_gain", "Tone Gain", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tonegen_freq", "Tone Freq",
        juce::NormalisableRange<float>(20.0f, 5000.0f, 0.1f, 0.3f), 440.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>("tonegen_waveform", "Tone Waveform",
        juce::StringArray{"Sine", "Triangle", "Saw", "Square"}, 0));
}
