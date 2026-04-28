#pragma once

#include <juce_core/juce_core.h>

namespace forge7
{

/** Agent debug NDJSON (session 985082). Fold region in IDE. */
// #region agent log
inline void debugAgentLog(const char* hypothesisId,
                          const char* location,
                          const char* message,
                          const juce::var& data = juce::var())
{
    auto* o = new juce::DynamicObject();
    o->setProperty("sessionId", juce::String("985082"));
    o->setProperty("hypothesisId", juce::String(hypothesisId));
    o->setProperty("location", juce::String(location));
    o->setProperty("message", juce::String(message));

    if (!data.isVoid())
        o->setProperty("data", data);
    else
        o->setProperty("data", juce::var(new juce::DynamicObject()));

    o->setProperty("timestamp", juce::Time::getMillisecondCounterHiRes());

    const juce::File f("/Users/brendenharrison/Forge_7/.cursor/debug-985082.log");
    (void)f.getParentDirectory().createDirectory();
    (void)f.appendText(juce::JSON::toString(juce::var(o)) + "\n");
}
// #endregion

} // namespace forge7
