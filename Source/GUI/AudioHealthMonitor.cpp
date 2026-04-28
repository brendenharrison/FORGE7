#include "AudioHealthMonitor.h"

#include "../Audio/AudioEngine.h"

namespace forge7
{

AudioHealthMonitor::AudioHealthMonitor(AudioEngine* engine)
    : audioEngine(engine)
{
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setFont(juce::Font(13.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc5c8ce));
    statusLabel.setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(statusLabel);

    startTimerHz(4);
}

AudioHealthMonitor::~AudioHealthMonitor() = default;

void AudioHealthMonitor::resized()
{
    statusLabel.setBounds(getLocalBounds().reduced(4, 2));
}

void AudioHealthMonitor::timerCallback()
{
    if (audioEngine == nullptr)
    {
        statusLabel.setText("Audio: not connected", juce::dontSendNotification);
        return;
    }

    const uint64_t total = audioEngine->getAudioCallbackInvocationCount();
    const uint64_t delta = total - previousCallbackTotal;
    previousCallbackTotal = total;

    juce::String line = "RT callbacks (est. / ~250 ms): " + juce::String(static_cast<juce::int64>(delta));

    if (delta == 0 && total > 0)
        line += " — possible stall";

    statusLabel.setText(line, juce::dontSendNotification);
}

} // namespace forge7
