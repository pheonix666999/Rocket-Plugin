cmake_minimum_required(VERSION 3.15)
project(TheRocketRemake VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Reuse JUCE from the existing checkout in this workspace.
set(_JUCE_DEFAULT_DIR "${CMAKE_CURRENT_LIST_DIR}/../The Rocket/JUCE_SRC")
if (DEFINED JUCE_DIR)
  add_subdirectory(${JUCE_DIR} JUCE)
elseif (EXISTS "${_JUCE_DEFAULT_DIR}/CMakeLists.txt")
  add_subdirectory("${_JUCE_DEFAULT_DIR}" JUCE)
else()
  message(FATAL_ERROR "JUCE not found. Set JUCE_DIR or ensure ../The Rocket/JUCE_SRC exists.")
endif()

# Formats: AU for macOS, VST3 for Windows/macOS, Standalone for testing
if(APPLE)
  set(ROCKET_FORMATS AU VST3 Standalone)
else()
  set(ROCKET_FORMATS VST3 Standalone)
endif()

# Enable AAX if SDK present
option(ROCKET_ENABLE_AAX "Enable AAX format" OFF)
if (ROCKET_ENABLE_AAX)
  list(APPEND ROCKET_FORMATS AAX)
endif()

# Build-time flag for the internal preset designer UI (hidden in public builds)
option(ROCKET_INTERNAL_UI "Show internal preset designer UI" OFF)

juce_add_plugin(TheRocket
  COMPANY_NAME "Singomakers"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  BUNDLE_ID "net.singomakers.the-rocket"
  PLUGIN_MANUFACTURER_CODE Sigo
  PLUGIN_CODE Thro
  FORMATS ${ROCKET_FORMATS}
  PRODUCT_NAME "The Rocket"
  AU_MAIN_TYPE "kAudioUnitType_Effect"
  VST3_CATEGORIES "Fx"
)

juce_generate_juce_header(TheRocket)

# Source files
set(SOURCE_FILES
  Source/Main.cpp
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/PluginEditor.cpp
  Source/PluginEditor.h
  Source/DSP/FxModule.h
  Source/DSP/FxChain.h
  Source/DSP/FxChain.cpp
  Source/DSP/Modules.h
  Source/DSP/Modules.cpp
  Source/DSP/ModMatrix.h
  Source/DSP/ModMatrix.cpp
  Source/PresetManager.h
  Source/PresetManager.cpp
  Source/LookAndFeel/RocketLookAndFeel.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source FILES ${SOURCE_FILES})

target_sources(TheRocket PRIVATE ${SOURCE_FILES})

target_compile_definitions(TheRocket
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

if (ROCKET_INTERNAL_UI)
  target_compile_definitions(TheRocket PUBLIC ROCKET_INTERNAL_UI=1)
endif()

target_link_libraries(TheRocket
  PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# UI assets - include both original assets and rack assets
juce_add_binary_data(TheRocketAssets SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/../The Rocket/Assets/ui_background.png"
  "${CMAKE_CURRENT_LIST_DIR}/../The Rocket/Assets/ui_clouds.png"
  "${CMAKE_CURRENT_LIST_DIR}/../The Rocket/Assets/ui_panel.png"
  "${CMAKE_CURRENT_LIST_DIR}/../The Rocket/Assets/ui_rocket.png"
  "${CMAKE_CURRENT_LIST_DIR}/../The Rocket/Assets/ui_flame.png"
)

target_link_libraries(TheRocket
  PRIVATE
    TheRocketAssets
)

# Platform specific settings for clean installer generation
if(APPLE)
  set_target_properties(TheRocket PROPERTIES
    MACOSX_BUNDLE TRUE
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ""
  )
endif()
