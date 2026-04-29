#pragma once

#include <juce_core/juce_core.h>

#include "HardwareControlTypes.h"

namespace forge7
{

/** One persisted mapping from physical hardware to a hosted plugin parameter for a specific
    scene + chain variation snapshot.

    Future **GUI learn** workflow (not implemented): inspector stores `hardwareControlId` +
    `(sceneId, chainVariationId)` as today, but `pluginParameterId` is discovered by intercepting the
    next parameter gesture from `AudioProcessorEditor` / attachment - see `ParameterMappingManager::armLearnTargetForHardware`.

    **Value scaling**
    - Knobs: product uses relative encoder deltas (`HardwareControlType::RelativeDelta`); optional dev/legacy
      absolute 0...1 (`HardwareControlType::AbsoluteNormalized`) for MIDI pots or tests. Linearly
      into `[minValue, maxValue]` where both are **plugin-normalized** 0...1 (`AudioProcessorParameter`
      domain), then apply `invert` to the hardware side before scaling.

    **Buttons** (`AssignButton1` / `AssignButton2`)
    - `momentaryMode`: pressed -> `maxValue`, released -> `minValue` (typical "hold" behaviour).
    - `toggleMode`: each press alternates between `minValue` and `maxValue` (used for bool/switch params).

    Threading: edit descriptors only from the **message thread**. `ParameterMappingManager` applies
    parameters on the message thread - never from `audioDeviceIOCallback`. */
struct ParameterMappingDescriptor
{
    HardwareControlId hardwareControlId { HardwareControlId::Knob1 };

    juce::String sceneId;
    juce::String chainVariationId;

    int pluginSlotIndex { 0 };

    /** Prefer stable VST3 `paramID` from `AudioProcessorParameter::getParameterID()`. */
    juce::String pluginParameterId;

    /** Fallback when `pluginParameterId` is empty or not found after reload. */
    int pluginParameterIndex { -1 };

    juce::String displayName;

    /** Plugin-normalized range endpoints (0...1) applied to this parameter. Defaults = full sweep. */
    float minValue { 0.0f };
    float maxValue { 1.0f };

    bool invert { false };

    bool toggleMode { false };
    bool momentaryMode { true };

    bool operator==(const ParameterMappingDescriptor& o) const noexcept;
    bool operator!=(const ParameterMappingDescriptor& o) const noexcept { return !(*this == o); }
};

/** Lightweight row for UI lists (slot inspector). */
struct AutomatableParameterSummary
{
    juce::String parameterId;
    int parameterIndex { -1 };
    juce::String name;
    bool isAutomatable { true };
};

} // namespace forge7
