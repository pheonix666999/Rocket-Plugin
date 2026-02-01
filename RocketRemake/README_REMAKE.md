# The Rocket - Remake Project

This project is a complete remake of "The Rocket" transition plugin, featuring a modular DSP architecture and a fully animated UI.

## Features

- **One-Knob Interface**: Simple "Amount" knob controls a complex chain of effects.
- **Visual Feedback**: Animated rocket launch, clouds, and flame intensity based on the Amount.
- **Modular DSP Chain**: 12+ effect modules including Reverb, Dual Delay, Distortion, Filters, etc.
- **Macro Modulation**: Internal modulation matrix maps the "Amount" knob to any parameter with custom ranges and curves.
- **Preset System**: Full preset management compatible with the original `.earcandy_preset` logic.

## Build Instructions

### Windows
Use the provided batch scripts:
- **Public Build**: Run `build_windows.bat` - Builds VST3 and Standalone with the standard minimal UI.
- **Internal UI Build**: Run `build_windows_internal.bat` - Builds Standalone with the `ROCKET_INTERNAL_UI` flag enabled.

### macOS
Use the provided shell scripts (requires CMake and Xcode):
- **Public Build**: Run `./build_mac.sh`
- **Internal UI Build**: Run `./build_mac_internal.sh`

## Internal Developer UI

To create new presets or tweak the sound design, you must use the **Internal UI Build**.
1. Run the internal build executable (e.g., `build_win_internal/TheRocket_artefacts/Release/Standalone/The Rocket.exe`).
2. Click the **"Internal"** toggle button in the top header.
3. The **Developer Panel** will appear, allowing you to:
   - Reorder effects in the FX Chain.
   - Adjust parameters for all modules.
   - create Modulation Mappings (map Amount -> Parameter with Min/Max range).
   - Save and Load presets.

Any preset saved in the internal UI can be loaded in the public plugin.

## Project Structure

- **Source/Main.cpp**: Plugin entry point.
- **Source/PluginProcessor**: Main audio processing and state management.
- **Source/PluginEditor**: UI implementation (both public animated UI and internal developer panel).
- **Source/DSP/**: Audio effect modules, FX chain logic, and modulation matrix.
- **Source/PresetManager**: Handling of `.earcandy_preset` files.
- **Source/LookAndFeel**: Custom styling for the internal UI controls.
- **Assets/**: UI image resources.
