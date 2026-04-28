#include "CpuMeter.h"

#include "../Audio/AudioEngine.h"

namespace forge7
{

CpuMeter::CpuMeter(AudioEngine* engine)
    : audioEngine(engine)
{
    startTimerHz(12);
}

CpuMeter::~CpuMeter()
{
    stopTimer();
}

void CpuMeter::timerCallback()
{
    if (audioEngine != nullptr)
    {
        const double p = audioEngine->getApproximateCpuUsage();
        cpuLabel = "CPU " + juce::String(juce::jlimit(0.0, 100.0, p * 100.0), 1) + "%";
    }
    else
    {
        cpuLabel = "CPU -%";
    }

    repaint();
}

void CpuMeter::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkslategrey);
    g.setColour(juce::Colours::lightgreen);
    g.drawText(cpuLabel, getLocalBounds().reduced(4), juce::Justification::centredLeft);
}

} // namespace forge7
