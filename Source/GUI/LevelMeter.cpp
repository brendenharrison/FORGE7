#include "LevelMeter.h"

#include "../Audio/AudioEngine.h"

namespace forge7
{

LevelMeter::LevelMeter(AudioEngine* engine, MeterChannel channel)
    : audioEngine(engine),
      meterChannel(channel)
{
}

LevelMeter::~LevelMeter() = default;

void LevelMeter::paint(juce::Graphics& g)
{
    float peak = 0.0f;
    if (audioEngine != nullptr)
    {
        peak = meterChannel == MeterChannel::inputAfterGain ? audioEngine->getSmoothedInputPeak()
                                                            : audioEngine->getSmoothedOutputPeak();
    }

    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::orange);
    const float barW = bounds.getWidth() * juce::jlimit(0.0f, 1.0f, peak);
    g.fillRect(bounds.withLeft(bounds.getRight() - barW));

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    const auto label = meterChannel == MeterChannel::inputAfterGain ? "IN" : "OUT";
    g.drawText(label, getLocalBounds().removeFromTop(14), juce::Justification::centredLeft, false);
}

} // namespace forge7
