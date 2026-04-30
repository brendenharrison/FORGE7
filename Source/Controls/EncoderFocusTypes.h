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
    /** When true, EncoderNavigator still navigates/activates but does not draw a global focus ring (e.g. rack cards with their own selection border). */
    bool hideNavigatorFocusRing { false };
    /** Optional callback fired when this item becomes focused via encoder navigation. */
    std::function<void()> onFocus;
};

} // namespace forge7
