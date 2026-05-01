#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

/** Non-interactive opaque strip behind fullscreen plugin editor chrome (header / tool rows / footer). */
class ChromeBackgroundBand final : public juce::Component
{
public:
    explicit ChromeBackgroundBand(juce::Colour fillColour) noexcept;

    void setFillColour(juce::Colour c) noexcept;

    void paint(juce::Graphics& g) override;

private:
    juce::Colour fill;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChromeBackgroundBand)
};

/** Dedicated bounds for `PluginEditorCanvas`; clips and paints behind the hosted editor subtree. */
class PluginEditorViewportFrame final : public juce::Component
{
public:
    PluginEditorViewportFrame() noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorViewportFrame)
};

} // namespace forge7
