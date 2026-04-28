#pragma once

#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

struct AppContext;

/** Edit Mode: slot metadata, bypass/remove/editor, automatable parameter list with hardware assignment.

    Assignments call `ParameterMappingManager::assignParameterToHardwareInActiveVariation` for the active
    scene + chain variation. Message thread only. */
class PluginInspectorComponent final : public juce::Component,
                                         private juce::Timer
{
public:
    explicit PluginInspectorComponent(AppContext& context);
    ~PluginInspectorComponent() override;

    /** -1 = none selected. */
    void setInspectedSlot(int slotIndex);

    void refreshFromHost();

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Optional: e.g. rack reload after remove / bypass. */
    std::function<void()> onModelChanged;

    /** Optional: notify after mapping edits (e.g. refresh Performance labels). */
    std::function<void()> onMappingsChanged;

private:
    class ParameterRow;
    void timerCallback() override;

    void rebuildParameterRows();
    void refreshHeader();
    void refreshParameterValueTexts();
    void updateEmptyStateVisibility();
    juce::String computeSlotSignature() const;

    AppContext& appContext;
    int inspectedSlot { -1 };

    juce::Label headingLabel;
    juce::Label pluginNameLabel;
    juce::Label metaLabel;
    juce::Label slotLabel;

    juce::ToggleButton bypassToggle { "Bypass" };
    juce::TextButton removeButton { "Remove plugin" };
    juce::TextButton openEditorButton { "Open fullscreen editor" };
    juce::TextButton moveLeftButton { "Move left" };
    juce::TextButton moveRightButton { "Move right" };
    juce::TextButton replacePluginButton { "Replace plugin" };

    juce::Label emptyHintLabel;

    juce::Viewport parameterViewport;
    std::unique_ptr<juce::Component> parameterList;

    std::vector<std::unique_ptr<ParameterRow>> parameterRows;

    juce::String lastSlotSignature;

    void openFullscreenPluginEditor();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginInspectorComponent)
};

} // namespace forge7
