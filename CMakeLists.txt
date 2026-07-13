cmake_minimum_required(VERSION 3.22)

project(StringLife VERSION 1.6.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(STRINGLIFE_ENABLE_LICENSING "Build StringLife with the reusable licensing module enabled" ON)
set(STRINGLIFE_LICENSE_SERVER_BASE_URL "" CACHE STRING "Optional build-time override for the licensing server base URL")
set(STRINGLIFE_LICENSE_PRODUCT_ID "" CACHE STRING "Optional build-time override for the licensing product ID")
set(STRINGLIFE_LICENSE_PUBLIC_KEY "" CACHE STRING "Optional build-time override for the licensing public key")

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS deployment target" FORCE)
endif()

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

set(STRINGLIFE_ASSETS
    Assets/background_panel.png
    Assets/button_on.png
    Assets/button_off.png
    Assets/guitarbass_on.png
    Assets/guitarbass_off.png
    Assets/knob_face.png
    Assets/slider_thumb.png
    Assets/moth_logo.png
    Assets/Fonts/Oxanium-Variable.ttf
    Assets/Fonts/BarlowCondensed-Light.ttf
    Assets/Fonts/BarlowCondensed-Medium.ttf
    Assets/Fonts/Rajdhani-Medium.ttf
)

if (STRINGLIFE_ENABLE_LICENSING)
    list(APPEND STRINGLIFE_ASSETS Assets/licensing_config.json)
endif()

juce_add_binary_data(StringLifeAssets
    SOURCES
        ${STRINGLIFE_ASSETS}
)

juce_add_plugin(StringLife
    COMPANY_NAME "Moth Production"
    BUNDLE_ID "com.mothproduction.stringlife"
    PLUGIN_MANUFACTURER_CODE MthP
    PLUGIN_CODE SLf1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "StringLife"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
)

if (APPLE AND STRINGLIFE_ENABLE_LICENSING)
    set_target_properties(StringLife PROPERTIES
        JUCE_PLIST_TO_MERGE
            "<key>NSAppTransportSecurity</key><dict><key>NSAllowsArbitraryLoads</key><true/></dict>")
endif()

set(STRINGLIFE_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/StringLifeLookAndFeel.cpp
    Source/StringLifeFonts.cpp
    Source/TransientEnhancer.cpp
    Source/HarmonicExciter.cpp
    Source/PresenceEQ.cpp
    Source/UpwardExpander.cpp
)

if (STRINGLIFE_ENABLE_LICENSING)
    list(APPEND STRINGLIFE_SOURCES
        Source/Licensing/LicenseConfig.cpp
        Source/Licensing/LicenseClient.cpp
        Source/Licensing/LicenseManager.cpp
        Source/Licensing/LicenseStore.cpp
        Source/Licensing/MachineFingerprint.cpp
        Source/Licensing/TrialManager.cpp
    )
endif()

target_sources(StringLife PRIVATE ${STRINGLIFE_SOURCES})

target_compile_definitions(StringLife
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        STRINGLIFE_ENABLE_LICENSING=$<BOOL:${STRINGLIFE_ENABLE_LICENSING}>
)

if (STRINGLIFE_ENABLE_LICENSING)
    if (NOT STRINGLIFE_LICENSE_SERVER_BASE_URL STREQUAL "")
        target_compile_definitions(StringLife PUBLIC LICENSE_SERVER_BASE_URL="${STRINGLIFE_LICENSE_SERVER_BASE_URL}")
    endif()

    if (NOT STRINGLIFE_LICENSE_PRODUCT_ID STREQUAL "")
        target_compile_definitions(StringLife PUBLIC LICENSE_PRODUCT_ID="${STRINGLIFE_LICENSE_PRODUCT_ID}")
    endif()

    if (NOT STRINGLIFE_LICENSE_PUBLIC_KEY STREQUAL "")
        target_compile_definitions(StringLife PUBLIC LICENSE_PUBLIC_KEY="${STRINGLIFE_LICENSE_PUBLIC_KEY}")
    endif()
endif()

target_link_libraries(StringLife
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        StringLifeAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if (STRINGLIFE_ENABLE_LICENSING)
    target_link_libraries(StringLife PRIVATE juce::juce_cryptography)

    if (APPLE)
        target_link_libraries(StringLife PRIVATE "-framework SystemConfiguration")
    endif()
endif()
