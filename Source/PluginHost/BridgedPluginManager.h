#pragma once

#include <juce_core/juce_core.h>

namespace forge7
{

/** Reserved for experimental Windows VST bridging (Wine / yabridge) on Linux x86.

    Native Linux VST3 hosting stays in `PluginHostManager`; do not route production
    loads through this type until a compatibility tier design is implemented. */
class BridgedPluginManager
{
public:
    BridgedPluginManager() = default;
    ~BridgedPluginManager() = default;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BridgedPluginManager)
};

} // namespace forge7
