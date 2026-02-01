#include "Modules.h"

namespace
{
    inline void applyMix(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& dry, float mix)
    {
        if (mix >= 0.999f)
            return;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            auto* d = dry.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                wet[i] = d[i] + mix * (wet[i] - d[i]);
        }
    }

    inline float dbToLin(float db) { return std::pow(10.0f, db / 20.0f); }
}

ReverbModule::ReverbModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "reverb", ModuleKind::Effect)
{
}

void ReverbModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    reverb.reset();
    convolution.prepare(spec);

    // Simple generated "plate-ish" impulse response. This is intentionally small to keep CPU reasonable.
    juce::AudioBuffer<float> ir(1, 4096);
    juce::Random rng(12345);
    const float decaySeconds = 1.1f;
    const float decayCoeff = std::exp(-1.0f / ((float) sampleRate * decaySeconds));
    float env = 1.0f;
    for (int i = 0; i < ir.getNumSamples(); ++i)
    {
        const float noise = (rng.nextFloat() * 2.0f - 1.0f);
        ir.setSample(0, i, noise * env);
        env *= decayCoeff;
    }

    convolution.loadImpulseResponse(std::move(ir), sampleRate,
                                    juce::dsp::Convolution::Stereo::no,
                                    juce::dsp::Convolution::Trim::yes,
                                    juce::dsp::Convolution::Normalise::yes);
}

void ReverbModule::reset()
{
    reverb.reset();
    convolution.reset();
}

void ReverbModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled())
        return;

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    const int algo = (int) std::round(modMatrix.getModulatedParamValue("reverb_algo", apvts.getRawParameterValue("reverb_algo")->load()));

    juce::dsp::Reverb::Parameters p;
    p.roomSize = modMatrix.getModulatedParamValue("reverb_size", apvts.getRawParameterValue("reverb_size")->load());
    p.damping = modMatrix.getModulatedParamValue("reverb_damping", apvts.getRawParameterValue("reverb_damping")->load());
    p.width = modMatrix.getModulatedParamValue("reverb_width", apvts.getRawParameterValue("reverb_width")->load());
    p.freezeMode = modMatrix.getModulatedParamValue("reverb_freeze", apvts.getRawParameterValue("reverb_freeze")->load());
    reverb.setParameters(p);

    auto block = juce::dsp::AudioBlock<float>(buffer);

    if (algo <= 0)
    {
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        reverb.process(ctx);
    }
    else
    {
        // Plate mode: light convolution + post reverb for density.
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        convolution.process(ctx);
        reverb.process(ctx);
    }

    applyMix(buffer, dry, getMix(modMatrix));
}

DelayModule::DelayModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "delay", ModuleKind::Effect)
{
}

void DelayModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
    delayL.reset();
    delayR.reset();
    delayL.setDelay(0.0f);
    delayR.setDelay(0.0f);
    fbLpL = fbLpR = 0.0f;
}

void DelayModule::reset()
{
    delayL.reset();
    delayR.reset();
    fbLpL = fbLpR = 0.0f;
}

static float divisionToBeats(int divIndex)
{
    // 0..4 => 1/1, 1/2, 1/4, 1/8, 1/16
    switch (divIndex)
    {
        case 0: return 4.0f;
        case 1: return 2.0f;
        case 2: return 1.0f;
        case 3: return 0.5f;
        case 4: return 0.25f;
        default: return 1.0f;
    }
}

void DelayModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport)
{
    if (!isEnabled())
        return;

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    const float time1Ms = modMatrix.getModulatedParamValue("delay_time1", apvts.getRawParameterValue("delay_time1")->load());
    const float time2Ms = modMatrix.getModulatedParamValue("delay_time2", apvts.getRawParameterValue("delay_time2")->load());
    const float fb1 = modMatrix.getModulatedParamValue("delay_fb1", apvts.getRawParameterValue("delay_fb1")->load());
    const float fb2 = modMatrix.getModulatedParamValue("delay_fb2", apvts.getRawParameterValue("delay_fb2")->load());

    const bool sync1 = apvts.getRawParameterValue("delay_sync1")->load() > 0.5f;
    const bool sync2 = apvts.getRawParameterValue("delay_sync2")->load() > 0.5f;
    const int div1 = (int) std::round(apvts.getRawParameterValue("delay_div1")->load());
    const int div2 = (int) std::round(apvts.getRawParameterValue("delay_div2")->load());

    const float bpm = (float) (transport.bpm > 0.0 ? transport.bpm : 120.0);
    const float secondsPerBeat = 60.0f / bpm;

    const float delaySamples1 = sync1
        ? juce::jlimit(1.0f, sampleRate * 2.0f, divisionToBeats(div1) * secondsPerBeat * sampleRate)
        : juce::jlimit(1.0f, sampleRate * 2.0f, time1Ms * 0.001f * sampleRate);

    const float delaySamples2 = sync2
        ? juce::jlimit(1.0f, sampleRate * 2.0f, divisionToBeats(div2) * secondsPerBeat * sampleRate)
        : juce::jlimit(1.0f, sampleRate * 2.0f, time2Ms * 0.001f * sampleRate);

    delayL.setDelay(delaySamples1);
    delayR.setDelay(delaySamples2);

    const int mode1 = (int) std::round(apvts.getRawParameterValue("delay_mode1")->load());
    const int mode2 = (int) std::round(apvts.getRawParameterValue("delay_mode2")->load());

    const float tapeTone = modMatrix.getModulatedParamValue("delay_tape_tone", apvts.getRawParameterValue("delay_tape_tone")->load());
    const float lpCoeff = juce::jlimit(0.0f, 1.0f, 0.02f + (1.0f - tapeTone) * 0.20f);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float inL = buffer.getSample(0, i);
        float inR = buffer.getNumChannels() > 1 ? buffer.getSample(1, i) : inL;

        float dl = delayL.popSample(0);
        float dr = delayR.popSample(0);

        float fbInL = dl * fb1;
        float fbInR = dr * fb2;

        // Ping-pong mode crossfeeds.
        if (mode1 == 1) fbInL = dr * fb1;
        if (mode2 == 1) fbInR = dl * fb2;

        // Tape mode: gentle lowpass + soft saturation in feedback.
        if (mode1 == 2)
        {
            fbLpL = fbLpL + lpCoeff * (fbInL - fbLpL);
            fbInL = std::tanh(fbLpL * 1.7f);
        }
        if (mode2 == 2)
        {
            fbLpR = fbLpR + lpCoeff * (fbInR - fbLpR);
            fbInR = std::tanh(fbLpR * 1.7f);
        }

        delayL.pushSample(0, inL + fbInL);
        delayR.pushSample(0, inR + fbInR);

        buffer.setSample(0, i, inL + dl);
        if (buffer.getNumChannels() > 1)
            buffer.setSample(1, i, inR + dr);
    }

    applyMix(buffer, dry, getMix(modMatrix));
}

FilterModule::FilterModule(juce::AudioProcessorValueTreeState& state, Type t, const juce::String& id)
    : FxModule(state, id, ModuleKind::Effect), type(t)
{
}

void FilterModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
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
    if (!isEnabled())
        return;

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    const float cutoff = modMatrix.getModulatedParamValue(moduleID + "_cutoff", apvts.getRawParameterValue(moduleID + "_cutoff")->load());
    const float slope = modMatrix.getModulatedParamValue(moduleID + "_slope", apvts.getRawParameterValue(moduleID + "_slope")->load());

    const int slopeIndex = (int) juce::jlimit(0.0f, 3.0f, std::round(slope));
    const int stages = (slopeIndex == 0 ? 1 : slopeIndex == 1 ? 2 : slopeIndex == 2 ? 4 : 16);

    auto coef = (type == Type::LowPass)
        ? juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoff)
        : juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, cutoff);

    for (int i = 0; i < (int) filters.size(); ++i)
        *filters[(size_t) i].state = *coef;

    auto block = juce::dsp::AudioBlock<float>(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);

    for (int i = 0; i < stages; ++i)
        filters[(size_t) i].process(ctx);

    applyMix(buffer, dry, getMix(modMatrix));
}

FlangerModule::FlangerModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "flanger", ModuleKind::Effect)
{
}

void FlangerModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
    delayL.reset();
    delayR.reset();
    phase = 0.0f;
}

void FlangerModule::reset()
{
    delayL.reset();
    delayR.reset();
    phase = 0.0f;
}

void FlangerModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled())
        return;

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    const float rate = modMatrix.getModulatedParamValue("flanger_rate", apvts.getRawParameterValue("flanger_rate")->load());
    const float depth = modMatrix.getModulatedParamValue("flanger_depth", apvts.getRawParameterValue("flanger_depth")->load());
    const float feedback = modMatrix.getModulatedParamValue("flanger_feedback", apvts.getRawParameterValue("flanger_feedback")->load());

    const float maxDelayMs = 10.0f;
    const float baseDelayMs = 1.0f;
    const float phaseInc = (rate / sampleRate) * juce::MathConstants<float>::twoPi;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float lfo = (std::sin(phase) * 0.5f + 0.5f);
        float currentDelay = (baseDelayMs + depth * maxDelayMs * lfo) * 0.001f * sampleRate;

        delayL.setDelay(currentDelay);
        delayR.setDelay(currentDelay);

        float inL = buffer.getSample(0, i);
        float inR = buffer.getNumChannels() > 1 ? buffer.getSample(1, i) : inL;

        float dl = delayL.popSample(0);
        float dr = delayR.popSample(0);

        delayL.pushSample(0, inL + dl * feedback);
        delayR.pushSample(0, inR + dr * feedback);

        buffer.setSample(0, i, inL + dl);
        if (buffer.getNumChannels() > 1)
            buffer.setSample(1, i, inR + dr);

        phase += phaseInc;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
    }

    applyMix(buffer, dry, getMix(modMatrix));
}

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
    if (!isEnabled())
        return;

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    phaser.setRate(modMatrix.getModulatedParamValue("phaser_rate", apvts.getRawParameterValue("phaser_rate")->load()));
    phaser.setDepth(modMatrix.getModulatedParamValue("phaser_depth", apvts.getRawParameterValue("phaser_depth")->load()));
    phaser.setFeedback(modMatrix.getModulatedParamValue("phaser_feedback", apvts.getRawParameterValue("phaser_feedback")->load()));
    phaser.setCentreFrequency(modMatrix.getModulatedParamValue("phaser_center", apvts.getRawParameterValue("phaser_center")->load()));

    auto block = juce::dsp::AudioBlock<float>(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    phaser.process(ctx);

    applyMix(buffer, dry, getMix(modMatrix));
}

BitcrusherModule::BitcrusherModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "bitcrusher", ModuleKind::Effect)
{
}

void BitcrusherModule::prepare(const juce::dsp::ProcessSpec&)
{
    downsampleCounter = 0;
    heldSampleL = heldSampleR = 0.0f;
}

void BitcrusherModule::reset()
{
    downsampleCounter = 0;
    heldSampleL = heldSampleR = 0.0f;
}

void BitcrusherModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled())
        return;

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    const float bitDepth = modMatrix.getModulatedParamValue("bitcrusher_bits", apvts.getRawParameterValue("bitcrusher_bits")->load());
    const float downsample = modMatrix.getModulatedParamValue("bitcrusher_downsample", apvts.getRawParameterValue("bitcrusher_downsample")->load());

    const int hold = (int) juce::jlimit(1.0f, 32.0f, std::round(downsample));
    const float step = 1.0f / std::pow(2.0f, bitDepth);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        if (downsampleCounter++ >= hold)
        {
            downsampleCounter = 0;
            heldSampleL = buffer.getSample(0, i);
            heldSampleR = buffer.getNumChannels() > 1 ? buffer.getSample(1, i) : heldSampleL;
        }

        auto crush = [&](float s)
        {
            return std::floor(s / step) * step;
        };

        buffer.setSample(0, i, crush(heldSampleL));
        if (buffer.getNumChannels() > 1)
            buffer.setSample(1, i, crush(heldSampleR));
    }

    applyMix(buffer, dry, getMix(modMatrix));
}

DistortionModule::DistortionModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "distortion", ModuleKind::Effect)
{
}

void DistortionModule::prepare(const juce::dsp::ProcessSpec&) {}
void DistortionModule::reset() {}

void DistortionModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled())
        return;

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    const float drive = modMatrix.getModulatedParamValue("distortion_drive", apvts.getRawParameterValue("distortion_drive")->load());
    const float algo = modMatrix.getModulatedParamValue("distortion_algo", apvts.getRawParameterValue("distortion_algo")->load());

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float x = data[i] * (1.0f + drive * 10.0f);
            if (algo < 0.5f)
                x = std::tanh(x);
            else
                x = juce::jlimit(-1.0f, 1.0f, x);
            data[i] = x;
        }
    }

    applyMix(buffer, dry, getMix(modMatrix));
}

EQModule::EQModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "eq", ModuleKind::Effect)
{
}

void EQModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
    eq.reset();
}

void EQModule::reset()
{
    eq.reset();
}

void EQModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled())
        return;

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    const float lowGain = dbToLin(modMatrix.getModulatedParamValue("eq_low_gain", apvts.getRawParameterValue("eq_low_gain")->load()));
    const float midGain = dbToLin(modMatrix.getModulatedParamValue("eq_mid_gain", apvts.getRawParameterValue("eq_mid_gain")->load()));
    const float mid2Gain = dbToLin(modMatrix.getModulatedParamValue("eq_mid2_gain", apvts.getRawParameterValue("eq_mid2_gain")->load()));
    const float highGain = dbToLin(modMatrix.getModulatedParamValue("eq_high_gain", apvts.getRawParameterValue("eq_high_gain")->load()));

    const float lowFreq = modMatrix.getModulatedParamValue("eq_low_freq", apvts.getRawParameterValue("eq_low_freq")->load());
    const float midFreq = modMatrix.getModulatedParamValue("eq_mid_freq", apvts.getRawParameterValue("eq_mid_freq")->load());
    const float mid2Freq = modMatrix.getModulatedParamValue("eq_mid2_freq", apvts.getRawParameterValue("eq_mid2_freq")->load());
    const float highFreq = modMatrix.getModulatedParamValue("eq_high_freq", apvts.getRawParameterValue("eq_high_freq")->load());

    const float midQ = modMatrix.getModulatedParamValue("eq_mid_q", apvts.getRawParameterValue("eq_mid_q")->load());
    const float mid2Q = modMatrix.getModulatedParamValue("eq_mid2_q", apvts.getRawParameterValue("eq_mid2_q")->load());

    *eq.get<0>().state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, lowFreq, 0.707f, lowGain);
    *eq.get<1>().state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, midFreq, midQ, midGain);
    *eq.get<2>().state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, mid2Freq, mid2Q, mid2Gain);
    *eq.get<3>().state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, highFreq, 0.707f, highGain);

    auto block = juce::dsp::AudioBlock<float>(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    eq.process(ctx);

    applyMix(buffer, dry, getMix(modMatrix));
}

TremoloModule::TremoloModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "tremolo", ModuleKind::Effect)
{
}

void TremoloModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
    phase = 0.0f;
}

void TremoloModule::reset()
{
    phase = 0.0f;
}

void TremoloModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo& transport)
{
    if (!isEnabled())
        return;

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    const bool sync = apvts.getRawParameterValue("tremolo_sync")->load() > 0.5f;
    const int div = (int) std::round(apvts.getRawParameterValue("tremolo_div")->load());

    float rate = modMatrix.getModulatedParamValue("tremolo_rate", apvts.getRawParameterValue("tremolo_rate")->load());
    if (sync)
    {
        const float bpm = (float) (transport.bpm > 0.0 ? transport.bpm : 120.0);
        const float quarterHz = bpm / 60.0f;
        const float mult = (div == 0 ? 0.25f : div == 1 ? 0.5f : div == 2 ? 1.0f : div == 3 ? 2.0f : 4.0f);
        rate = quarterHz * mult;
    }
    const float depth = modMatrix.getModulatedParamValue("tremolo_depth", apvts.getRawParameterValue("tremolo_depth")->load());
    const float mode = modMatrix.getModulatedParamValue("tremolo_mode", apvts.getRawParameterValue("tremolo_mode")->load());

    const float phaseInc = (rate / sampleRate) * juce::MathConstants<float>::twoPi;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float lfo = (std::sin(phase) * 0.5f + 0.5f);
        float gate = (lfo > 0.5f) ? 1.0f : 0.0f;
        float mod = (mode < 0.5f) ? lfo : gate;
        float gain = 1.0f - depth + depth * mod;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer(ch)[i] *= gain;
        phase += phaseInc;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
    }

    applyMix(buffer, dry, getMix(modMatrix));
}

RingModModule::RingModModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "ringmod", ModuleKind::Effect)
{
}

void RingModModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
    phase = 0.0f;
}

void RingModModule::reset()
{
    phase = 0.0f;
}

void RingModModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled())
        return;

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    const float freq = modMatrix.getModulatedParamValue("ringmod_freq", apvts.getRawParameterValue("ringmod_freq")->load());
    const float depth = modMatrix.getModulatedParamValue("ringmod_depth", apvts.getRawParameterValue("ringmod_depth")->load());

    const float phaseInc = (freq / sampleRate) * juce::MathConstants<float>::twoPi;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float mod = std::sin(phase);
        float gain = (1.0f - depth + depth * mod);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer(ch)[i] *= gain;
        phase += phaseInc;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
    }

    applyMix(buffer, dry, getMix(modMatrix));
}

NoiseGenModule::NoiseGenModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "noise", ModuleKind::Generator)
{
}

void NoiseGenModule::prepare(const juce::dsp::ProcessSpec&) {}
void NoiseGenModule::reset() {}

void NoiseGenModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled())
        return;

    const float level = modMatrix.getModulatedParamValue("noise_level", apvts.getRawParameterValue("noise_level")->load());

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] += (rng.nextFloat() * 2.0f - 1.0f) * level;
    }
}

ToneGenModule::ToneGenModule(juce::AudioProcessorValueTreeState& state)
    : FxModule(state, "tone", ModuleKind::Generator)
{
}

void ToneGenModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
    phase = 0.0f;
}

void ToneGenModule::reset()
{
    phase = 0.0f;
}

void ToneGenModule::process(juce::AudioBuffer<float>& buffer, ModMatrix& modMatrix, const FxTransportInfo&)
{
    if (!isEnabled())
        return;

    const float freq = modMatrix.getModulatedParamValue("tone_freq", apvts.getRawParameterValue("tone_freq")->load());
    const float level = modMatrix.getModulatedParamValue("tone_level", apvts.getRawParameterValue("tone_level")->load());
    const float phaseInc = (freq / sampleRate) * juce::MathConstants<float>::twoPi;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float s = std::sin(phase) * level;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer(ch)[i] += s;
        phase += phaseInc;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
    }
}
