#include "DemoFxChain.h"

namespace
{
    inline void mixWet(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>& dry, float mix)
    {
        mix = juce::jlimit(0.0f, 1.0f, mix);
        if (mix >= 0.999f)
            return;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            auto* d = dry.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                wet[i] = d[i] + mix * (wet[i] - d[i]);
        }
    }

    inline float clampSafe(float v, float lo, float hi)
    {
        if (!std::isfinite(v))
            return lo;
        return juce::jlimit(lo, hi, v);
    }

    inline float rhythmToBeats(int idx)
    {
        switch (idx)
        {
            case 0: return 4.0f;   // 1/1
            case 1: return 2.0f;   // 1/2
            case 2: return 1.0f;   // 1/4
            case 3: return 0.5f;   // 1/8
            case 4: return 0.25f;  // 1/16
            default: return 1.0f;
        }
    }
}

DemoFxChain::DemoFxChain(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
}

double DemoFxChain::getBpmOrDefault(juce::AudioPlayHead* playHead, double fallback)
{
    if (playHead == nullptr)
        return fallback;

    juce::AudioPlayHead::CurrentPositionInfo info;
    if (!playHead->getCurrentPosition(info))
        return fallback;

    if (info.bpm > 0.0)
        return info.bpm;

    return fallback;
}

void DemoFxChain::prepare(double sr, int maxSamples, int ch)
{
    sampleRate = sr;
    maxBlockSize = maxSamples;
    numChannels = ch;

    dry.setSize(numChannels, maxBlockSize);

    amountSmoothed.reset(sampleRate, 0.05);
    amountSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("amount")->load());

    preEq.prepare(sampleRate, numChannels, maxBlockSize);
    postEq.prepare(sampleRate, numChannels, maxBlockSize);
    preComp.prepare(sampleRate, maxBlockSize, numChannels);
    postComp.prepare(sampleRate, maxBlockSize, numChannels);
    deesser.prepare(sampleRate, maxBlockSize, numChannels);
    delay1.prepare(sampleRate, maxBlockSize);
    delay2.prepare(sampleRate, maxBlockSize);
    flanger.prepare(sampleRate, maxBlockSize);
    phaser.prepare(sampleRate, maxBlockSize, numChannels);
    distortion.prepare(sampleRate, maxBlockSize, numChannels);
    bitcrush.prepare(sampleRate, maxBlockSize, numChannels);
    reverb.prepare(sampleRate, maxBlockSize, numChannels);
    pitch.prepare(sampleRate, maxBlockSize, numChannels);
}

void DemoFxChain::reset()
{
    preEq.reset();
    postEq.reset();
    preComp.reset();
    postComp.reset();
    deesser.reset();
    delay1.reset();
    delay2.reset();
    flanger.reset();
    phaser.reset();
    distortion.reset();
    bitcrush.reset();
    reverb.reset();
    pitch.reset();
}

void DemoFxChain::process(juce::AudioBuffer<float>& buffer, juce::AudioPlayHead* playHead)
{
    lastBpm = getBpmOrDefault(playHead, lastBpm);

    dry.makeCopyOf(buffer, true);

    // Global macro: Amount is the public knob and should fade the whole effect in/out.
    const float amountTarget = apvts.getRawParameterValue("amount")->load();
    amountSmoothed.setTargetValue(amountTarget);
    const float amountStart = amountSmoothed.getNextValue();
    if (buffer.getNumSamples() > 1)
        amountSmoothed.skip(buffer.getNumSamples() - 1);
    const float amountEnd = amountSmoothed.getCurrentValue();
    const float amount = juce::jlimit(0.0f, 1.0f, 0.5f * (amountStart + amountEnd));

    const float inDb = apvts.getRawParameterValue("inGain")->load();
    const float outDb = apvts.getRawParameterValue("outGain")->load();
    const float inG = dbToLin(inDb);
    const float outG = dbToLin(outDb);
    buffer.applyGain(inG);

    // ---- update parameter blocks ----
    preEq.setEnabled(apvts.getRawParameterValue("preEQEnable")->load() > 0.5f);
    preEq.setCuts(apvts.getRawParameterValue("preEQLowCut")->load(),
                  apvts.getRawParameterValue("preEQHighCut")->load());
    for (int b = 1; b <= 4; ++b)
    {
        const auto s = juce::String(b);
        preEq.setBand(b - 1,
                      apvts.getRawParameterValue("preEQFrequency" + s)->load(),
                      apvts.getRawParameterValue("preEQGain" + s)->load(),
                      apvts.getRawParameterValue("preEQQuality" + s)->load());
    }

    postEq.setEnabled(apvts.getRawParameterValue("postEQEnable")->load() > 0.5f);
    postEq.setCuts(apvts.getRawParameterValue("postEQLowCut")->load(),
                   apvts.getRawParameterValue("postEQHighCut")->load());
    for (int b = 1; b <= 4; ++b)
    {
        const auto s = juce::String(b);
        postEq.setBand(b - 1,
                       apvts.getRawParameterValue("postEQFrequency" + s)->load(),
                       apvts.getRawParameterValue("postEQGain" + s)->load(),
                       apvts.getRawParameterValue("postEQQuality" + s)->load());
    }

    preComp.setEnabled(apvts.getRawParameterValue("preCompressorEnable")->load() > 0.5f);
    preComp.setParams(apvts.getRawParameterValue("preCompressorThreshold")->load(),
                      apvts.getRawParameterValue("preCompressorRatio")->load(),
                      apvts.getRawParameterValue("preCompressorAttack")->load(),
                      apvts.getRawParameterValue("preCompressorRelease")->load());
    preComp.setInOutGain(apvts.getRawParameterValue("preCompressorInGain")->load(),
                         apvts.getRawParameterValue("preCompressorOutGain")->load());

    postComp.setEnabled(apvts.getRawParameterValue("postCompressorEnable")->load() > 0.5f);
    postComp.setParams(apvts.getRawParameterValue("postCompressorThreshold")->load(),
                       apvts.getRawParameterValue("postCompressorRatio")->load(),
                       apvts.getRawParameterValue("postCompressorAttack")->load(),
                       apvts.getRawParameterValue("postCompressorRelease")->load());
    postComp.setInOutGain(apvts.getRawParameterValue("postCompressorInGain")->load(),
                          apvts.getRawParameterValue("postCompressorOutGain")->load());

    deesser.setEnabled(apvts.getRawParameterValue("deesserEnable")->load() > 0.5f);
    deesser.setParams(apvts.getRawParameterValue("deesserFrequency")->load(),
                      apvts.getRawParameterValue("deesserThreshold")->load());

    delay1.setEnabled(apvts.getRawParameterValue("delayEnable1")->load() > 0.5f);
    delay1.setParams((int) apvts.getRawParameterValue("delayType1")->load(),
                     apvts.getRawParameterValue("delaySync1")->load() > 0.5f,
                     (int) apvts.getRawParameterValue("delayRhythm1")->load(),
                     apvts.getRawParameterValue("delayTime1")->load(),
                     apvts.getRawParameterValue("delayFeedback1")->load(),
                     apvts.getRawParameterValue("delayMix1")->load() * amount,
                     apvts.getRawParameterValue("delayHP1")->load(),
                     apvts.getRawParameterValue("delayLP1")->load(),
                     apvts.getRawParameterValue("delayLfoRate1")->load(),
                     apvts.getRawParameterValue("delayLfoDepth1")->load());

    delay2.setEnabled(apvts.getRawParameterValue("delayEnable2")->load() > 0.5f);
    delay2.setParams((int) apvts.getRawParameterValue("delayType2")->load(),
                     apvts.getRawParameterValue("delaySync2")->load() > 0.5f,
                     (int) apvts.getRawParameterValue("delayRhythm2")->load(),
                     apvts.getRawParameterValue("delayTime2")->load(),
                     apvts.getRawParameterValue("delayFeedback2")->load(),
                     apvts.getRawParameterValue("delayMix2")->load() * amount,
                     apvts.getRawParameterValue("delayHP2")->load(),
                     apvts.getRawParameterValue("delayLP2")->load(),
                     apvts.getRawParameterValue("delayLfoRate2")->load(),
                     apvts.getRawParameterValue("delayLfoDepth2")->load());

    distortion.setEnabled(apvts.getRawParameterValue("distortionEnable")->load() > 0.5f);
    distortion.setParams(apvts.getRawParameterValue("distortionDrive1")->load(),
                         apvts.getRawParameterValue("distortionDrive2")->load(),
                         apvts.getRawParameterValue("distortionMix1")->load() * amount,
                         apvts.getRawParameterValue("distortionMix2")->load() * amount);

    phaser.setEnabled(apvts.getRawParameterValue("phaserEnable")->load() > 0.5f);
    phaser.setParams(apvts.getRawParameterValue("phaserFrequency")->load(),
                     apvts.getRawParameterValue("phaserIntensity")->load(),
                     apvts.getRawParameterValue("phaserDepth")->load(),
                     apvts.getRawParameterValue("phaserMix")->load() * amount);

    flanger.setEnabled(apvts.getRawParameterValue("flangerEnable")->load() > 0.5f);
    flanger.setParams(apvts.getRawParameterValue("flangerFrequency")->load(),
                      apvts.getRawParameterValue("flangerIntensity")->load(),
                      apvts.getRawParameterValue("flangerFeedback")->load(),
                      apvts.getRawParameterValue("flangerMix")->load() * amount);

    bitcrush.setParams(apvts.getRawParameterValue("bitCrushDepth")->load(),
                       apvts.getRawParameterValue("bitCrushFrequency")->load(),
                       apvts.getRawParameterValue("bitCrushHard")->load(),
                       apvts.getRawParameterValue("bitCrushMix")->load() * amount);

    reverb.setEnabled(apvts.getRawParameterValue("reverbEnable")->load() > 0.5f);
    reverb.setParams((int) apvts.getRawParameterValue("reverbType")->load(),
                     apvts.getRawParameterValue("reverbDecayTime")->load(),
                     apvts.getRawParameterValue("reverbPreDelay")->load(),
                     apvts.getRawParameterValue("reverbMix")->load() * amount);

    pitch.setEnabled(apvts.getRawParameterValue("pitchShifterEnable")->load() > 0.5f);
    pitch.setParams(apvts.getRawParameterValue("pitchShifterSemitones")->load(),
                    apvts.getRawParameterValue("pitchShifterMix")->load() * amount);

    // ---- process chain ----
    preEq.process(buffer);
    preComp.process(buffer);
    pitch.process(buffer);
    delay1.process(buffer, lastBpm);
    delay2.process(buffer, lastBpm);
    distortion.process(buffer);
    phaser.process(buffer);
    flanger.process(buffer);
    bitcrush.process(buffer);
    reverb.process(buffer);
    postEq.process(buffer);
    deesser.process(buffer);
    postComp.process(buffer);

    buffer.applyGain(outG);

    // Final global wet/dry by Amount (Amount=0 => dry, Amount=1 => fully processed).
    mixWet(buffer, dry, amount);
}

// ===================== Eq4 =====================

void DemoFxChain::Eq4::prepare(double sr, int ch, int)
{
    sampleRate = sr;
    numChannels = ch;
    for (auto& f : filtersL) f.reset();
    for (auto& f : filtersR) f.reset();
    coeffs.fill(juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, 1000.0));
    for (int i = 0; i < (int) coeffs.size(); ++i)
    {
        filtersL[(size_t) i].coefficients = coeffs[(size_t) i];
        filtersR[(size_t) i].coefficients = coeffs[(size_t) i];
    }
}

void DemoFxChain::Eq4::reset()
{
    for (auto& f : filtersL) f.reset();
    for (auto& f : filtersR) f.reset();
}

void DemoFxChain::Eq4::setCuts(float lowCutHz, float highCutHz)
{
    lowCutHz = clampSafe(lowCutHz, 20.0f, 20000.0f);
    highCutHz = clampSafe(highCutHz, 20.0f, 20000.0f);

    coeffs[0] = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, lowCutHz);
    coeffs[1] = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, highCutHz);

    filtersL[0].coefficients = coeffs[0];
    filtersR[0].coefficients = coeffs[0];
    filtersL[1].coefficients = coeffs[1];
    filtersR[1].coefficients = coeffs[1];
}

void DemoFxChain::Eq4::setBand(int bandIndex0, float freqHz, float gainDb, float q)
{
    if (bandIndex0 < 0 || bandIndex0 > 3)
        return;

    freqHz = clampSafe(freqHz, 20.0f, 20000.0f);
    q = clampSafe(q, 0.2f, 10.0f);

    const float gain = juce::Decibels::decibelsToGain(gainDb);
    coeffs[(size_t) (2 + bandIndex0)] = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, freqHz, q, gain);

    filtersL[(size_t) (2 + bandIndex0)].coefficients = coeffs[(size_t) (2 + bandIndex0)];
    filtersR[(size_t) (2 + bandIndex0)].coefficients = coeffs[(size_t) (2 + bandIndex0)];
}

void DemoFxChain::Eq4::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled)
        return;

    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto left = block.getSingleChannelBlock(0);
    juce::dsp::ProcessContextReplacing<float> ctxL(left);

    for (int i = 0; i < 6; ++i)
        filtersL[(size_t) i].process(ctxL);

    if (buffer.getNumChannels() > 1)
    {
        auto right = block.getSingleChannelBlock(1);
        juce::dsp::ProcessContextReplacing<float> ctxR(right);
        for (int i = 0; i < 6; ++i)
            filtersR[(size_t) i].process(ctxR);
    }
}

// ===================== Comp =====================

void DemoFxChain::Comp::prepare(double sr, int, int ch)
{
    juce::dsp::ProcessSpec spec { sr, 2048u, (juce::uint32) ch };
    comp.prepare(spec);
    comp.reset();
}

void DemoFxChain::Comp::reset()
{
    comp.reset();
}

void DemoFxChain::Comp::setParams(float thresholdDb, float ratio, float attackMs, float releaseMs)
{
    comp.setThreshold(thresholdDb);
    comp.setRatio(clampSafe(ratio, 1.0f, 20.0f));
    comp.setAttack(clampSafe(attackMs, 0.1f, 200.0f));
    comp.setRelease(clampSafe(releaseMs, 10.0f, 2000.0f));
}

void DemoFxChain::Comp::setInOutGain(float inDb, float outDb)
{
    inGain = juce::Decibels::decibelsToGain(inDb);
    outGain = juce::Decibels::decibelsToGain(outDb);
}

void DemoFxChain::Comp::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled)
        return;

    buffer.applyGain(inGain);
    auto block = juce::dsp::AudioBlock<float>(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    comp.process(ctx);
    buffer.applyGain(outGain);
}

// ===================== DeEsser =====================

void DemoFxChain::DeEsser::prepare(double sr, int, int ch)
{
    sampleRate = sr;
    juce::dsp::ProcessSpec spec { sr, 2048u, (juce::uint32) ch };
    comp.prepare(spec);
    reset();
    setParams(6000.0f, -24.0f);
}

void DemoFxChain::DeEsser::reset()
{
    hpL.reset();
    hpR.reset();
    comp.reset();
}

void DemoFxChain::DeEsser::setParams(float freqHz, float thresholdDb)
{
    freqHz = clampSafe(freqHz, 1000.0f, 12000.0f);
    hpCoef = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, freqHz);
    hpL.coefficients = hpCoef;
    hpR.coefficients = hpCoef;

    comp.setThreshold(thresholdDb);
    comp.setRatio(6.0f);
    comp.setAttack(2.0f);
    comp.setRelease(80.0f);
}

void DemoFxChain::DeEsser::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled)
        return;

    juce::AudioBuffer<float> hf;
    hf.makeCopyOf(buffer, true);

    auto block = juce::dsp::AudioBlock<float>(hf);
    auto left = block.getSingleChannelBlock(0);
    juce::dsp::ProcessContextReplacing<float> ctxL(left);
    hpL.process(ctxL);
    if (hf.getNumChannels() > 1)
    {
        auto right = block.getSingleChannelBlock(1);
        juce::dsp::ProcessContextReplacing<float> ctxR(right);
        hpR.process(ctxR);
    }

    // Compress high band
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    comp.process(ctx);

    // Subtract compressed HF delta from original to reduce sibilance.
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* dst = buffer.getWritePointer(ch);
        auto* hfIn = hf.getReadPointer(ch);
        auto* dryPtr = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float hfDelta = dryPtr[i] - hfIn[i];
            dst[i] = dst[i] - hfDelta;
        }
    }
}

// ===================== Delay =====================

void DemoFxChain::Delay::prepare(double sr, int)
{
    sampleRate = sr;
    dl.reset();
    dr.reset();
    dl.setDelay(0.0f);
    dr.setDelay(0.0f);
    hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
    phase = 0.0f;
    fbStateL = fbStateR = 0.0f;
}

void DemoFxChain::Delay::reset()
{
    dl.reset();
    dr.reset();
    phase = 0.0f;
    fbStateL = fbStateR = 0.0f;
    hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
}

void DemoFxChain::Delay::setParams(int t, bool s, int r, float tm, float fb, float m, float hp, float lp, float lr, float ld)
{
    type = juce::jlimit(0, 2, t);
    sync = s;
    rhythm = juce::jlimit(0, 4, r);
    timeMs = clampSafe(tm, 1.0f, 2000.0f);
    feedback = clampSafe(fb, 0.0f, 0.95f);
    mix = clampSafe(m, 0.0f, 1.0f);
    hpHz = clampSafe(hp, 20.0f, 20000.0f);
    lpHz = clampSafe(lp, 20.0f, 20000.0f);
    lfoRate = clampSafe(lr, 0.0f, 20.0f);
    lfoDepth = clampSafe(ld, 0.0f, 1.0f);
}

void DemoFxChain::Delay::process(juce::AudioBuffer<float>& buffer, double bpm)
{
    if (!enabled || mix <= 0.0001f)
        return;

    juce::AudioBuffer<float> localDry;
    localDry.makeCopyOf(buffer, true);

    const float sr = (float) sampleRate;
    const float secondsPerBeat = 60.0f / (float) (bpm > 0.0 ? bpm : 120.0);
    const float baseSamples = sync
        ? rhythmToBeats(rhythm) * secondsPerBeat * sr
        : timeMs * 0.001f * sr;

    const float lfo = (lfoRate > 0.0f && lfoDepth > 0.0f)
        ? (std::sin(phase) * 0.5f + 0.5f)
        : 0.0f;
    const float mod = 1.0f + lfoDepth * 0.10f * (2.0f * lfo - 1.0f);
    const float delaySamples = juce::jlimit(1.0f, sr * 2.0f, baseSamples * mod);

    phase += (float) (juce::MathConstants<double>::twoPi * (double) lfoRate / sampleRate) * (float) buffer.getNumSamples();
    while (phase > juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;

    dl.setDelay(delaySamples);
    dr.setDelay(delaySamples);

    hpCoef = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, hpHz);
    lpCoef = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, lpHz);
    hpL.coefficients = hpCoef; hpR.coefficients = hpCoef;
    lpL.coefficients = lpCoef; lpR.coefficients = lpCoef;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float inL = buffer.getSample(0, i);
        float inR = buffer.getNumChannels() > 1 ? buffer.getSample(1, i) : inL;

        float wetL = dl.popSample(0);
        float wetR = dr.popSample(0);

        float fbL = wetL * feedback;
        float fbR = wetR * feedback;

        // Ping-pong crossfeed
        if (type == 1)
            std::swap(fbL, fbR);

        // Tape: low-pass + gentle saturation in feedback
        if (type == 2)
        {
            fbStateL = fbStateL + 0.08f * (fbL - fbStateL);
            fbStateR = fbStateR + 0.08f * (fbR - fbStateR);
            fbL = std::tanh(fbStateL * 1.7f);
            fbR = std::tanh(fbStateR * 1.7f);
        }

        // Feedback filtering (hp/lp)
        {
            float tmp = fbL;
            tmp = hpL.processSample(tmp);
            tmp = lpL.processSample(tmp);
            fbL = tmp;
        }
        {
            float tmp = fbR;
            tmp = hpR.processSample(tmp);
            tmp = lpR.processSample(tmp);
            fbR = tmp;
        }

        dl.pushSample(0, inL + fbL);
        dr.pushSample(0, inR + fbR);

        buffer.setSample(0, i, inL + wetL);
        if (buffer.getNumChannels() > 1)
            buffer.setSample(1, i, inR + wetR);
    }

    mixWet(buffer, localDry, mix);
}

// ===================== Flanger =====================

void DemoFxChain::Flanger::prepare(double sr, int)
{
    sampleRate = sr;
    reset();
}

void DemoFxChain::Flanger::reset()
{
    dl.reset();
    dr.reset();
    phase = 0.0f;
}

void DemoFxChain::Flanger::setParams(float r, float i, float fb, float m)
{
    rateHz = clampSafe(r, 0.01f, 5.0f);
    intensity = clampSafe(i, 0.0f, 1.0f);
    feedback = clampSafe(fb, -0.95f, 0.95f);
    mix = clampSafe(m, 0.0f, 1.0f);
}

void DemoFxChain::Flanger::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled || mix <= 0.0001f)
        return;

    juce::AudioBuffer<float> localDry;
    localDry.makeCopyOf(buffer, true);

    const float sr = (float) sampleRate;
    const float phaseInc = (rateHz / sr) * juce::MathConstants<float>::twoPi;
    const float baseDelayMs = 0.8f;
    const float maxDelayMs = 8.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float lfo = std::sin(phase) * 0.5f + 0.5f;
        const float delaySamples = (baseDelayMs + intensity * maxDelayMs * lfo) * 0.001f * sr;
        dl.setDelay(delaySamples);
        dr.setDelay(delaySamples);

        float inL = buffer.getSample(0, i);
        float inR = buffer.getNumChannels() > 1 ? buffer.getSample(1, i) : inL;

        float wetL = dl.popSample(0);
        float wetR = dr.popSample(0);

        dl.pushSample(0, inL + wetL * feedback);
        dr.pushSample(0, inR + wetR * feedback);

        buffer.setSample(0, i, inL + wetL);
        if (buffer.getNumChannels() > 1)
            buffer.setSample(1, i, inR + wetR);

        phase += phaseInc;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
    }

    mixWet(buffer, localDry, mix);
}

// ===================== Phaser =====================

void DemoFxChain::Phaser::prepare(double sr, int maxSamples, int ch)
{
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxSamples, (juce::uint32) ch };
    phaser.prepare(spec);
    reset();
}

void DemoFxChain::Phaser::reset()
{
    phaser.reset();
}

void DemoFxChain::Phaser::setParams(float r, float i, float d, float m)
{
    rateHz = clampSafe(r, 0.01f, 5.0f);
    intensity = clampSafe(i, 0.0f, 1.0f);
    depth = clampSafe(d, 0.0f, 1.0f);
    mix = clampSafe(m, 0.0f, 1.0f);
}

void DemoFxChain::Phaser::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled || mix <= 0.0001f)
        return;

    juce::AudioBuffer<float> localDry;
    localDry.makeCopyOf(buffer, true);

    phaser.setRate(rateHz);
    phaser.setDepth(depth);
    phaser.setFeedback(0.2f * (2.0f * intensity - 1.0f));
    phaser.setCentreFrequency(200.0f + intensity * 1800.0f);

    auto block = juce::dsp::AudioBlock<float>(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    phaser.process(ctx);

    mixWet(buffer, localDry, mix);
}

// ===================== Distortion =====================

void DemoFxChain::Distortion::prepare(double, int, int) {}
void DemoFxChain::Distortion::reset() {}

void DemoFxChain::Distortion::setParams(float d1, float d2, float m1, float m2)
{
    drive1 = clampSafe(d1, 0.0f, 10.0f);
    drive2 = clampSafe(d2, 0.0f, 10.0f);
    mix1 = clampSafe(m1, 0.0f, 1.0f);
    mix2 = clampSafe(m2, 0.0f, 1.0f);
}

void DemoFxChain::Distortion::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled || (mix1 <= 0.0001f && mix2 <= 0.0001f))
        return;

    juce::AudioBuffer<float> localDry;
    localDry.makeCopyOf(buffer, true);

    const auto stage = [&](float x, float drive)
    {
        const float g = 1.0f + drive * 2.0f;
        return std::tanh(x * g);
    };

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        auto* d = localDry.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float x = d[i];
            float y1 = stage(x, drive1);
            float y = x + mix1 * (y1 - x);
            float y2 = stage(y, drive2);
            y = y + mix2 * (y2 - y);
            data[i] = y;
        }
    }
}

// ===================== BitCrush =====================

void DemoFxChain::BitCrush::prepare(double, int, int) { reset(); }

void DemoFxChain::BitCrush::reset()
{
    counter = 0;
    heldL = heldR = 0.0f;
}

void DemoFxChain::BitCrush::setParams(float d, float f, float h, float m)
{
    depth = clampSafe(d, 0.0f, 1.0f);
    freq = clampSafe(f, 0.0f, 1.0f);
    hard = clampSafe(h, 0.0f, 1.0f);
    mix = clampSafe(m, 0.0f, 1.0f);
}

void DemoFxChain::BitCrush::process(juce::AudioBuffer<float>& buffer)
{
    if (mix <= 0.0001f)
        return;

    juce::AudioBuffer<float> localDry;
    localDry.makeCopyOf(buffer, true);

    const float bits = 16.0f - depth * 14.0f; // 16 -> 2
    const float step = 1.0f / std::pow(2.0f, bits);
    const int hold = juce::jlimit(1, 32, (int) std::round(1.0f + (1.0f - freq) * 31.0f));

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        if (counter++ >= hold)
        {
            counter = 0;
            heldL = buffer.getSample(0, i);
            heldR = buffer.getNumChannels() > 1 ? buffer.getSample(1, i) : heldL;
        }

        auto crush = [&](float s)
        {
            const float q = std::floor(s / step) * step;
            if (hard > 0.5f)
                return juce::jlimit(-1.0f, 1.0f, q);
            return std::tanh(q * 1.5f);
        };

        buffer.setSample(0, i, crush(heldL));
        if (buffer.getNumChannels() > 1)
            buffer.setSample(1, i, crush(heldR));
    }

    mixWet(buffer, localDry, mix);
}

// ===================== Reverb =====================

void DemoFxChain::Reverb::prepare(double sr, int maxSamples, int ch)
{
    sampleRate = sr;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxSamples, (juce::uint32) ch };
    reverb.reset();
    convolution.prepare(spec);
    preDelay.reset();
    preDelay.setDelay(0.0f);

    // Generated plate-ish IR.
    juce::AudioBuffer<float> ir(1, 4096);
    juce::Random rng(12345);
    const float plateDecaySeconds = 1.1f;
    const float decayCoeff = std::exp(-1.0f / ((float) sampleRate * plateDecaySeconds));
    float env = 1.0f;
    for (int i = 0; i < ir.getNumSamples(); ++i)
    {
        ir.setSample(0, i, (rng.nextFloat() * 2.0f - 1.0f) * env);
        env *= decayCoeff;
    }
    convolution.loadImpulseResponse(std::move(ir), sampleRate,
                                    juce::dsp::Convolution::Stereo::no,
                                    juce::dsp::Convolution::Trim::yes,
                                    juce::dsp::Convolution::Normalise::yes);
}

void DemoFxChain::Reverb::reset()
{
    reverb.reset();
    convolution.reset();
    preDelay.reset();
}

void DemoFxChain::Reverb::setParams(int t, float decay, float preMs, float m)
{
    type = juce::jlimit(0, 1, t);
    decaySeconds = clampSafe(decay, 0.05f, 10.0f);
    predelayMs = clampSafe(preMs, 0.0f, 250.0f);
    mix = clampSafe(m, 0.0f, 1.0f);
}

void DemoFxChain::Reverb::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled || mix <= 0.0001f)
        return;

    juce::AudioBuffer<float> localDry;
    localDry.makeCopyOf(buffer, true);

    const float preSamples = (predelayMs * 0.001f) * (float) sampleRate;
    preDelay.setDelay(juce::jlimit(0.0f, (float) sampleRate, preSamples));

    // Apply pre-delay in-place
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float x = buffer.getSample(ch, i);
            const float y = preDelay.popSample(0);
            preDelay.pushSample(0, x);
            buffer.setSample(ch, i, y);
        }
    }

    juce::dsp::Reverb::Parameters p;
    p.roomSize = juce::jlimit(0.0f, 1.0f, decaySeconds / 10.0f);
    p.damping = 0.5f;
    p.width = 1.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters(p);

    auto block = juce::dsp::AudioBlock<float>(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);

    if (type == 0)
        reverb.process(ctx);
    else
    {
        convolution.process(ctx);
        reverb.process(ctx);
    }

    mixWet(buffer, localDry, mix);
}

// ===================== PitchShifter =====================

void DemoFxChain::PitchShifter::prepare(double sr, int maxSamples, int ch)
{
    sampleRate = sr;
    numChannels = ch;
    ringSize = (int) std::round(sampleRate * 0.12); // 120ms window
    ringSize = juce::jlimit(2048, 131072, ringSize);
    ring.setSize(numChannels, ringSize);
    ring.clear();
    writePos = 0;
    readPosA = 0.0f;
    readPosB = (float) ringSize * 0.5f;
    speed = 1.0f;
    juce::ignoreUnused(maxSamples);
}

void DemoFxChain::PitchShifter::reset()
{
    ring.clear();
    writePos = 0;
    readPosA = 0.0f;
    readPosB = (float) ringSize * 0.5f;
}

void DemoFxChain::PitchShifter::setParams(float st, float m)
{
    semitones = clampSafe(st, -24.0f, 24.0f);
    mix = clampSafe(m, 0.0f, 1.0f);
    speed = std::pow(2.0f, semitones / 12.0f);
}

void DemoFxChain::PitchShifter::process(juce::AudioBuffer<float>& buffer)
{
    if (!enabled || mix <= 0.0001f || std::abs(semitones) < 0.001f)
        return;

    juce::AudioBuffer<float> localDry;
    localDry.makeCopyOf(buffer, true);

    const auto readSample = [&](int ch, float pos) -> float
    {
        // Linear interpolation
        int i0 = (int) std::floor(pos);
        int i1 = (i0 + 1) % ringSize;
        float frac = pos - (float) i0;
        auto* r = ring.getReadPointer(ch);
        return r[i0] + frac * (r[i1] - r[i0]);
    };

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float aDist = std::fmod((float) writePos - readPosA + (float) ringSize, (float) ringSize);
        const float bDist = std::fmod((float) writePos - readPosB + (float) ringSize, (float) ringSize);
        const float xfadeA = juce::jlimit(0.0f, 1.0f, aDist / ((float) ringSize * 0.5f));
        const float xfadeB = juce::jlimit(0.0f, 1.0f, bDist / ((float) ringSize * 0.5f));

        const float wA = std::sin(xfadeA * juce::MathConstants<float>::pi);
        const float wB = std::sin(xfadeB * juce::MathConstants<float>::pi);
        const float norm = (wA + wB) > 0.0001f ? (1.0f / (wA + wB)) : 1.0f;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float in = buffer.getSample(ch, i);
            ring.setSample(ch, writePos, in);
            const float yA = readSample(ch, readPosA);
            const float yB = readSample(ch, readPosB);
            const float y = (wA * yA + wB * yB) * norm;
            buffer.setSample(ch, i, y);
        }

        writePos = (writePos + 1) % ringSize;
        readPosA += speed;
        readPosB += speed;
        if (readPosA >= (float) ringSize) readPosA -= (float) ringSize;
        if (readPosB >= (float) ringSize) readPosB -= (float) ringSize;

        // Keep read heads away from write head by wrapping when too close.
        if (std::abs((float) writePos - readPosA) < 32.0f)
            readPosA = std::fmod(readPosA + (float) ringSize * 0.5f, (float) ringSize);
        if (std::abs((float) writePos - readPosB) < 32.0f)
            readPosB = std::fmod(readPosB + (float) ringSize * 0.5f, (float) ringSize);
    }

    mixWet(buffer, localDry, mix);
}
