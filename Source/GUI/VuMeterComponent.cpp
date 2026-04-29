#include "VuMeterComponent.h"

namespace forge7
{
namespace
{
constexpr float kClipThreshold = 0.98f;
constexpr uint32_t kClipHoldMs = 1200;
constexpr float kDecay = 0.88f;

inline void smoothPeak(float& displayed, float target) noexcept
{
    if (target >= displayed)
        displayed = target;
    else
        displayed = target + (displayed - target) * kDecay;
}

inline juce::Colour colourForLevel(float level01) noexcept
{
    const float x = juce::jlimit(0.0f, 1.0f, level01);

    if (x < 0.55f)
        return juce::Colour(0xff4caf50);

    if (x < 0.82f)
        return juce::Colour(0xffffc44d);

    return juce::Colour(0xffff7043);
}

} // namespace

VuMeterComponent::VuMeterComponent(std::function<float()> readLeft,
                                   std::function<float()> readRight,
                                   const bool verticalLayoutIn)
    : readLeftFn(std::move(readLeft))
    , readRightFn(readRight != nullptr ? std::move(readRight) : std::function<float()>())
    , verticalLayout(verticalLayoutIn)
{
    setInterceptsMouseClicks(false, false);
    setOpaque(false);
    startTimerHz(40);
}

VuMeterComponent::~VuMeterComponent()
{
    stopTimer();
}

void VuMeterComponent::timerCallback()
{
    float rawL = 0.0f;
    float rawR = 0.0f;

    if (readLeftFn)
        rawL = juce::jlimit(0.0f, 1.0f, readLeftFn());

    if (readRightFn)
        rawR = juce::jlimit(0.0f, 1.0f, readRightFn());
    else
        rawR = rawL;

    smoothPeak(displayedL, rawL);
    smoothPeak(displayedR, rawR);

    const uint32_t now = juce::Time::getApproximateMillisecondCounter();

    if (rawL >= kClipThreshold || rawR >= kClipThreshold)
        clipHoldUntilMs = now + kClipHoldMs;

    repaint();
}

void VuMeterComponent::paint(juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    const float captionH = caption.isNotEmpty() ? 11.0f : 0.0f;
    auto work = full;

    if (captionH > 0.0f)
        work = full.removeFromBottom(captionH);

    const uint32_t now = juce::Time::getApproximateMillisecondCounter();
    const bool clipLit = now < clipHoldUntilMs;

    if (verticalLayout)
    {
        auto r = work.reduced(0.0f, 1.0f);
        const float dotH = 7.0f;
        auto dotRow = r.removeFromTop(dotH);

        if (clipLit)
        {
            const float d = 5.0f;
            g.setColour(juce::Colours::red);
            g.fillEllipse(juce::Rectangle<float>(d, d).withCentre({dotRow.getCentreX(), dotRow.getCentreY()}));
        }

        const float gap = 2.0f;
        const float barW = juce::jmax(3.0f, (r.getWidth() - gap) * 0.5f);
        auto leftBar = r.removeFromLeft(barW);
        r.removeFromLeft(gap);
        auto rightBar = r;

        auto drawBar = [&](juce::Rectangle<float> zone, float level01)
        {
            g.setColour(juce::Colour(0xff1a1f26));
            g.fillRoundedRectangle(zone, 2.0f);

            const float h = zone.getHeight() * juce::jlimit(0.0f, 1.0f, level01);
            auto fill = zone.withTop(zone.getBottom() - h);

            g.setColour(colourForLevel(level01));
            g.fillRoundedRectangle(fill, 2.0f);

            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.drawRoundedRectangle(zone, 2.0f, 1.0f);
        };

        drawBar(leftBar, displayedL);
        drawBar(rightBar, displayedR);
    }
    else
    {
        auto r = work.reduced(0.0f, 1.0f);
        const float gap = 2.0f;
        const float barH = juce::jmax(3.0f, (r.getHeight() - gap) * 0.5f);
        auto topBar = r.removeFromTop(barH);
        r.removeFromTop(gap);
        auto botBar = r;

        auto drawBarH = [&](juce::Rectangle<float> zone, float level01)
        {
            g.setColour(juce::Colour(0xff1a1f26));
            g.fillRoundedRectangle(zone, 2.0f);

            const float w = zone.getWidth() * juce::jlimit(0.0f, 1.0f, level01);
            auto fill = zone.withLeft(zone.getX()).withRight(zone.getX() + w);

            g.setColour(colourForLevel(level01));
            g.fillRoundedRectangle(fill, 2.0f);

            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.drawRoundedRectangle(zone, 2.0f, 1.0f);
        };

        drawBarH(topBar, displayedL);
        drawBarH(botBar, displayedR);

        if (clipLit)
        {
            g.setColour(juce::Colours::red);
            g.fillEllipse(full.removeFromRight(7.0f).removeFromTop(7.0f).toNearestIntEdges().toFloat());
        }
    }

    if (caption.isNotEmpty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.setFont(juce::Font(9.0f));
        g.drawText(caption, getLocalBounds().removeFromBottom(static_cast<int>(captionH)), juce::Justification::centred,
                   false);
    }
}

} // namespace forge7
