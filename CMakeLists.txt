# FORGE 7 — CMake entry. Example: cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release && cmake --build cmake-build
cmake_minimum_required(VERSION 3.22)
project(Forge7 VERSION 0.1.0 LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Desktop/dev-only helpers.
option(FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW
       "Enable simulated hardware controls for desktop development"
       ON)

add_subdirectory(Source)

include(FetchContent)
# JUCE 8.x requires macOS 11.1+ SDK (Xcode 12.4+). Pin 7.0.12 for older Command Line Tools / CI images;
# bump to 8.x when your machine has a current Xcode SDK.
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(Forge7
    COMPANY_NAME "FORGE"
    PRODUCT_NAME "FORGE 7"
    VERSION ${PROJECT_VERSION}
    PLIST_TO_MERGE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info-Microphone.plist"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "FORGE7 needs audio input access to process live guitar and audio input."
)

# Reinforce JUCE target properties on Apple so juceaide always sees microphone + merged plist.
if(APPLE)
    set_target_properties(Forge7 PROPERTIES
        JUCE_MICROPHONE_PERMISSION_ENABLED TRUE
        JUCE_MICROPHONE_PERMISSION_TEXT "FORGE7 needs audio input access to process live guitar and audio input.")
endif()

juce_generate_juce_header(Forge7)

# Paths in Source/CMakeLists.txt are relative to Source/
list(TRANSFORM FORGE7_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/Source/"
     OUTPUT_VARIABLE FORGE7_SOURCES_ABS)

target_sources(Forge7 PRIVATE ${FORGE7_SOURCES_ABS})

if(APPLE)
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Platform/MacAudioPermission.mm"
        PROPERTIES LANGUAGE OBJCXX
    )
endif()

target_include_directories(Forge7 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_link_libraries(Forge7 PRIVATE
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
)

if(APPLE)
    target_link_libraries(Forge7 PRIVATE "-framework AVFoundation")
endif()

target_compile_definitions(Forge7 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="FORGE 7"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    # Required for VST3 format registration in AudioPluginFormatManager::addDefaultFormats().
    JUCE_PLUGINHOST_VST3=1
    FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW=$<BOOL:${FORGE7_ENABLE_SIMULATED_HARDWARE_WINDOW}>
)

# Optional dev-only guard: ensure no non-ASCII characters exist in Source/ (prevents mojibake regressions).
add_custom_target(check_ascii_ui_text
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_ascii_ui_text.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Checking Source/ for non-ASCII characters"
)
