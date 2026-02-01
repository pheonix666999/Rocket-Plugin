# The Rocket (JUCE Plugin)

One‑knob transition / build‑up plugin with a hidden internal preset‑designer mode.

## What You Get

**End‑user UI**
- `Amount` macro knob
- Preset selector + next/prev buttons

**Internal preset designer UI (build option)**
- Reorder FX chain
- Enable/disable modules
- Per‑module mix + full parameter control
- Macro modulation mapping (positive/negative + optional min/max range)
- Preset tools (refresh/save/save-as/delete)

## Presets

Presets are stored as `*.rocketpreset` files here:
- Windows: `C:\\Users\\<You>\\Documents\\TheRocket\\Presets`

On first run (when that folder is empty), the plugin auto‑creates a set of factory presets so the preset menu is never empty.

## Build (CMake)

This repo includes JUCE sources under `JUCE_SRC/` (and may also have a `JUCE/` submodule folder). You can build without setting `JUCE_DIR`.

```powershell
cd "The Rocket"
cmake -S . -B build_juce
cmake --build build_juce --config Release --target TheRocket_Standalone
cmake --build build_juce --config Release --target TheRocket_VST3
```

### Internal UI build

The internal preset‑designer UI is **disabled by default** (end‑user build).

Enable it like this:

```powershell
cmake -S . -B build_juce -DROCKET_INTERNAL_UI=ON
cmake --build build_juce --config Release --target TheRocket_Standalone
```

### AAX build (optional)

If you have the AAX SDK set up:

```powershell
cmake -S . -B build_juce -DROCKET_ENABLE_AAX=ON
cmake --build build_juce --config Release
```

## Output Files

**Standalone exe**
- `build_juce\\TheRocket_artefacts\\Release\\Standalone\\The Rocket.exe`

**VST3**
- `build_juce\\TheRocket_artefacts\\Release\\VST3\\The Rocket.vst3`

If you’re looking for the VST3 after building, this is the exact folder to copy:
- `The Rocket\\build_juce\\TheRocket_artefacts\\Release\\VST3\\The Rocket.vst3`

## Install (Windows VST3)

Copy the `.vst3` folder to:
- `C:\\Program Files\\Common Files\\VST3`

Then rescan plugins in your DAW.

## UI Notes

- The plugin/editor is portrait and resizable with a fixed aspect ratio.
- The standalone window sizes itself to the display’s usable area (taskbar-safe).

## Packaging / Installer Notes

This repo builds plugin binaries (Standalone/VST3/AU/AAX if configured) but does **not** include an installer script.

- Windows: ship the built `.vst3` by zipping it, or create an installer with tools like Inno Setup / NSIS.
- macOS: package `.vst3` / `.component` / `.aaxplugin` into a `.pkg` / `.dmg` in a separate packaging step.
