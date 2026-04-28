# FORGE 7 — CMake entry. Example: cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release && cmake --build cmake-build
cmake_minimum_required(VERSION 3.22)
project(Forge7 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
)

juce_generate_juce_header(Forge7)

# Paths in Source/CMakeLists.txt are relative to Source/
list(TRANSFORM FORGE7_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/Source/"
     OUTPUT_VARIABLE FORGE7_SOURCES_ABS)

target_sources(Forge7 PRIVATE ${FORGE7_SOURCES_ABS})

target_include_directories(Forge7 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_link_libraries(Forge7 PRIVATE
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
)

target_compile_definitions(Forge7 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="FORGE 7"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    # Required for VST3 format registration in AudioPluginFormatManager::addDefaultFormats().
    JUCE_PLUGINHOST_VST3=1
)
