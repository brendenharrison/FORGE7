#pragma once

#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "HardwareControlTypes.h"
#include "ParameterMappingDescriptor.h"

namespace forge7
{

class PluginHostManager;
class SceneManager;

/** Maintains hardware -> plugin parameter bindings scoped by scene + chain variation.

    All parameter writes use `AudioProcessorParameter::{beginChangeGesture,setValueNotifyingHost,endChangeGesture}`
    from the **message thread** only (driven by `ControlManager::submitHardwareEvent`). */
class ParameterMappingManager
{
public:
    ParameterMappingManager(SceneManager& scenes, PluginHostManager& hosts);
    ~ParameterMappingManager();

    PluginHostManager& getPluginHostManager() noexcept { return pluginHostManager; }
    SceneManager& getSceneManager() noexcept { return sceneManager; }

    /** Invoked from `ControlManager` on the message thread for knob + assign-button events. */
    void processHardwareEvent(const HardwareControlEvent& event);

    /** Replace or insert a mapping (matched on hardwareControlId + sceneId + chainVariationId). */
    void upsertMapping(ParameterMappingDescriptor mapping);

    void removeMapping(const ParameterMappingDescriptor& keyMatch);

    /** All stored rows (copy) - message thread. */
    juce::Array<ParameterMappingDescriptor> getAllMappings() const;

    /** Merge `fields` with the active scene + variation ids, then upsert. */
    bool upsertMappingForActiveSceneVariation(ParameterMappingDescriptor fields);

    /** Convenience: bind a single automatable parameter to one of the six assignable controls. */
    bool assignParameterToHardwareInActiveVariation(HardwareControlId hardwareId,
                                                   int pluginSlotIndex,
                                                   const juce::String& pluginParameterId,
                                                   int pluginParameterIndexFallback,
                                                   const juce::String& displayNameForUi,
                                                   float minNorm = 0.0f,
                                                   float maxNorm = 1.0f,
                                                   bool invertMapping = false,
                                                   bool toggleForButton = false,
                                                   bool momentaryForButton = true);

    /** Enumerate automatable parameters for the audible chain's slot - message thread only. */
    juce::Array<AutomatableParameterSummary> getAutomatableParametersForSlot(int pluginSlotIndex) const;

    /** Live value string from the hosted plugin for Performance HUD (message thread only). */
    juce::String getMappedParameterValueText(const ParameterMappingDescriptor& row) const;

    /** Fullscreen Assign Mode: next K1-K4 hardware move assigns that knob to this parameter (scene/variation scoped). */
    void prepareKnobAssignmentToNextHardwareMove(int pluginSlotIndex,
                                                 const juce::String& pluginParameterId,
                                                 int pluginParameterIndexFallback,
                                                 const juce::String& displayNameForUi);

    void cancelKnobAssignmentLearn() noexcept;

    bool isAwaitingKnobAssignmentHardwareMove() const noexcept;

    /** Serialize / deserialize under project key `globalParameterMappings`. */
    juce::var exportMappingsToVar() const;

    void importMappingsFromVar(const juce::var& data);

    // --- Future GUI learn (placeholders) ---------------------------------------------

    /** Future: arm learn mode so the next touched plugin UI control binds to `hardwareId`. */
    void armLearnTargetForHardware(HardwareControlId hardwareId);

    void cancelLearn();

    bool isLearning() const noexcept;

private:
    SceneManager& sceneManager;
    PluginHostManager& pluginHostManager;

    mutable juce::CriticalSection mappingLock;
    std::vector<ParameterMappingDescriptor> mappings;

    bool learnArmed { false };
    HardwareControlId learnHardwareTarget { HardwareControlId::Knob1 };

    static void clampDescriptor(ParameterMappingDescriptor& d);

    static juce::String hardwareIdToKey(HardwareControlId id);
    static bool keyToHardwareId(const juce::String& key, HardwareControlId& out);

    const ParameterMappingDescriptor* findActiveMapping(const HardwareControlEvent& event,
                                                        const juce::String& sceneId,
                                                        const juce::String& variationId) const;

    juce::AudioProcessorParameter* resolveParameter(juce::AudioProcessor& processor,
                                                   const ParameterMappingDescriptor& row) const;

    void applyNormalizedToParameter(juce::AudioProcessorParameter& param,
                                    const ParameterMappingDescriptor& row,
                                    float hardwareNormalized01) const;

    void applyButtonToParameter(juce::AudioProcessorParameter& param,
                                const ParameterMappingDescriptor& row,
                                bool pressed) const;

    mutable juce::CriticalSection assignmentLearnLock;
    struct KnobAssignmentLearnState
    {
        bool awaitingKnobTwist { false };
        int pluginSlotIndex { -1 };
        juce::String pluginParameterId;
        int pluginParameterIndex { -1 };
        juce::String displayNameForUi;
    } knobAssignmentLearn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterMappingManager)
};

} // namespace forge7
