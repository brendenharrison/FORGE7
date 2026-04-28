#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

/** One encoder-navigable stop: optional rotate handler consumes detents instead of moving focus. */
struct EncoderFocusItem
{
    juce::Component::SafePointer<juce::Component> target;
    std::function<void()> onActivate;
    std::function<void(int deltaSteps)> onRotate;
};

} // namespace forge7
