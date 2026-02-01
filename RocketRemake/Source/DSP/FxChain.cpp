#include "FxChain.h"

FxChain::FxChain(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    // Create all effect modules
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<ReverbModule>(apvts);
        entry->kind = ModuleKind::Effect;
        modules.add(entry.release());
    }
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<DelayModule>(apvts, 1);
        entry->kind = ModuleKind::Effect;
        modules.add(entry.release());
    }
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<DelayModule>(apvts, 2);
        entry->kind = ModuleKind::Effect;
        modules.add(entry.release());
    }
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<FilterModule>(apvts, FilterModule::Type::LowPass, "filter_lp");
        entry->kind = ModuleKind::Effect;
        modules.add(entry.release());
    }
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<FilterModule>(apvts, FilterModule::Type::HighPass, "filter_hp");
        entry->kind = ModuleKind::Effect;
        modules.add(entry.release());
    }
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<FlangerModule>(apvts);
        entry->kind = ModuleKind::Effect;
        modules.add(entry.release());
    }
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<PhaserModule>(apvts);
        entry->kind = ModuleKind::Effect;
        modules.add(entry.release());
    }
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<BitcrusherModule>(apvts);
        entry->kind = ModuleKind::Effect;
        modules.add(entry.release());
    }
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<DistortionModule>(apvts);
        entry->kind = ModuleKind::Effect;
        modules.add(entry.release());
    }
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<EQModule>(apvts, "eq");
        entry->kind = ModuleKind::Effect;
        modules.add(entry.release());
    }
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<TremoloModule>(apvts);
        entry->kind = ModuleKind::Effect;
        modules.add(entry.release());
    }
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<RingModModule>(apvts);
        entry->kind = ModuleKind::Effect;
        modules.add(entry.release());
    }

    // Create generator modules
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<NoiseGenModule>(apvts);
        entry->kind = ModuleKind::Generator;
        modules.add(entry.release());
    }
    {
        auto entry = std::make_unique<ModuleEntry>();
        entry->module = std::make_unique<ToneGenModule>(apvts);
        entry->kind = ModuleKind::Generator;
        modules.add(entry.release());
    }

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

FxChain::ModuleEntry* FxChain::findModuleById(const juce::String& id) const
{
    for (auto* entry : modules)
    {
        if (entry->module->getId() == id)
            return entry;
    }
    return nullptr;
}

void FxChain::prepare(const juce::dsp::ProcessSpec& spec)
{
    for (auto* entry : modules)
        entry->module->prepare(spec);

    genBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);
    dryGenBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);
}

void FxChain::reset()
{
    for (auto* entry : modules)
        entry->module->reset();
}

void FxChain::process(juce::AudioBuffer<float>& buffer,
                      juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& amount,
                      ModMatrix& modMatrix,
                      const FxTransportInfo& transport)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Set macro value for modulation
    modMatrix.setMacroValue(amount.getCurrentValue());

    // Process generators first (they add to the buffer)
    for (auto* entry : modules)
    {
        if (entry->kind == ModuleKind::Generator)
            entry->module->process(buffer, modMatrix, transport);
    }

    // Process effects in order
    for (const auto& id : order)
    {
        if (auto* entry = findModuleById(id))
        {
            if (entry->kind == ModuleKind::Effect)
                entry->module->process(buffer, modMatrix, transport);
        }
    }
}

void FxChain::moveModule(int fromIndex, int toIndex)
{
    if (!juce::isPositiveAndBelow(fromIndex, order.size()) ||
        !juce::isPositiveAndBelow(toIndex, order.size()) ||
        fromIndex == toIndex)
        return;

    auto id = order[fromIndex];
    order.remove(fromIndex);
    order.insert(toIndex, id);
}

juce::StringArray FxChain::getModuleOrder() const
{
    return order;
}

void FxChain::setModuleOrder(const juce::StringArray& newOrder)
{
    // Validate the new order
    juce::StringArray validOrder;
    for (const auto& id : newOrder)
    {
        if (findModuleById(id) != nullptr)
            validOrder.add(id);
    }

    // Add any missing effect modules
    for (auto* entry : modules)
    {
        if (entry->kind == ModuleKind::Effect && !validOrder.contains(entry->module->getId()))
            validOrder.add(entry->module->getId());
    }

    order = validOrder;
}

void FxChain::appendState(juce::ValueTree& parent) const
{
    juce::ValueTree chainTree("FXCHAIN");
    
    for (int i = 0; i < order.size(); ++i)
    {
        juce::ValueTree orderEntry("MODULE");
        orderEntry.setProperty("id", order[i], nullptr);
        orderEntry.setProperty("index", i, nullptr);
        chainTree.addChild(orderEntry, -1, nullptr);
    }

    parent.addChild(chainTree, -1, nullptr);
}

void FxChain::restoreFromState(const juce::ValueTree& parent)
{
    auto chainTree = parent.getChildWithName("FXCHAIN");
    if (!chainTree.isValid())
        return;

    juce::StringArray newOrder;
    for (int i = 0; i < chainTree.getNumChildren(); ++i)
    {
        auto entry = chainTree.getChild(i);
        if (entry.hasType("MODULE"))
        {
            auto id = entry.getProperty("id").toString();
            if (id.isNotEmpty())
                newOrder.add(id);
        }
    }

    if (newOrder.size() > 0)
        setModuleOrder(newOrder);
}

void FxChain::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    // Add parameters for all modules
    ReverbModule::addParameters(layout);
    DelayModule::addParameters(layout, 1);
    DelayModule::addParameters(layout, 2);
    FilterModule::addParameters(layout, "filter_lp", FilterModule::Type::LowPass);
    FilterModule::addParameters(layout, "filter_hp", FilterModule::Type::HighPass);
    FlangerModule::addParameters(layout);
    PhaserModule::addParameters(layout);
    BitcrusherModule::addParameters(layout);
    DistortionModule::addParameters(layout);
    EQModule::addParameters(layout, "eq");
    TremoloModule::addParameters(layout);
    RingModModule::addParameters(layout);
    NoiseGenModule::addParameters(layout);
    ToneGenModule::addParameters(layout);

    // Global mix parameter
    layout.add(std::make_unique<juce::AudioParameterFloat>("global_mix", "Global Mix", 0.0f, 1.0f, 1.0f));
}

void FxChain::addParameterIDs(juce::StringArray& ids)
{
    // This will be populated by the processor during initialization
    ids.add("amount");
    ids.add("global_mix");
    
    // Reverb
    ids.add("reverb_enabled");
    ids.add("reverb_mix");
    ids.add("reverb_decay");
    ids.add("reverb_predelay");
    ids.add("reverb_tone");
    ids.add("reverb_algorithm");
    
    // Delays
    for (int d = 1; d <= 2; ++d)
    {
        juce::String prefix = "delay" + juce::String(d) + "_";
        ids.add(prefix + "enabled");
        ids.add(prefix + "mix");
        ids.add(prefix + "time");
        ids.add(prefix + "feedback");
        ids.add(prefix + "sync");
        ids.add(prefix + "rhythm");
        ids.add(prefix + "hp");
        ids.add(prefix + "lp");
    }
    
    // Filters
    for (auto* filterId : { "filter_lp", "filter_hp" })
    {
        juce::String id(filterId);
        ids.add(id + "_enabled");
        ids.add(id + "_mix");
        ids.add(id + "_cutoff");
        ids.add(id + "_slope");
    }
    
    // Flanger
    ids.add("flanger_enabled");
    ids.add("flanger_mix");
    ids.add("flanger_rate");
    ids.add("flanger_depth");
    ids.add("flanger_feedback");
    
    // Phaser
    ids.add("phaser_enabled");
    ids.add("phaser_mix");
    ids.add("phaser_rate");
    ids.add("phaser_depth");
    ids.add("phaser_feedback");
    
    // Bitcrusher
    ids.add("bitcrush_enabled");
    ids.add("bitcrush_mix");
    ids.add("bitcrush_bits");
    ids.add("bitcrush_downsample");
    
    // Distortion
    ids.add("distortion_enabled");
    ids.add("distortion_mix");
    ids.add("distortion_drive");
    ids.add("distortion_algorithm");
    
    // EQ
    ids.add("eq_enabled");
    ids.add("eq_low_freq");
    ids.add("eq_low_gain");
    ids.add("eq_mid_freq");
    ids.add("eq_mid_gain");
    ids.add("eq_mid_q");
    ids.add("eq_midhi_freq");
    ids.add("eq_midhi_gain");
    ids.add("eq_midhi_q");
    ids.add("eq_high_freq");
    ids.add("eq_high_gain");
    
    // Tremolo
    ids.add("tremolo_enabled");
    ids.add("tremolo_mix");
    ids.add("tremolo_rate");
    ids.add("tremolo_depth");
    ids.add("tremolo_sync");
    ids.add("tremolo_rhythm");
    ids.add("tremolo_waveform");
    
    // Ring Mod
    ids.add("ringmod_enabled");
    ids.add("ringmod_mix");
    ids.add("ringmod_freq");
    
    // Noise Gen
    ids.add("noisegen_enabled");
    ids.add("noisegen_gain");
    ids.add("noisegen_lp");
    ids.add("noisegen_hp");
    
    // Tone Gen
    ids.add("tonegen_enabled");
    ids.add("tonegen_gain");
    ids.add("tonegen_freq");
    ids.add("tonegen_waveform");
}
