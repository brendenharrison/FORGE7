#pragma once

#include <array>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

struct AppContext;

/** Rack/Edit: always-visible read-only strip of K1-K4 + Button 1 / Button 2 assignments.

    Scene/chain context comes from the rack header; this panel only shows the hardware mapping summary.
    Message thread only; does not write parameters. */
class ChainControlsPanelComponent final : public juce::Component,
                                          private juce::Timer
{
public:
    explicit ChainControlsPanelComponent(AppContext& context);
    ~ChainControlsPanelComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Safe after chain/project/mapping changes. */
    void refreshFromHost();

private:
    void timerCallback() override;
    void refreshStripFromHost();

    AppContext& appContext;

    juce::Label headingLabel;

    /** K1-K4 | Button 1 | Button 2 */
    std::array<std::unique_ptr<juce::Component>, 6> stripCells {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainControlsPanelComponent)
};

} // namespace forge7
