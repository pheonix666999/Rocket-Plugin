#include "PresetManager.h"

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& state, FxChain& chain, ModMatrix& mod)
    : apvts(state), fxChain(chain), modMatrix(mod)
{
    ensureFactoryPresets();
}

juce::File PresetManager::getPresetFolder() const
{
    auto folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                      .getChildFile("TheRocket")
                      .getChildFile("Presets");
    folder.createDirectory();
    return folder;
}

void PresetManager::savePreset(const juce::String& name)
{
    if (name.isEmpty())
        return;

    auto file = getPresetFolder().getChildFile(name + ".earcandy_preset");

    juce::ValueTree root("PRESET");
    root.setProperty("name", name, nullptr);
    root.setProperty("version", "1.0.0", nullptr);

    // Save parameter state
    auto paramState = apvts.copyState();
    root.addChild(paramState, -1, nullptr);

    // Save FX chain order
    fxChain.appendState(root);

    // Save modulation mappings
    modMatrix.appendState(root);

    // Write to file
    auto xml = root.createXml();
    if (xml)
        xml->writeTo(file);

    currentPresetName = name;
}

void PresetManager::loadPreset(const juce::String& name)
{
    if (name.isEmpty())
        return;

    auto file = getPresetFolder().getChildFile(name + ".earcandy_preset");
    if (!file.existsAsFile())
        return;

    auto xml = juce::XmlDocument::parse(file);
    if (!xml)
        return;

    auto root = juce::ValueTree::fromXml(*xml);
    if (!root.isValid() || !root.hasType("PRESET"))
        return;

    // Restore parameters
    auto paramState = root.getChildWithName(apvts.state.getType());
    if (paramState.isValid())
        apvts.replaceState(paramState);

    // Restore FX chain order
    fxChain.restoreFromState(root);

    // Restore modulation mappings
    modMatrix.restoreFromState(root);

    // Reset all modules for clean transition
    fxChain.reset();

    currentPresetName = name;
}

bool PresetManager::deletePreset(const juce::String& name)
{
    if (name.isEmpty())
        return false;

    auto file = getPresetFolder().getChildFile(name + ".earcandy_preset");
    if (file.existsAsFile())
        return file.deleteFile();

    return false;
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    auto folder = getPresetFolder();

    for (const auto& file : folder.findChildFiles(juce::File::findFiles, false, "*.earcandy_preset"))
    {
        names.add(file.getFileNameWithoutExtension());
    }

    names.sort(true);
    return names;
}

void PresetManager::appendState(juce::ValueTree& parent) const
{
    juce::ValueTree presetState("PRESET_STATE");
    presetState.setProperty("currentPreset", currentPresetName, nullptr);
    parent.addChild(presetState, -1, nullptr);
}

void PresetManager::restoreFromState(const juce::ValueTree& parent)
{
    auto presetState = parent.getChildWithName("PRESET_STATE");
    if (presetState.isValid())
    {
        currentPresetName = presetState.getProperty("currentPreset").toString();
        if (currentPresetName.isNotEmpty())
            loadPreset(currentPresetName);
    }
}

void PresetManager::ensureFactoryPresets()
{
    addFactoryPresetsIfMissing();
}

bool PresetManager::setParamValueInState(juce::ValueTree& state, const juce::String& paramID, float normalisedValue)
{
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild(i);
        if (child.hasProperty("id") && child.getProperty("id").toString() == paramID)
        {
            child.setProperty("value", normalisedValue, nullptr);
            return true;
        }
        if (setParamValueInState(child, paramID, normalisedValue))
            return true;
    }
    return false;
}

juce::ValueTree PresetManager::buildFactoryPresetState(const juce::String& name) const
{
    juce::ValueTree root("PRESET");
    root.setProperty("name", name, nullptr);
    root.setProperty("version", "1.0.0", nullptr);

    // Start with default parameter state
    auto paramState = apvts.copyState();

    // Configure default preset settings based on name
    if (name.containsIgnoreCase("Default"))
    {
        setParamValueInState(paramState, "reverb_mix", 0.2f);
        setParamValueInState(paramState, "delay1_mix", 0.15f);
    }
    else if (name.containsIgnoreCase("Vocal"))
    {
        setParamValueInState(paramState, "reverb_mix", 0.4f);
        setParamValueInState(paramState, "reverb_decay", 0.6f);
        setParamValueInState(paramState, "eq_mid_gain", 3.0f);
    }
    else if (name.containsIgnoreCase("Bass"))
    {
        setParamValueInState(paramState, "distortion_enabled", 1.0f);
        setParamValueInState(paramState, "distortion_mix", 0.3f);
        setParamValueInState(paramState, "eq_low_gain", 4.0f);
    }
    else if (name.containsIgnoreCase("Lead"))
    {
        setParamValueInState(paramState, "delay1_mix", 0.25f);
        setParamValueInState(paramState, "reverb_mix", 0.35f);
        setParamValueInState(paramState, "eq_high_gain", 2.0f);
    }
    else if (name.containsIgnoreCase("Riser"))
    {
        setParamValueInState(paramState, "noisegen_enabled", 1.0f);
        setParamValueInState(paramState, "filter_hp_cutoff", 20.0f);
        setParamValueInState(paramState, "reverb_mix", 0.5f);
    }

    root.addChild(paramState, -1, nullptr);

    // Add default FX chain order
    juce::ValueTree chainTree("FXCHAIN");
    juce::StringArray defaultOrder = {
        "filter_hp", "eq", "distortion", "phaser", "flanger",
        "delay1", "delay2", "reverb", "tremolo", "ringmod",
        "bitcrush", "filter_lp"
    };
    for (int i = 0; i < defaultOrder.size(); ++i)
    {
        juce::ValueTree orderEntry("MODULE");
        orderEntry.setProperty("id", defaultOrder[i], nullptr);
        orderEntry.setProperty("index", i, nullptr);
        chainTree.addChild(orderEntry, -1, nullptr);
    }
    root.addChild(chainTree, -1, nullptr);

    // Add default modulation mappings
    juce::ValueTree modTree("MODMATRIX");
    
    // Default: global mix follows amount
    {
        juce::ValueTree asg("ASSIGNMENT");
        asg.setProperty("paramID", "global_mix", nullptr);
        asg.setProperty("amount", 1.0f, nullptr);
        asg.setProperty("useRange", true, nullptr);
        asg.setProperty("min", 0.0f, nullptr);
        asg.setProperty("max", 1.0f, nullptr);
        modTree.addChild(asg, -1, nullptr);
    }
    
    root.addChild(modTree, -1, nullptr);

    return root;
}

void PresetManager::writePresetFileIfMissing(const juce::String& name, const juce::ValueTree& state)
{
    auto file = getPresetFolder().getChildFile(name + ".earcandy_preset");
    if (file.existsAsFile())
        return;

    auto xml = state.createXml();
    if (xml)
        xml->writeTo(file);
}

void PresetManager::addFactoryPresetsIfMissing()
{
    // Create default preset
    auto defaultState = buildFactoryPresetState("Default");
    writePresetFileIfMissing("Default", defaultState);

    // Create a few basic presets
    juce::StringArray factoryNames = {
        "Vocal Clean",
        "Vocal Space",
        "Bass Power",
        "Lead Crisp",
        "Deep Reverb",
        "Lo-Fi",
        "Riser Energy"
    };

    for (const auto& name : factoryNames)
    {
        auto state = buildFactoryPresetState(name);
        writePresetFileIfMissing(name, state);
    }
}
