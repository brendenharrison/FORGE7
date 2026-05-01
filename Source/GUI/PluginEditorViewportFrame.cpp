#include "PluginEditorViewportFrame.h"

namespace forge7
{
namespace
{
juce::Colour viewportFrameBg() noexcept { return juce::Colour(0xff12161c); }
} // namespace

ChromeBackgroundBand::ChromeBackgroundBand(juce::Colour fillColour) noexcept : fill(fillColour)
{
    setOpaque(true);
    setInterceptsMouseClicks(false, false);
    setPaintingIsUnclipped(false);
}

void ChromeBackgroundBand::setFillColour(juce::Colour c) noexcept
{
    fill = c;
    repaint();
}

void ChromeBackgroundBand::paint(juce::Graphics& g)
{
    g.fillAll(fill);
}

PluginEditorViewportFrame::PluginEditorViewportFrame() noexcept
{
    setOpaque(true);
    setPaintingIsUnclipped(false);
}

void PluginEditorViewportFrame::paint(juce::Graphics& g)
{
    g.reduceClipRegion(getLocalBounds());
    g.fillAll(viewportFrameBg());
}

void PluginEditorViewportFrame::resized()
{
    for (auto* child : getChildren())
        if (child != nullptr)
            child->setBounds(getLocalBounds());
}

} // namespace forge7
