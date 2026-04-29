#pragma once

#include <array>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Controls/HardwareControlTypes.h"

namespace forge7
{

struct AppContext;

/** Rack/Edit details panel: read-only reference of the active Chain's control assignments.

    Shows K1-K4 plus Button 1 / Button 2 mappings for the active Scene/Chain and the loaded plugin
    parameter values. Includes a compact flat strip plus detail cards; touch-friendly; message thread only. */
class ChainControlsPanelComponent final : public juce::Component,
                                         private juce::Timer
{
public:
    explicit ChainControlsPanelComponent(AppContext& context);
    ~ChainControlsPanelComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Safe to call after chain/project hydration or mapping edits. */
    void refreshFromHost();

private:
    void timerCallback() override;
    void rebuildHeaderText();
    void refreshCardsFromHost();

    AppContext& appContext;

    juce::Label headingLabel;
    juce::Label sceneLabel;
    juce::Label chainLabel;

    /** K1-K4 | Button 1 | Button 2 horizontal summary strip. */
    std::array<std::unique_ptr<juce::Component>, 6> stripCells {};

    std::array<std::unique_ptr<juce::Component>, 6> cards {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainControlsPanelComponent)
};

} // namespace forge7

