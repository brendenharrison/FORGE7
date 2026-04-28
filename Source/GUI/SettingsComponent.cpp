#include "SettingsComponent.h"

#include "../App/AppConfig.h"
#include "../App/AppContext.h"
#include "../Audio/AudioEngine.h"
#include "../GUI/LevelMeter.h"
#include "../Utilities/Logger.h"

namespace forge7
{
namespace
{
juce::Colour bg() noexcept { return juce::Colour(0xff0d0f12); }
juce::Colour panel() noexcept { return juce::Colour(0xff161a20); }
juce::Colour text() noexcept { return juce::Colour(0xffe8eaed); }
juce::Colour muted() noexcept { return juce::Colour(0xff8a9099); }
juce::Colour accent() noexcept { return juce::Colour(0xff4a9eff); }

void styleTopButton(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId, panel().brighter(0.08f));
    b.setColour(juce::TextButton::textColourOffId, text());
    b.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void styleHeading(juce::Label& l, const float fontH)
{
    l.setJustificationType(juce::Justification::centredLeft);
    l.setFont(juce::Font(fontH));
    l.setColour(juce::Label::textColourId, text());
}

void styleMuted(juce::Label& l, const float fontH)
{
    l.setJustificationType(juce::Justification::centredLeft);
    l.setFont(juce::Font(fontH));
    l.setColour(juce::Label::textColourId, muted());
}
} // namespace

SettingsComponent::SettingsComponent(AppContext& context, std::function<void()> onBack)
    : appContext(context),
      onBackPressed(std::move(onBack))
{
    setOpaque(true);

    styleTopButton(backButton);
    backButton.onClick = [this]()
    {
        if (onBackPressed != nullptr)
            onBackPressed();
    };
    addAndMakeVisible(backButton);

    titleLabel.setText("Settings", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(18.0f));
    titleLabel.setColour(juce::Label::textColourId, text());
    addAndMakeVisible(titleLabel);

    sectionAudioLabel.setText("Audio I/O", juce::dontSendNotification);
    sectionAudioLabel.setJustificationType(juce::Justification::centredLeft);
    sectionAudioLabel.setFont(juce::Font(16.0f));
    sectionAudioLabel.setColour(juce::Label::textColourId, accent());
    addAndMakeVisible(sectionAudioLabel);

    monoRoutingHintLabel.setText(
        "FORGE7 currently uses the first enabled input as mono guitar input and outputs stereo to the first two enabled output channels.",
        juce::dontSendNotification);
    monoRoutingHintLabel.setJustificationType(juce::Justification::topLeft);
    monoRoutingHintLabel.setFont(juce::Font(12.0f));
    monoRoutingHintLabel.setColour(juce::Label::textColourId, muted());
    addAndMakeVisible(monoRoutingHintLabel);

    styleTopButton(saveAudioButton);
    saveAudioButton.onClick = [this]() { saveAudioSettings(); };
    addAndMakeVisible(saveAudioButton);

    saveStatusLabel.setJustificationType(juce::Justification::centredLeft);
    saveStatusLabel.setFont(juce::Font(12.0f));
    saveStatusLabel.setColour(juce::Label::textColourId, muted());
    addAndMakeVisible(saveStatusLabel);

    if (appContext.audioEngine != nullptr)
    {
        auto& dm = appContext.audioEngine->getDeviceManager();

        deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
            dm,
            1,
            8,
            2,
            8,
            false,
            false,
            true,
            false);

        deviceSelectorViewport = std::make_unique<juce::Viewport>();
        deviceSelectorViewport->setScrollBarsShown(true, false);
        deviceSelectorViewport->setViewedComponent(deviceSelector.get(), false);
        addAndMakeVisible(*deviceSelectorViewport);

        inputMeter = std::make_unique<LevelMeter>(appContext.audioEngine, MeterChannel::inputAfterGain);
        outputMeter = std::make_unique<LevelMeter>(appContext.audioEngine, MeterChannel::outputAfterGain);
        addAndMakeVisible(*inputMeter);
        addAndMakeVisible(*outputMeter);
    }

    statusHeading.setText("Status", juce::dontSendNotification);
    styleHeading(statusHeading, 14.0f);
    addAndMakeVisible(statusHeading);

    for (auto* l : { &deviceTypeLabel,
                     &inputDeviceLabel,
                     &outputDeviceLabel,
                     &sampleRateLabel,
                     &bufferSizeLabel,
                     &cpuLabel,
                     &callbackCountLabel })
    {
        styleMuted(*l, 12.0f);
        addAndMakeVisible(*l);
    }

    meterHintLabel.setText("Play guitar or send signal into the selected input. Input meter should move.", juce::dontSendNotification);
    meterHintLabel.setJustificationType(juce::Justification::topLeft);
    meterHintLabel.setFont(juce::Font(12.0f));
    meterHintLabel.setColour(juce::Label::textColourId, muted());
    addAndMakeVisible(meterHintLabel);

    refreshStatus();
    startTimerHz(8);
}

SettingsComponent::~SettingsComponent()
{
    stopTimer();
}

void SettingsComponent::paint(juce::Graphics& g)
{
    g.fillAll(bg());

    // Subtle panel behind the device selector + status.
    g.setColour(panel());
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f), 10.0f);
}

void SettingsComponent::resized()
{
    auto area = getLocalBounds().reduced(14, 12);

    const int topH = 46;
    auto top = area.removeFromTop(topH);

    backButton.setBounds(top.removeFromLeft(100).reduced(0, 6));
    titleLabel.setBounds(top);

    area.removeFromTop(8);

    auto audioHeader = area.removeFromTop(26);
    sectionAudioLabel.setBounds(audioHeader);

    area.removeFromTop(6);
    monoRoutingHintLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(8);

    auto saveRow = area.removeFromTop(34);
    saveAudioButton.setBounds(saveRow.removeFromLeft(170).reduced(0, 2));
    saveRow.removeFromLeft(10);
    saveStatusLabel.setBounds(saveRow);

    area.removeFromTop(10);

    // Right column: status + meters.
    const int rightW = juce::jlimit(240, 360, area.getWidth() / 3);
    auto right = area.removeFromRight(rightW);
    right.removeFromLeft(12);

    statusHeading.setBounds(right.removeFromTop(20));
    right.removeFromTop(4);

    const int rowH = 18;
    deviceTypeLabel.setBounds(right.removeFromTop(rowH));
    inputDeviceLabel.setBounds(right.removeFromTop(rowH));
    outputDeviceLabel.setBounds(right.removeFromTop(rowH));
    sampleRateLabel.setBounds(right.removeFromTop(rowH));
    bufferSizeLabel.setBounds(right.removeFromTop(rowH));
    cpuLabel.setBounds(right.removeFromTop(rowH));
    callbackCountLabel.setBounds(right.removeFromTop(rowH));

    right.removeFromTop(10);

    const int meterH = 20;
    if (inputMeter != nullptr)
        inputMeter->setBounds(right.removeFromTop(meterH));
    right.removeFromTop(6);
    if (outputMeter != nullptr)
        outputMeter->setBounds(right.removeFromTop(meterH));

    right.removeFromTop(8);
    meterHintLabel.setBounds(right);

    // Left column: AudioDeviceSelectorComponent (scrollable).
    if (deviceSelectorViewport != nullptr && deviceSelector != nullptr)
    {
        deviceSelectorViewport->setBounds(area);
        const int contentW = juce::jmax(360, deviceSelectorViewport->getWidth() - 18);
        const int contentH = juce::jmax(520, deviceSelectorViewport->getHeight());
        deviceSelector->setSize(contentW, contentH);
    }
}

void SettingsComponent::timerCallback()
{
    refreshStatus();
    if (inputMeter != nullptr)
        inputMeter->repaint();
    if (outputMeter != nullptr)
        outputMeter->repaint();
}

void SettingsComponent::refreshStatus()
{
    if (appContext.audioEngine == nullptr)
    {
        deviceTypeLabel.setText("Device type: -", juce::dontSendNotification);
        inputDeviceLabel.setText("Input: -", juce::dontSendNotification);
        outputDeviceLabel.setText("Output: -", juce::dontSendNotification);
        sampleRateLabel.setText("Sample rate: -", juce::dontSendNotification);
        bufferSizeLabel.setText("Buffer: -", juce::dontSendNotification);
        cpuLabel.setText("CPU: -", juce::dontSendNotification);
        callbackCountLabel.setText("Callbacks: -", juce::dontSendNotification);
        return;
    }

    auto& dm = appContext.audioEngine->getDeviceManager();
    auto* dev = dm.getCurrentAudioDevice();

    const juce::String dtype = dm.getCurrentAudioDeviceType();
    const juce::String dname = dev != nullptr ? dev->getName() : juce::String("-");

    deviceTypeLabel.setText("Device type: " + (dtype.isNotEmpty() ? dtype : juce::String("-")), juce::dontSendNotification);
    inputDeviceLabel.setText("Input: " + dname, juce::dontSendNotification);
    outputDeviceLabel.setText("Output: " + dname, juce::dontSendNotification);

    sampleRateLabel.setText("Sample rate: " + juce::String(appContext.audioEngine->getCurrentSampleRate(), 1) + " Hz",
                            juce::dontSendNotification);
    bufferSizeLabel.setText("Buffer: " + juce::String(appContext.audioEngine->getCurrentBufferSize()) + " frames",
                            juce::dontSendNotification);

    const double cpu = appContext.audioEngine->getApproximateCpuUsage();
    cpuLabel.setText("CPU: " + juce::String(cpu * 100.0, 1) + "%", juce::dontSendNotification);

    callbackCountLabel.setText("Callbacks: " + juce::String((juce::int64)appContext.audioEngine->getAudioCallbackInvocationCount()),
                               juce::dontSendNotification);
}

void SettingsComponent::saveAudioSettings()
{
    if (appContext.audioEngine == nullptr || appContext.appConfig == nullptr)
        return;

    auto& dm = appContext.audioEngine->getDeviceManager();
    std::unique_ptr<juce::XmlElement> xml(dm.createStateXml());

    if (xml == nullptr)
    {
        saveStatusLabel.setText("Could not serialize audio device state.", juce::dontSendNotification);
        return;
    }

    const juce::String xmlText = xml->toString();
    appContext.appConfig->setAudioDeviceStateXml(xmlText);

    // Also persist a few human-readable fields for debugging.
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    dm.getAudioDeviceSetup(setup);

    if (auto* o = appContext.appConfig->getSettings().getDynamicObject())
    {
        o->setProperty("audioDeviceType", dm.getCurrentAudioDeviceType());
        o->setProperty("audioDeviceName", setup.outputDeviceName.isNotEmpty() ? setup.outputDeviceName : setup.inputDeviceName);
        o->setProperty("audioInputDeviceName", setup.inputDeviceName);
        o->setProperty("audioOutputDeviceName", setup.outputDeviceName);
        o->setProperty("sampleRate", setup.sampleRate);
        o->setProperty("bufferSize", setup.bufferSize);
        o->setProperty("audioInputChannelsMask", (int)setup.inputChannels.toInteger());
        o->setProperty("audioOutputChannelsMask", (int)setup.outputChannels.toInteger());
    }

    const bool ok = appContext.appConfig->saveToFile();
    saveStatusLabel.setText(ok ? "Saved." : "Save failed (check permissions).", juce::dontSendNotification);

    Logger::info("FORGE7 AudioIO: saved audio device state (xmlLen=" + juce::String(xmlText.length()) + ")");
}

} // namespace forge7

