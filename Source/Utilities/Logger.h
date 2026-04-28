#pragma once

#include <juce_core/juce_core.h>

namespace forge7
{

/** Thin wrapper around juce::Logger for consistent tagging and optional file sinks.
    Must not be invoked from AudioEngine::audioDeviceIOCallback or any RT path — logging
    can allocate and lock. Use only from message / worker threads. */
class Logger
{
public:
    static void info(const juce::String& message);
    static void warn(const juce::String& message);
    static void error(const juce::String& message);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Logger)
};

} // namespace forge7
