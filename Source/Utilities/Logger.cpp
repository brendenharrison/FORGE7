#include "Logger.h"

namespace forge7
{

void Logger::info(const juce::String& message)
{
    juce::Logger::writeToLog("[FORGE7] " + message);
}

void Logger::warn(const juce::String& message)
{
    juce::Logger::writeToLog("[FORGE7][warn] " + message);
}

void Logger::error(const juce::String& message)
{
    juce::Logger::writeToLog("[FORGE7][error] " + message);
}

} // namespace forge7
