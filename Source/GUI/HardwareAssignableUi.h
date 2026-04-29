#pragma once

#include "../Controls/HardwareControlTypes.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

/** Stage-readable LED colours: Button 1 = blue, Button 2 = warm yellow/amber. */
inline juce::Colour button1Colour() noexcept { return juce::Colour(0xff4a9eff); }
inline juce::Colour button2Colour() noexcept { return juce::Colour(0xffffc44d); }

/** Short user-visible names for knobs and programmable buttons (not serialization keys). */
inline juce::String hardwareDisplayShortName(const HardwareControlId id)
{
    switch (id)
    {
        case HardwareControlId::Knob1:
            return "K1";
        case HardwareControlId::Knob2:
            return "K2";
        case HardwareControlId::Knob3:
            return "K3";
        case HardwareControlId::Knob4:
            return "K4";
        case HardwareControlId::AssignButton1:
            return "Button 1";
        case HardwareControlId::AssignButton2:
            return "Button 2";
        default:
            return {};
    }
}

} // namespace forge7
