#include "PresetManager.h"

namespace
{
    struct ModAssignDef
    {
        const char* paramID;
        float amount; // -1..1
        bool useRange;
        float min;
        float max;
    };

    struct FactoryPresetDef
    {
        const char* name;
        const char* const* order; // nullptr terminated module ids
        const char* const* paramPairs; // nullptr terminated: id, valueString, id, valueString...
        const ModAssignDef* assigns; // array terminated by paramID==nullptr
    };

    static void addFxChainState(juce::ValueTree& parent,
                                const juce::StringArray& order,
                                bool genToChain,
                                float genMix)
    {
        parent.removeChild(parent.getChildWithName("FX_CHAIN"), nullptr);
        juce::ValueTree chain("FX_CHAIN");
        chain.setProperty("gen_to_chain", genToChain, nullptr);
        chain.setProperty("gen_mix", genMix, nullptr);
        for (const auto& id : order)
        {
            juce::ValueTree child("MODULE");
            child.setProperty("id", id, nullptr);
            chain.addChild(child, -1, nullptr);
        }
        parent.addChild(chain, -1, nullptr);
    }

    static void addModMatrixState(juce::ValueTree& parent, const ModAssignDef* assigns)
    {
        parent.removeChild(parent.getChildWithName("MOD_MATRIX"), nullptr);
        juce::ValueTree modTree("MOD_MATRIX");

        if (assigns != nullptr)
        {
            for (int i = 0; assigns[i].paramID != nullptr; ++i)
            {
                const auto& a = assigns[i];
                juce::ValueTree child("ASSIGN");
                child.setProperty("paramID", a.paramID, nullptr);
                child.setProperty("amount", a.amount, nullptr);
                child.setProperty("useRange", a.useRange, nullptr);
                child.setProperty("min", a.min, nullptr);
                child.setProperty("max", a.max, nullptr);
                modTree.addChild(child, -1, nullptr);
            }
        }

        parent.addChild(modTree, -1, nullptr);
    }

    static juce::StringArray orderFromNullTerminated(const char* const* ids)
    {
        static const juce::StringArray allowed {
            "reverb", "delay", "lpf", "hpf", "flanger", "phaser", "bitcrusher", "distortion", "eq", "tremolo", "ringmod"
        };

        juce::StringArray out;
        if (ids == nullptr)
            return out;
        for (int i = 0; ids[i] != nullptr; ++i)
            if (allowed.contains(juce::String(ids[i])))
                out.add(ids[i]);
        return out;
    }

    static bool parseFloat(const char* s, float& out)
    {
        if (s == nullptr)
            return false;
        out = juce::String(s).getFloatValue();
        return true;
    }

    // Module order presets (effect-only ids, matching FxChain::getId()).
    static const char* const kOrder_Default[] = { "lpf", "distortion", "delay", "reverb", nullptr };
    static const char* const kOrder_GateBuild[] = { "tremolo", "hpf", "distortion", "reverb", nullptr };
    static const char* const kOrder_BitRise[] = { "bitcrusher", "distortion", "lpf", "reverb", nullptr };
    static const char* const kOrder_SweepSpace[] = { "hpf", "lpf", "delay", "reverb", nullptr };
    static const char* const kOrder_RingSciFi[] = { "ringmod", "phaser", "delay", "reverb", nullptr };
    static const char* const kOrder_FlangeLift[] = { "flanger", "hpf", "delay", "reverb", nullptr };
    static const char* const kOrder_WideWash[] = { "delay", "reverb", "eq", nullptr };
    static const char* const kOrder_DistortDrive[] = { "distortion", "hpf", "delay", "reverb", nullptr };
    static const char* const kOrder_PhaserSweep[] = { "phaser", "lpf", "delay", "reverb", nullptr };
    static const char* const kOrder_StutterGate[] = { "tremolo", "hpf", "delay", "reverb", nullptr };

    // Parameter helper lists: id/value pairs, null terminated.
    static const char* const kParams_CommonClean[] = {
        "global_mix", "1.0",

        "reverb_enabled", "1", "reverb_mix", "0.45", "reverb_size", "0.55", "reverb_damping", "0.35", "reverb_width", "0.90", "reverb_freeze", "0",
        "delay_enabled", "1", "delay_mix", "0.35", "delay_time1", "220", "delay_time2", "340", "delay_fb1", "0.25", "delay_fb2", "0.28",

        "lpf_enabled", "1", "lpf_mix", "1.0", "lpf_cutoff", "14000", "lpf_slope", "2",
        "hpf_enabled", "1", "hpf_mix", "1.0", "hpf_cutoff", "40", "hpf_slope", "1",

        "flanger_enabled", "0", "flanger_mix", "0.0", "flanger_rate", "0.30", "flanger_depth", "0.45", "flanger_feedback", "0.10",
        "phaser_enabled", "0", "phaser_mix", "0.0", "phaser_rate", "0.25", "phaser_depth", "0.50", "phaser_feedback", "0.10", "phaser_center", "400",
        "bitcrusher_enabled", "0", "bitcrusher_mix", "0.0", "bitcrusher_bits", "10", "bitcrusher_downsample", "1",
        "distortion_enabled", "0", "distortion_mix", "0.0", "distortion_drive", "0.25", "distortion_algo", "0.0",
        "eq_enabled", "0", "eq_mix", "1.0",
        "tremolo_enabled", "0", "tremolo_mix", "1.0", "tremolo_rate", "8.0", "tremolo_depth", "0.75", "tremolo_mode", "0.0",
        "ringmod_enabled", "0", "ringmod_mix", "0.0", "ringmod_freq", "180", "ringmod_depth", "0.65",

        "noise_enabled", "0", "noise_level", "0.0",
        "tone_enabled", "0", "tone_level", "0.0", "tone_freq", "440",
        "gen_to_chain", "1", "gen_mix", "1.0",
        nullptr
    };

    static const ModAssignDef kAssign_CleanLift[] = {
        { "global_mix", 1.0f, true, 0.0f, 1.0f },
        { "lpf_cutoff", -0.85f, true, 250.0f, 16000.0f },
        { "reverb_mix", 0.65f, true, 0.05f, 0.55f },
        { "delay_mix", 0.55f, true, 0.0f, 0.40f },
        { nullptr, 0.0f, false, 0.0f, 0.0f }
    };

    static const char* const kParams_NoiseSweep[] = {
        "global_mix", "1.0",

        "reverb_enabled", "1", "reverb_mix", "0.55", "reverb_size", "0.70", "reverb_damping", "0.25", "reverb_width", "1.00", "reverb_freeze", "0",
        "delay_enabled", "1", "delay_mix", "0.25", "delay_time1", "180", "delay_time2", "260", "delay_fb1", "0.35", "delay_fb2", "0.32",

        "lpf_enabled", "0", "lpf_mix", "1.0", "lpf_cutoff", "16000", "lpf_slope", "2",
        "hpf_enabled", "1", "hpf_mix", "1.0", "hpf_cutoff", "120", "hpf_slope", "2",

        "flanger_enabled", "0", "flanger_mix", "0.0", "flanger_rate", "0.25", "flanger_depth", "0.55", "flanger_feedback", "0.10",
        "phaser_enabled", "0", "phaser_mix", "0.0", "phaser_rate", "0.25", "phaser_depth", "0.55", "phaser_feedback", "0.10", "phaser_center", "500",
        "bitcrusher_enabled", "0", "bitcrusher_mix", "0.0", "bitcrusher_bits", "10", "bitcrusher_downsample", "1",
        "distortion_enabled", "0", "distortion_mix", "0.0", "distortion_drive", "0.30", "distortion_algo", "0.0",
        "eq_enabled", "0", "eq_mix", "1.0",
        "tremolo_enabled", "0", "tremolo_mix", "1.0", "tremolo_rate", "8.0", "tremolo_depth", "0.75", "tremolo_mode", "0.0",
        "ringmod_enabled", "0", "ringmod_mix", "0.0", "ringmod_freq", "180", "ringmod_depth", "0.65",

        "noise_enabled", "1", "noise_level", "0.0",
        "tone_enabled", "0", "tone_level", "0.0", "tone_freq", "440",
        "gen_to_chain", "1", "gen_mix", "0.75",
        nullptr
    };

    static const ModAssignDef kAssign_NoiseSweep[] = {
        { "global_mix", 1.0f, true, 0.0f, 1.0f },
        { "noise_level", 1.0f, true, 0.0f, 0.65f },
        { "hpf_cutoff", 0.9f, true, 120.0f, 6000.0f },
        { "reverb_size", 0.75f, true, 0.35f, 0.85f },
        { nullptr, 0.0f, false, 0.0f, 0.0f }
    };

    static const char* const kParams_ToneRiser[] = {
        "global_mix", "1.0",
        "reverb_enabled", "1", "reverb_mix", "0.45", "reverb_size", "0.60", "reverb_damping", "0.30", "reverb_width", "0.95", "reverb_freeze", "0",
        "delay_enabled", "1", "delay_mix", "0.20", "delay_time1", "110", "delay_time2", "160", "delay_fb1", "0.25", "delay_fb2", "0.22",

        "lpf_enabled", "1", "lpf_mix", "1.0", "lpf_cutoff", "16000", "lpf_slope", "2",
        "hpf_enabled", "0", "hpf_mix", "1.0", "hpf_cutoff", "60", "hpf_slope", "2",

        "flanger_enabled", "0", "flanger_mix", "0.0", "flanger_rate", "0.20", "flanger_depth", "0.55", "flanger_feedback", "0.10",
        "phaser_enabled", "0", "phaser_mix", "0.0", "phaser_rate", "0.25", "phaser_depth", "0.55", "phaser_feedback", "0.10", "phaser_center", "500",
        "bitcrusher_enabled", "0", "bitcrusher_mix", "0.0", "bitcrusher_bits", "10", "bitcrusher_downsample", "1",
        "distortion_enabled", "0", "distortion_mix", "0.0", "distortion_drive", "0.30", "distortion_algo", "0.0",
        "eq_enabled", "0", "eq_mix", "1.0",
        "tremolo_enabled", "0", "tremolo_mix", "1.0", "tremolo_rate", "8.0", "tremolo_depth", "0.75", "tremolo_mode", "0.0",
        "ringmod_enabled", "0", "ringmod_mix", "0.0", "ringmod_freq", "180", "ringmod_depth", "0.65",

        "noise_enabled", "0", "noise_level", "0.0",
        "tone_enabled", "1", "tone_level", "0.0", "tone_freq", "120",
        "gen_to_chain", "1", "gen_mix", "0.65",
        nullptr
    };

    static const ModAssignDef kAssign_ToneRiser[] = {
        { "global_mix", 1.0f, true, 0.0f, 1.0f },
        { "tone_level", 1.0f, true, 0.0f, 0.55f },
        { "tone_freq", 1.0f, true, 120.0f, 4200.0f },
        { "lpf_cutoff", -0.9f, true, 250.0f, 16000.0f },
        { nullptr, 0.0f, false, 0.0f, 0.0f }
    };

    static const char* const kParams_GateBuild[] = {
        "global_mix", "1.0",
        "reverb_enabled", "1", "reverb_mix", "0.25", "reverb_size", "0.45", "reverb_damping", "0.45", "reverb_width", "0.90", "reverb_freeze", "0",
        "delay_enabled", "0", "delay_mix", "0.0", "delay_time1", "220", "delay_time2", "340", "delay_fb1", "0.25", "delay_fb2", "0.28",

        "lpf_enabled", "0", "lpf_mix", "1.0", "lpf_cutoff", "16000", "lpf_slope", "2",
        "hpf_enabled", "1", "hpf_mix", "1.0", "hpf_cutoff", "90", "hpf_slope", "2",

        "flanger_enabled", "0", "flanger_mix", "0.0", "flanger_rate", "0.25", "flanger_depth", "0.55", "flanger_feedback", "0.10",
        "phaser_enabled", "0", "phaser_mix", "0.0", "phaser_rate", "0.25", "phaser_depth", "0.55", "phaser_feedback", "0.10", "phaser_center", "500",
        "bitcrusher_enabled", "0", "bitcrusher_mix", "0.0", "bitcrusher_bits", "10", "bitcrusher_downsample", "1",
        "distortion_enabled", "1", "distortion_mix", "0.35", "distortion_drive", "0.40", "distortion_algo", "0.0",
        "eq_enabled", "0", "eq_mix", "1.0",
        "tremolo_enabled", "1", "tremolo_mix", "1.0", "tremolo_rate", "8.0", "tremolo_depth", "0.85", "tremolo_mode", "0.85",
        "ringmod_enabled", "0", "ringmod_mix", "0.0", "ringmod_freq", "180", "ringmod_depth", "0.65",

        "noise_enabled", "0", "noise_level", "0.0",
        "tone_enabled", "0", "tone_level", "0.0", "tone_freq", "440",
        "gen_to_chain", "1", "gen_mix", "1.0",
        nullptr
    };

    static const ModAssignDef kAssign_GateBuild[] = {
        { "global_mix", 1.0f, true, 0.0f, 1.0f },
        { "distortion_drive", 0.9f, true, 0.15f, 0.95f },
        { "distortion_mix", 0.8f, true, 0.10f, 0.65f },
        { "tremolo_depth", 0.8f, true, 0.15f, 0.90f },
        { "reverb_mix", 0.5f, true, 0.05f, 0.35f },
        { nullptr, 0.0f, false, 0.0f, 0.0f }
    };

    static const char* const kParams_BitRise[] = {
        "global_mix", "1.0",
        "reverb_enabled", "1", "reverb_mix", "0.20", "reverb_size", "0.55", "reverb_damping", "0.40", "reverb_width", "0.92", "reverb_freeze", "0",
        "delay_enabled", "0", "delay_mix", "0.0", "delay_time1", "220", "delay_time2", "340", "delay_fb1", "0.25", "delay_fb2", "0.28",

        "lpf_enabled", "1", "lpf_mix", "1.0", "lpf_cutoff", "16000", "lpf_slope", "2",
        "hpf_enabled", "0", "hpf_mix", "1.0", "hpf_cutoff", "60", "hpf_slope", "2",

        "flanger_enabled", "0", "flanger_mix", "0.0", "flanger_rate", "0.25", "flanger_depth", "0.55", "flanger_feedback", "0.10",
        "phaser_enabled", "0", "phaser_mix", "0.0", "phaser_rate", "0.25", "phaser_depth", "0.55", "phaser_feedback", "0.10", "phaser_center", "500",
        "bitcrusher_enabled", "1", "bitcrusher_mix", "0.20", "bitcrusher_bits", "10.0", "bitcrusher_downsample", "1.0",
        "distortion_enabled", "1", "distortion_mix", "0.15", "distortion_drive", "0.25", "distortion_algo", "1.0",
        "eq_enabled", "0", "eq_mix", "1.0",
        "tremolo_enabled", "0", "tremolo_mix", "1.0", "tremolo_rate", "8.0", "tremolo_depth", "0.75", "tremolo_mode", "0.0",
        "ringmod_enabled", "0", "ringmod_mix", "0.0", "ringmod_freq", "180", "ringmod_depth", "0.65",

        "noise_enabled", "0", "noise_level", "0.0",
        "tone_enabled", "0", "tone_level", "0.0", "tone_freq", "440",
        "gen_to_chain", "1", "gen_mix", "1.0",
        nullptr
    };

    static const ModAssignDef kAssign_BitRise[] = {
        { "global_mix", 1.0f, true, 0.0f, 1.0f },
        { "bitcrusher_mix", 0.8f, true, 0.0f, 0.75f },
        { "bitcrusher_bits", -0.9f, true, 16.0f, 4.0f },
        { "bitcrusher_downsample", 0.85f, true, 1.0f, 10.0f },
        { "lpf_cutoff", -0.8f, true, 350.0f, 16000.0f },
        { nullptr, 0.0f, false, 0.0f, 0.0f }
    };

    static const char* const kParams_RingSciFi[] = {
        "global_mix", "1.0",
        "reverb_enabled", "1", "reverb_mix", "0.25", "reverb_size", "0.55", "reverb_damping", "0.35", "reverb_width", "0.95", "reverb_freeze", "0",
        "delay_enabled", "1", "delay_mix", "0.20", "delay_time1", "250", "delay_time2", "380", "delay_fb1", "0.35", "delay_fb2", "0.28",

        "lpf_enabled", "0", "lpf_mix", "1.0", "lpf_cutoff", "16000", "lpf_slope", "2",
        "hpf_enabled", "0", "hpf_mix", "1.0", "hpf_cutoff", "60", "hpf_slope", "2",

        "flanger_enabled", "0", "flanger_mix", "0.0", "flanger_rate", "0.25", "flanger_depth", "0.55", "flanger_feedback", "0.10",
        "phaser_enabled", "1", "phaser_mix", "0.25", "phaser_rate", "0.25", "phaser_depth", "0.65", "phaser_feedback", "0.15", "phaser_center", "500",
        "bitcrusher_enabled", "0", "bitcrusher_mix", "0.0", "bitcrusher_bits", "10", "bitcrusher_downsample", "1",
        "distortion_enabled", "0", "distortion_mix", "0.0", "distortion_drive", "0.30", "distortion_algo", "0.0",
        "eq_enabled", "0", "eq_mix", "1.0",
        "tremolo_enabled", "0", "tremolo_mix", "1.0", "tremolo_rate", "8.0", "tremolo_depth", "0.75", "tremolo_mode", "0.0",
        "ringmod_enabled", "1", "ringmod_mix", "0.25", "ringmod_freq", "180.0", "ringmod_depth", "0.65",

        "noise_enabled", "0", "noise_level", "0.0",
        "tone_enabled", "0", "tone_level", "0.0", "tone_freq", "440",
        "gen_to_chain", "1", "gen_mix", "1.0",
        nullptr
    };

    static const ModAssignDef kAssign_RingSciFi[] = {
        { "global_mix", 1.0f, true, 0.0f, 1.0f },
        { "ringmod_mix", 0.7f, true, 0.0f, 0.70f },
        { "ringmod_freq", 0.9f, true, 60.0f, 1200.0f },
        { "delay_mix", 0.6f, true, 0.0f, 0.35f },
        { nullptr, 0.0f, false, 0.0f, 0.0f }
    };

    static const char* const kParams_WideWash[] = {
        "global_mix", "1.0",

        "reverb_enabled", "1", "reverb_mix", "0.65", "reverb_size", "0.78", "reverb_damping", "0.25", "reverb_width", "1.00", "reverb_freeze", "0",
        "delay_enabled", "1", "delay_mix", "0.45", "delay_time1", "320", "delay_time2", "520", "delay_fb1", "0.38", "delay_fb2", "0.34",

        "lpf_enabled", "0", "lpf_mix", "1.0", "lpf_cutoff", "16000", "lpf_slope", "2",
        "hpf_enabled", "0", "hpf_mix", "1.0", "hpf_cutoff", "60", "hpf_slope", "2",

        "flanger_enabled", "0", "flanger_mix", "0.0", "flanger_rate", "0.20", "flanger_depth", "0.55", "flanger_feedback", "0.10",
        "phaser_enabled", "0", "phaser_mix", "0.0", "phaser_rate", "0.25", "phaser_depth", "0.55", "phaser_feedback", "0.10", "phaser_center", "500",
        "bitcrusher_enabled", "0", "bitcrusher_mix", "0.0", "bitcrusher_bits", "10", "bitcrusher_downsample", "1",
        "distortion_enabled", "0", "distortion_mix", "0.0", "distortion_drive", "0.25", "distortion_algo", "0.0",
        "eq_enabled", "1", "eq_mix", "1.0",
        "eq_low_freq", "120", "eq_mid_freq", "800", "eq_mid2_freq", "2400", "eq_high_freq", "9000",
        "eq_low_gain", "-2.0", "eq_mid_gain", "0.0", "eq_mid2_gain", "1.5", "eq_high_gain", "2.0",
        "eq_mid_q", "0.7", "eq_mid2_q", "0.7",

        "tremolo_enabled", "0", "tremolo_mix", "1.0", "tremolo_rate", "8.0", "tremolo_depth", "0.75", "tremolo_mode", "0.0",
        "ringmod_enabled", "0", "ringmod_mix", "0.0", "ringmod_freq", "180", "ringmod_depth", "0.65",

        "noise_enabled", "0", "noise_level", "0.0",
        "tone_enabled", "0", "tone_level", "0.0", "tone_freq", "440",
        "gen_to_chain", "1", "gen_mix", "1.0",
        nullptr
    };

    static const ModAssignDef kAssign_WideWash[] = {
        { "global_mix", 1.0f, true, 0.0f, 1.0f },
        { "reverb_mix", 0.9f, true, 0.10f, 0.75f },
        { "reverb_size", 0.9f, true, 0.45f, 0.92f },
        { "delay_mix", 0.7f, true, 0.05f, 0.55f },
        { nullptr, 0.0f, false, 0.0f, 0.0f }
    };

    static const char* const kParams_FlangeLift[] = {
        "global_mix", "1.0",

        "reverb_enabled", "1", "reverb_mix", "0.35", "reverb_size", "0.58", "reverb_damping", "0.32", "reverb_width", "0.95", "reverb_freeze", "0",
        "delay_enabled", "1", "delay_mix", "0.25", "delay_time1", "170", "delay_time2", "260", "delay_fb1", "0.22", "delay_fb2", "0.18",

        "lpf_enabled", "0", "lpf_mix", "1.0", "lpf_cutoff", "16000", "lpf_slope", "2",
        "hpf_enabled", "1", "hpf_mix", "1.0", "hpf_cutoff", "80", "hpf_slope", "2",

        "flanger_enabled", "1", "flanger_mix", "0.28", "flanger_rate", "0.22", "flanger_depth", "0.70", "flanger_feedback", "0.20",
        "phaser_enabled", "0", "phaser_mix", "0.0", "phaser_rate", "0.25", "phaser_depth", "0.55", "phaser_feedback", "0.10", "phaser_center", "500",
        "bitcrusher_enabled", "0", "bitcrusher_mix", "0.0", "bitcrusher_bits", "10", "bitcrusher_downsample", "1",
        "distortion_enabled", "0", "distortion_mix", "0.0", "distortion_drive", "0.25", "distortion_algo", "0.0",
        "eq_enabled", "0", "eq_mix", "1.0",
        "tremolo_enabled", "0", "tremolo_mix", "1.0", "tremolo_rate", "8.0", "tremolo_depth", "0.75", "tremolo_mode", "0.0",
        "ringmod_enabled", "0", "ringmod_mix", "0.0", "ringmod_freq", "180", "ringmod_depth", "0.65",

        "noise_enabled", "0", "noise_level", "0.0",
        "tone_enabled", "0", "tone_level", "0.0", "tone_freq", "440",
        "gen_to_chain", "1", "gen_mix", "1.0",
        nullptr
    };

    static const ModAssignDef kAssign_FlangeLift[] = {
        { "global_mix", 1.0f, true, 0.0f, 1.0f },
        { "hpf_cutoff", 0.85f, true, 80.0f, 4200.0f },
        { "flanger_mix", 0.8f, true, 0.0f, 0.65f },
        { "flanger_depth", 0.8f, true, 0.25f, 0.95f },
        { nullptr, 0.0f, false, 0.0f, 0.0f }
    };

    static const char* const kParams_DistortDrive[] = {
        "global_mix", "1.0",

        "reverb_enabled", "1", "reverb_mix", "0.20", "reverb_size", "0.50", "reverb_damping", "0.40", "reverb_width", "0.90", "reverb_freeze", "0",
        "delay_enabled", "1", "delay_mix", "0.18", "delay_time1", "140", "delay_time2", "220", "delay_fb1", "0.25", "delay_fb2", "0.18",

        "lpf_enabled", "0", "lpf_mix", "1.0", "lpf_cutoff", "16000", "lpf_slope", "2",
        "hpf_enabled", "1", "hpf_mix", "1.0", "hpf_cutoff", "60", "hpf_slope", "2",

        "flanger_enabled", "0", "flanger_mix", "0.0", "flanger_rate", "0.20", "flanger_depth", "0.55", "flanger_feedback", "0.10",
        "phaser_enabled", "0", "phaser_mix", "0.0", "phaser_rate", "0.25", "phaser_depth", "0.55", "phaser_feedback", "0.10", "phaser_center", "500",
        "bitcrusher_enabled", "0", "bitcrusher_mix", "0.0", "bitcrusher_bits", "10", "bitcrusher_downsample", "1",
        "distortion_enabled", "1", "distortion_mix", "0.22", "distortion_drive", "0.35", "distortion_algo", "1.0",
        "eq_enabled", "0", "eq_mix", "1.0",
        "tremolo_enabled", "0", "tremolo_mix", "1.0", "tremolo_rate", "8.0", "tremolo_depth", "0.75", "tremolo_mode", "0.0",
        "ringmod_enabled", "0", "ringmod_mix", "0.0", "ringmod_freq", "180", "ringmod_depth", "0.65",

        "noise_enabled", "0", "noise_level", "0.0",
        "tone_enabled", "0", "tone_level", "0.0", "tone_freq", "440",
        "gen_to_chain", "1", "gen_mix", "1.0",
        nullptr
    };

    static const ModAssignDef kAssign_DistortDrive[] = {
        { "global_mix", 1.0f, true, 0.0f, 1.0f },
        { "distortion_drive", 1.0f, true, 0.10f, 0.98f },
        { "distortion_mix", 0.9f, true, 0.05f, 0.85f },
        { "hpf_cutoff", 0.8f, true, 60.0f, 3800.0f },
        { nullptr, 0.0f, false, 0.0f, 0.0f }
    };

    static const char* const kParams_PhaserSweep[] = {
        "global_mix", "1.0",

        "reverb_enabled", "1", "reverb_mix", "0.30", "reverb_size", "0.58", "reverb_damping", "0.32", "reverb_width", "0.95", "reverb_freeze", "0",
        "delay_enabled", "1", "delay_mix", "0.20", "delay_time1", "210", "delay_time2", "290", "delay_fb1", "0.28", "delay_fb2", "0.22",

        "lpf_enabled", "1", "lpf_mix", "1.0", "lpf_cutoff", "16000", "lpf_slope", "2",
        "hpf_enabled", "0", "hpf_mix", "1.0", "hpf_cutoff", "60", "hpf_slope", "2",

        "flanger_enabled", "0", "flanger_mix", "0.0", "flanger_rate", "0.20", "flanger_depth", "0.55", "flanger_feedback", "0.10",
        "phaser_enabled", "1", "phaser_mix", "0.30", "phaser_rate", "0.20", "phaser_depth", "0.75", "phaser_feedback", "0.20", "phaser_center", "650",
        "bitcrusher_enabled", "0", "bitcrusher_mix", "0.0", "bitcrusher_bits", "10", "bitcrusher_downsample", "1",
        "distortion_enabled", "0", "distortion_mix", "0.0", "distortion_drive", "0.25", "distortion_algo", "0.0",
        "eq_enabled", "0", "eq_mix", "1.0",
        "tremolo_enabled", "0", "tremolo_mix", "1.0", "tremolo_rate", "8.0", "tremolo_depth", "0.75", "tremolo_mode", "0.0",
        "ringmod_enabled", "0", "ringmod_mix", "0.0", "ringmod_freq", "180", "ringmod_depth", "0.65",

        "noise_enabled", "0", "noise_level", "0.0",
        "tone_enabled", "0", "tone_level", "0.0", "tone_freq", "440",
        "gen_to_chain", "1", "gen_mix", "1.0",
        nullptr
    };

    static const ModAssignDef kAssign_PhaserSweep[] = {
        { "global_mix", 1.0f, true, 0.0f, 1.0f },
        { "phaser_mix", 0.8f, true, 0.0f, 0.75f },
        { "phaser_center", 0.9f, true, 250.0f, 1800.0f },
        { "lpf_cutoff", -0.85f, true, 300.0f, 16000.0f },
        { nullptr, 0.0f, false, 0.0f, 0.0f }
    };

    static const char* const kParams_StutterGate[] = {
        "global_mix", "1.0",

        "reverb_enabled", "1", "reverb_mix", "0.22", "reverb_size", "0.45", "reverb_damping", "0.45", "reverb_width", "0.90", "reverb_freeze", "0",
        "delay_enabled", "1", "delay_mix", "0.18", "delay_time1", "120", "delay_time2", "180", "delay_fb1", "0.22", "delay_fb2", "0.18",

        "lpf_enabled", "0", "lpf_mix", "1.0", "lpf_cutoff", "16000", "lpf_slope", "2",
        "hpf_enabled", "1", "hpf_mix", "1.0", "hpf_cutoff", "110", "hpf_slope", "2",

        "flanger_enabled", "0", "flanger_mix", "0.0", "flanger_rate", "0.20", "flanger_depth", "0.55", "flanger_feedback", "0.10",
        "phaser_enabled", "0", "phaser_mix", "0.0", "phaser_rate", "0.25", "phaser_depth", "0.55", "phaser_feedback", "0.10", "phaser_center", "500",
        "bitcrusher_enabled", "0", "bitcrusher_mix", "0.0", "bitcrusher_bits", "10", "bitcrusher_downsample", "1",
        "distortion_enabled", "0", "distortion_mix", "0.0", "distortion_drive", "0.25", "distortion_algo", "0.0",
        "eq_enabled", "0", "eq_mix", "1.0",
        "tremolo_enabled", "1", "tremolo_mix", "1.0", "tremolo_rate", "12.0", "tremolo_depth", "0.90", "tremolo_mode", "1.0",
        "ringmod_enabled", "0", "ringmod_mix", "0.0", "ringmod_freq", "180", "ringmod_depth", "0.65",

        "noise_enabled", "0", "noise_level", "0.0",
        "tone_enabled", "0", "tone_level", "0.0", "tone_freq", "440",
        "gen_to_chain", "1", "gen_mix", "1.0",
        nullptr
    };

    static const ModAssignDef kAssign_StutterGate[] = {
        { "global_mix", 1.0f, true, 0.0f, 1.0f },
        { "tremolo_depth", 1.0f, true, 0.10f, 0.95f },
        { "tremolo_rate", 0.9f, true, 6.0f, 16.0f },
        { "hpf_cutoff", 0.8f, true, 110.0f, 5000.0f },
        { nullptr, 0.0f, false, 0.0f, 0.0f }
    };

    static const ModAssignDef kAssign_InvertedMix[] = {
        { "global_mix", -1.0f, true, 0.0f, 1.0f },
        { "lpf_cutoff", -0.85f, true, 250.0f, 16000.0f },
        { nullptr, 0.0f, false, 0.0f, 0.0f }
    };

    static const FactoryPresetDef kFactoryPresets[] = {
        { "Clean Lift (Init)", kOrder_Default, kParams_CommonClean, kAssign_CleanLift },
        { "Noise Sweep Up", kOrder_SweepSpace, kParams_NoiseSweep, kAssign_NoiseSweep },
        { "Tone Riser", kOrder_SweepSpace, kParams_ToneRiser, kAssign_ToneRiser },
        { "Gate Build", kOrder_GateBuild, kParams_GateBuild, kAssign_GateBuild },
        { "Digital Bit Rise", kOrder_BitRise, kParams_BitRise, kAssign_BitRise },
        { "Sci-Fi Ring Lift", kOrder_RingSciFi, kParams_RingSciFi, kAssign_RingSciFi },
        { "Wide Space Wash", kOrder_WideWash, kParams_WideWash, kAssign_WideWash },
        { "Flange Lift", kOrder_FlangeLift, kParams_FlangeLift, kAssign_FlangeLift },
        { "Distort Drive", kOrder_DistortDrive, kParams_DistortDrive, kAssign_DistortDrive },
        { "Phaser Sweep", kOrder_PhaserSweep, kParams_PhaserSweep, kAssign_PhaserSweep },
        { "Stutter Gate", kOrder_StutterGate, kParams_StutterGate, kAssign_StutterGate },
        { "Inverted (Dry to Wet)", kOrder_SweepSpace, kParams_CommonClean, kAssign_InvertedMix },
    };

    static bool findPlainParamValue(const char* const* paramPairs, const juce::String& id, float& out)
    {
        if (paramPairs == nullptr)
            return false;
        for (int i = 0; paramPairs[i] != nullptr && paramPairs[i + 1] != nullptr; i += 2)
        {
            if (juce::String(paramPairs[i]) == id)
                return parseFloat(paramPairs[i + 1], out);
        }
        return false;
    }
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& state, FxChain& chain, ModMatrix& mod)
    : apvts(state), fxChain(chain), modMatrix(mod)
{
    ensureFactoryPresets();
}

juce::File PresetManager::getPresetFolder() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("TheRocket").getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

bool PresetManager::setParamValueInState(juce::ValueTree& state, const juce::String& paramID, float normalisedValue)
{
    bool updated = false;

    if (state.hasProperty(paramID))
    {
        state.setProperty(paramID, normalisedValue, nullptr);
        return true;
    }

    const auto tryNode = [&](juce::ValueTree& node) -> bool
    {
        const bool idMatch = (node.hasProperty("id") && node.getProperty("id").toString() == paramID)
                             || (node.hasProperty("paramID") && node.getProperty("paramID").toString() == paramID)
                             || (node.getType().toString() == paramID);

        if (idMatch && node.hasProperty("value"))
        {
            node.setProperty("value", normalisedValue, nullptr);
            return true;
        }
        return false;
    };

    std::function<void(juce::ValueTree&)> walk = [&](juce::ValueTree& node)
    {
        if (updated)
            return;

        if (tryNode(node))
        {
            updated = true;
            return;
        }

        for (int i = 0; i < node.getNumChildren(); ++i)
        {
            auto c = node.getChild(i);
            walk(c);
            if (updated)
                return;
        }
    };

    walk(state);
    return updated;
}

static bool hasParamInState(const juce::ValueTree& state, const juce::String& paramID)
{
    if (state.hasProperty(paramID))
        return true;

    const auto matches = [&](const juce::ValueTree& node) -> bool
    {
        const bool idMatch = (node.hasProperty("id") && node.getProperty("id").toString() == paramID)
                             || (node.hasProperty("paramID") && node.getProperty("paramID").toString() == paramID)
                             || (node.getType().toString() == paramID);

        return idMatch && node.hasProperty("value");
    };

    std::function<bool(const juce::ValueTree&)> walk = [&](const juce::ValueTree& node) -> bool
    {
        if (matches(node))
            return true;

        for (int i = 0; i < node.getNumChildren(); ++i)
            if (walk(node.getChild(i)))
                return true;

        return false;
    };

    return walk(state);
}

static void ensureDefaultParamExists(juce::AudioProcessorValueTreeState& apvts,
                                     juce::ValueTree& state,
                                     const juce::String& paramID)
{
    if (hasParamInState(state, paramID))
        return;

    if (auto* param = apvts.getParameter(paramID))
    {
        const float defaultNorm = (float) param->getDefaultValue();
        juce::ValueTree child(paramID);
        child.setProperty("value", defaultNorm, nullptr);
        state.addChild(child, -1, nullptr);
    }
}

juce::ValueTree PresetManager::buildFactoryPresetState(const juce::String& name) const
{
    juce::ValueTree state = apvts.copyState();

    const FactoryPresetDef* def = nullptr;
    for (const auto& p : kFactoryPresets)
    {
        if (name == p.name)
        {
            def = &p;
            break;
        }
    }
    if (def == nullptr)
        return state;

    // Apply parameter values into the APVTS state tree.
    if (def->paramPairs != nullptr)
    {
        for (int i = 0; def->paramPairs[i] != nullptr && def->paramPairs[i + 1] != nullptr; i += 2)
        {
            const juce::String id(def->paramPairs[i]);
            const char* valueStr = def->paramPairs[i + 1];

            float plain = 0.0f;
            if (!parseFloat(valueStr, plain))
                continue;

            if (auto* param = apvts.getParameter(id))
            {
                const float norm = (float) param->convertTo0to1(plain);
                setParamValueInState(state, id, juce::jlimit(0.0f, 1.0f, norm));
            }
        }
    }

    const auto order = orderFromNullTerminated(def->order);
    float genToChainPlain = apvts.getRawParameterValue("gen_to_chain")->load();
    float genMixPlain = apvts.getRawParameterValue("gen_mix")->load();
    findPlainParamValue(def->paramPairs, "gen_to_chain", genToChainPlain);
    findPlainParamValue(def->paramPairs, "gen_mix", genMixPlain);

    const bool genToChain = genToChainPlain > 0.5f;
    const float genMix = genMixPlain;

    addFxChainState(state, order.size() > 0 ? order : fxChain.getModuleOrder(), genToChain, genMix);
    addModMatrixState(state, def->assigns);

    return state;
}

void PresetManager::writePresetFileIfMissing(const juce::String& name, const juce::ValueTree& state)
{
    auto presetFile = getPresetFolder().getChildFile(name + ".rocketpreset");
    if (presetFile.existsAsFile())
        return;

    presetFile.deleteFile();
    presetFile.create();
    juce::FileOutputStream out(presetFile);
    state.writeToStream(out);
}

void PresetManager::addFactoryPresetsIfMissing()
{
    for (const auto& p : kFactoryPresets)
        writePresetFileIfMissing(p.name, buildFactoryPresetState(p.name));
}

void PresetManager::ensureFactoryPresets()
{
    auto dir = getPresetFolder();
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.rocketpreset");

    // If the user already has presets, don't touch anything.
    if (files.size() > 0)
        return;

    addFactoryPresetsIfMissing();
}

void PresetManager::savePreset(const juce::String& name)
{
    auto presetFile = getPresetFolder().getChildFile(name + ".rocketpreset");
    juce::ValueTree state = apvts.copyState();
    appendState(state);

    presetFile.deleteFile();
    presetFile.create();
    juce::FileOutputStream out(presetFile);
    state.writeToStream(out);
}

void PresetManager::loadPreset(const juce::String& name)
{
    auto presetFile = getPresetFolder().getChildFile(name + ".rocketpreset");
    if (!presetFile.existsAsFile())
        return;

    juce::FileInputStream in(presetFile);
    auto state = juce::ValueTree::readFromStream(in);
    if (!state.isValid())
        return;

    // Forward-compat: older preset files may not include newer parameters.
    // Ensure newly added parameters exist with defaults before replacing state.
    ensureDefaultParamExists(apvts, state, "reverb_algo");
    ensureDefaultParamExists(apvts, state, "delay_mode1");
    ensureDefaultParamExists(apvts, state, "delay_mode2");
    ensureDefaultParamExists(apvts, state, "delay_tape_tone");
    ensureDefaultParamExists(apvts, state, "delay_sync1");
    ensureDefaultParamExists(apvts, state, "delay_div1");
    ensureDefaultParamExists(apvts, state, "delay_sync2");
    ensureDefaultParamExists(apvts, state, "delay_div2");
    ensureDefaultParamExists(apvts, state, "tremolo_sync");
    ensureDefaultParamExists(apvts, state, "tremolo_div");

    apvts.replaceState(state);
    restoreFromState(state);

    // Flush internal DSP state (delay lines, reverb tails, filters) so preset loads are deterministic.
    fxChain.reset();
}

bool PresetManager::deletePreset(const juce::String& name)
{
    auto presetFile = getPresetFolder().getChildFile(name + ".rocketpreset");
    if (!presetFile.existsAsFile())
        return false;
    return presetFile.deleteFile();
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    auto dir = getPresetFolder();
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.rocketpreset");
    for (auto& f : files)
        names.add(f.getFileNameWithoutExtension());
    return names;
}

void PresetManager::appendState(juce::ValueTree& parent) const
{
    fxChain.appendState(parent);
    modMatrix.appendState(parent);
}

void PresetManager::restoreFromState(const juce::ValueTree& parent)
{
    fxChain.restoreFromState(parent);
    modMatrix.restoreFromState(parent);
}
