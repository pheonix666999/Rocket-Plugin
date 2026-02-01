#include "FxChain.h"

FxChain::FxChain(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    modules.add(new ModuleEntry{ std::make_unique<ReverbModule>(apvts), ModuleKind::Effect });
    modules.add(new ModuleEntry{ std::make_unique<DelayModule>(apvts), ModuleKind::Effect });
    modules.add(new ModuleEntry{ std::make_unique<FilterModule>(apvts, FilterModule::Type::LowPass, "lpf"), ModuleKind::Effect });
    modules.add(new ModuleEntry{ std::make_unique<FilterModule>(apvts, FilterModule::Type::HighPass, "hpf"), ModuleKind::Effect });
    modules.add(new ModuleEntry{ std::make_unique<FlangerModule>(apvts), ModuleKind::Effect });
    modules.add(new ModuleEntry{ std::make_unique<PhaserModule>(apvts), ModuleKind::Effect });
    modules.add(new ModuleEntry{ std::make_unique<BitcrusherModule>(apvts), ModuleKind::Effect });
    modules.add(new ModuleEntry{ std::make_unique<DistortionModule>(apvts), ModuleKind::Effect });
    modules.add(new ModuleEntry{ std::make_unique<EQModule>(apvts), ModuleKind::Effect });
    modules.add(new ModuleEntry{ std::make_unique<TremoloModule>(apvts), ModuleKind::Effect });
    modules.add(new ModuleEntry{ std::make_unique<RingModModule>(apvts), ModuleKind::Effect });
    modules.add(new ModuleEntry{ std::make_unique<NoiseGenModule>(apvts), ModuleKind::Generator });
    modules.add(new ModuleEntry{ std::make_unique<ToneGenModule>(apvts), ModuleKind::Generator });

    buildDefaultOrder();
}

void FxChain::buildDefaultOrder()
{
    order.clear();
    for (auto* entry : modules)
    {
        if (entry->kind == ModuleKind::Effect)
            order.add(entry->module->getId());
    }
}

void FxChain::prepare(const juce::dsp::ProcessSpec& spec)
{
    for (auto* entry : modules)
        entry->module->prepare(spec);

    genBuffer.setSize((int) spec.numChannels, (int) spec.maximumBlockSize);
    dryGenBuffer.setSize((int) spec.numChannels, (int) spec.maximumBlockSize);
}

void FxChain::reset()
{
    for (auto* entry : modules)
        entry->module->reset();
}

void FxChain::process(juce::AudioBuffer<float>& buffer,
                      juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>&,
                      ModMatrix& modMatrix,
                      const FxTransportInfo& transport)
{
    genBuffer.clear();
    for (auto* entry : modules)
    {
        if (entry->kind == ModuleKind::Generator)
            entry->module->process(genBuffer, modMatrix, transport);
    }

    const bool genToChain = apvts.getRawParameterValue("gen_to_chain")->load() > 0.5f;
    if (genToChain)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom(ch, 0, genBuffer, ch, 0, buffer.getNumSamples());
    }

    for (const auto& id : order)
    {
        if (auto* entry = findModuleById(id))
            entry->module->process(buffer, modMatrix, transport);
    }

    if (!genToChain)
    {
        const float genMix = modMatrix.getModulatedParamValue("gen_mix", apvts.getRawParameterValue("gen_mix")->load());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom(ch, 0, genBuffer, ch, 0, buffer.getNumSamples(), genMix);
    }
}

void FxChain::moveModule(int fromIndex, int toIndex)
{
    if (!juce::isPositiveAndBelow(fromIndex, order.size()) || !juce::isPositiveAndBelow(toIndex, order.size()))
        return;
    order.move(fromIndex, toIndex);
}

juce::StringArray FxChain::getModuleOrder() const
{
    return order;
}

void FxChain::setModuleOrder(const juce::StringArray& newOrder)
{
    order = newOrder;
}

FxChain::ModuleEntry* FxChain::findModuleById(const juce::String& id) const
{
    for (auto* entry : modules)
    {
        if (entry->module->getId() == id)
            return entry;
    }
    return nullptr;
}

void FxChain::appendState(juce::ValueTree& parent) const
{
    juce::ValueTree chain("FX_CHAIN");
    chain.setProperty("gen_to_chain", apvts.getRawParameterValue("gen_to_chain")->load(), nullptr);
    chain.setProperty("gen_mix", apvts.getRawParameterValue("gen_mix")->load(), nullptr);

    for (const auto& id : order)
    {
        juce::ValueTree child("MODULE");
        child.setProperty("id", id, nullptr);
        chain.addChild(child, -1, nullptr);
    }

    parent.addChild(chain, -1, nullptr);
}

void FxChain::restoreFromState(const juce::ValueTree& parent)
{
    auto chain = parent.getChildWithName("FX_CHAIN");
    if (!chain.isValid())
        return;

    juce::StringArray newOrder;
    for (int i = 0; i < chain.getNumChildren(); ++i)
        newOrder.add(chain.getChild(i).getProperty("id").toString());

    if (newOrder.size() > 0)
        order = newOrder;
}

void FxChain::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    auto addBool = [&](const juce::String& id, const juce::String& name)
    {
        layout.add(std::make_unique<juce::AudioParameterBool>(id, name, true));
    };
    auto addMix = [&](const juce::String& id)
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(id + "_mix", id + " Mix", 0.0f, 1.0f, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterBool>(id + "_enabled", id + " Enabled", true));
    };

    addMix("reverb");
    layout.add(std::make_unique<juce::AudioParameterChoice>("reverb_algo", "Reverb Type", juce::StringArray { "Hall", "Plate" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("reverb_size", "Reverb Size", 0.0f, 1.0f, 0.4f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("reverb_damping", "Reverb Damping", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("reverb_width", "Reverb Width", 0.0f, 1.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("reverb_freeze", "Reverb Freeze", 0.0f, 1.0f, 0.0f));

    addMix("delay");
    layout.add(std::make_unique<juce::AudioParameterFloat>("delay_time1", "Delay Time 1", 1.0f, 2000.0f, 250.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("delay_time2", "Delay Time 2", 1.0f, 2000.0f, 500.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("delay_fb1", "Delay Feedback 1", 0.0f, 0.95f, 0.3f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("delay_fb2", "Delay Feedback 2", 0.0f, 0.95f, 0.25f));
    layout.add(std::make_unique<juce::AudioParameterChoice>("delay_mode1", "Delay Mode 1", juce::StringArray { "Digital", "PingPong", "Tape" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("delay_mode2", "Delay Mode 2", juce::StringArray { "Digital", "PingPong", "Tape" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("delay_tape_tone", "Delay Tape Tone", 0.0f, 1.0f, 0.6f));
    layout.add(std::make_unique<juce::AudioParameterBool>("delay_sync1", "Delay Sync 1", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>("delay_div1", "Delay Division 1", juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16" }, 2));
    layout.add(std::make_unique<juce::AudioParameterBool>("delay_sync2", "Delay Sync 2", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>("delay_div2", "Delay Division 2", juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16" }, 2));

    addMix("lpf");
    layout.add(std::make_unique<juce::AudioParameterFloat>("lpf_cutoff", "LPF Cutoff", 20.0f, 20000.0f, 12000.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>("lpf_slope", "LPF Slope", juce::StringArray { "6 dB/oct", "12 dB/oct", "24 dB/oct", "96 dB/oct" }, 2));

    addMix("hpf");
    layout.add(std::make_unique<juce::AudioParameterFloat>("hpf_cutoff", "HPF Cutoff", 20.0f, 20000.0f, 30.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>("hpf_slope", "HPF Slope", juce::StringArray { "6 dB/oct", "12 dB/oct", "24 dB/oct", "96 dB/oct" }, 1));

    addMix("flanger");
    layout.add(std::make_unique<juce::AudioParameterFloat>("flanger_rate", "Flanger Rate", 0.01f, 5.0f, 0.25f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("flanger_depth", "Flanger Depth", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("flanger_feedback", "Flanger Feedback", 0.0f, 0.95f, 0.2f));

    addMix("phaser");
    layout.add(std::make_unique<juce::AudioParameterFloat>("phaser_rate", "Phaser Rate", 0.01f, 5.0f, 0.2f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("phaser_depth", "Phaser Depth", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("phaser_feedback", "Phaser Feedback", -0.95f, 0.95f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("phaser_center", "Phaser Center", 100.0f, 2000.0f, 400.0f));

    addMix("bitcrusher");
    layout.add(std::make_unique<juce::AudioParameterFloat>("bitcrusher_bits", "Bit Depth", 2.0f, 16.0f, 8.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("bitcrusher_downsample", "Downsample", 1.0f, 16.0f, 1.0f));

    addMix("distortion");
    layout.add(std::make_unique<juce::AudioParameterFloat>("distortion_drive", "Distortion Drive", 0.0f, 1.0f, 0.3f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("distortion_algo", "Distortion Algorithm", 0.0f, 1.0f, 0.0f));

    addMix("eq");
    layout.add(std::make_unique<juce::AudioParameterFloat>("eq_low_freq", "EQ Low Freq", 20.0f, 500.0f, 120.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eq_mid_freq", "EQ Mid Freq", 100.0f, 4000.0f, 800.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eq_mid2_freq", "EQ Mid2 Freq", 400.0f, 8000.0f, 2400.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eq_high_freq", "EQ High Freq", 2000.0f, 20000.0f, 9000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eq_low_gain", "EQ Low Gain", -24.0f, 24.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eq_mid_gain", "EQ Mid Gain", -24.0f, 24.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eq_mid2_gain", "EQ Mid2 Gain", -24.0f, 24.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eq_high_gain", "EQ High Gain", -24.0f, 24.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eq_mid_q", "EQ Mid Q", 0.2f, 10.0f, 0.7f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("eq_mid2_q", "EQ Mid2 Q", 0.2f, 10.0f, 0.7f));

    addMix("tremolo");
    layout.add(std::make_unique<juce::AudioParameterFloat>("tremolo_rate", "Tremolo Rate", 0.1f, 20.0f, 4.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tremolo_depth", "Tremolo Depth", 0.0f, 1.0f, 0.7f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tremolo_mode", "Tremolo Mode", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>("tremolo_sync", "Tremolo Sync", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>("tremolo_div", "Tremolo Division", juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16" }, 3));

    addMix("ringmod");
    layout.add(std::make_unique<juce::AudioParameterFloat>("ringmod_freq", "RingMod Freq", 10.0f, 4000.0f, 200.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("ringmod_depth", "RingMod Depth", 0.0f, 1.0f, 0.5f));

    layout.add(std::make_unique<juce::AudioParameterBool>("noise_enabled", "Noise Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("noise_level", "Noise Level", 0.0f, 1.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterBool>("tone_enabled", "Tone Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tone_level", "Tone Level", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tone_freq", "Tone Freq", 20.0f, 20000.0f, 440.0f));

    layout.add(std::make_unique<juce::AudioParameterBool>("gen_to_chain", "Generators To Chain", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>("gen_mix", "Generator Mix", 0.0f, 1.0f, 1.0f));
}

void FxChain::addParameterIDs(juce::StringArray& ids)
{
    auto addMix = [&](const juce::String& id)
    {
        ids.add(id + "_mix");
        ids.add(id + "_enabled");
    };

    addMix("reverb");
    ids.add("reverb_algo");
    ids.add("reverb_size");
    ids.add("reverb_damping");
    ids.add("reverb_width");
    ids.add("reverb_freeze");

    addMix("delay");
    ids.add("delay_time1");
    ids.add("delay_time2");
    ids.add("delay_fb1");
    ids.add("delay_fb2");
    ids.add("delay_mode1");
    ids.add("delay_mode2");
    ids.add("delay_tape_tone");
    ids.add("delay_sync1");
    ids.add("delay_div1");
    ids.add("delay_sync2");
    ids.add("delay_div2");

    addMix("lpf");
    ids.add("lpf_cutoff");
    ids.add("lpf_slope");

    addMix("hpf");
    ids.add("hpf_cutoff");
    ids.add("hpf_slope");

    addMix("flanger");
    ids.add("flanger_rate");
    ids.add("flanger_depth");
    ids.add("flanger_feedback");

    addMix("phaser");
    ids.add("phaser_rate");
    ids.add("phaser_depth");
    ids.add("phaser_feedback");
    ids.add("phaser_center");

    addMix("bitcrusher");
    ids.add("bitcrusher_bits");
    ids.add("bitcrusher_downsample");

    addMix("distortion");
    ids.add("distortion_drive");
    ids.add("distortion_algo");

    addMix("eq");
    ids.add("eq_low_freq");
    ids.add("eq_mid_freq");
    ids.add("eq_mid2_freq");
    ids.add("eq_high_freq");
    ids.add("eq_low_gain");
    ids.add("eq_mid_gain");
    ids.add("eq_mid2_gain");
    ids.add("eq_high_gain");
    ids.add("eq_mid_q");
    ids.add("eq_mid2_q");

    addMix("tremolo");
    ids.add("tremolo_rate");
    ids.add("tremolo_depth");
    ids.add("tremolo_mode");
    ids.add("tremolo_sync");
    ids.add("tremolo_div");

    addMix("ringmod");
    ids.add("ringmod_freq");
    ids.add("ringmod_depth");

    ids.add("noise_enabled");
    ids.add("noise_level");
    ids.add("tone_enabled");
    ids.add("tone_level");
    ids.add("tone_freq");
    ids.add("gen_to_chain");
    ids.add("gen_mix");
}
