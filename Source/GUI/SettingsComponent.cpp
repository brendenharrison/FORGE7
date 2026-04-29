#include "SettingsComponent.h"

#include <fstream>

#include "../App/AppConfig.h"
#include "../App/AppContext.h"
#include "../Audio/AudioEngine.h"
#include "../GUI/LevelMeter.h"
#include "../Platform/MacAudioPermission.h"
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
juce::Colour warn() noexcept { return juce::Colour(0xffffb454); }

// #region agent log
void debugNdjsonAppendSettings(const juce::String& location,
                               const juce::String& message,
                               const juce::var& data,
                               const juce::String& hypothesisId)
{
    static const juce::String kPath { "/Users/brendenharrison/Forge_7/.cursor/debug-af0f62.log" };
    juce::DynamicObject::Ptr obj { new juce::DynamicObject() };
    obj->setProperty("sessionId", "af0f62");
    obj->setProperty("timestamp", (juce::int64) juce::Time::getCurrentTime().toMilliseconds());
    obj->setProperty("location", location);
    obj->setProperty("message", message);
    obj->setProperty("data", data);
    obj->setProperty("hypothesisId", hypothesisId);
    const juce::String line = juce::JSON::toString(juce::var(obj.get()), true) + "\n";
    std::ofstream f(kPath.toStdString(), std::ios::app);
    if (f.is_open())
        f << line.toStdString();
}
// #endregion

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

juce::String macMicStatusDisplayString(MacMicPermissionStatus s)
{
    switch (s)
    {
        case MacMicPermissionStatus::NotMac:
            return "Not Mac";
        case MacMicPermissionStatus::Unknown:
            return "Unknown";
        case MacMicPermissionStatus::NotDetermined:
            return "Not Determined";
        case MacMicPermissionStatus::Denied:
            return "Denied";
        case MacMicPermissionStatus::Restricted:
            return "Restricted";
        case MacMicPermissionStatus::Authorized:
            return "Authorized";
        default:
            return "Unknown";
    }
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

    styleTopButton(requestMicPermissionButton);
    requestMicPermissionButton.onClick = [this]()
    {
        forge7::requestMacMicPermission([](bool granted)
                                        {
                                            Logger::info("FORGE7 AudioIO: requestMacMicPermission completed, granted="
                                                         + juce::String(granted ? "yes" : "no"));
                                        });
    };
    addAndMakeVisible(requestMicPermissionButton);

    styleTopButton(checkMicPermissionButton);
    checkMicPermissionButton.onClick = [this]() { refreshStatus(); };
    addAndMakeVisible(checkMicPermissionButton);

    styleTopButton(runInputProbeButton);
    runInputProbeButton.onClick = [this]()
    {
        if (appContext.audioEngine == nullptr)
            return;
        appContext.audioEngine->setInputProbeEnabled(true);
        inputProbeToggle.setToggleState(true, juce::dontSendNotification);
        Logger::info("FORGE7 AudioIO: Run Input Probe - input probe mode enabled");
        refreshStatus();
    };
    addAndMakeVisible(runInputProbeButton);

    inputProbeToggle.setColour(juce::ToggleButton::textColourId, text());
    inputProbeToggle.setColour(juce::ToggleButton::tickColourId, accent());
    inputProbeToggle.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    inputProbeToggle.setClickingTogglesState(true);
    if (appContext.audioEngine != nullptr)
        inputProbeToggle.setToggleState(appContext.audioEngine->isInputProbeEnabled(), juce::dontSendNotification);
    inputProbeToggle.onClick = [this]()
    {
        if (appContext.audioEngine == nullptr)
            return;
        const bool on = inputProbeToggle.getToggleState();
        appContext.audioEngine->setInputProbeEnabled(on);
        Logger::info("FORGE7 AudioIO: input probe " + juce::String(on ? "enabled" : "disabled"));
        refreshStatus();
    };
    addAndMakeVisible(inputProbeToggle);

    inputSourceHeadingLabel.setText("Input Source:", juce::dontSendNotification);
    styleMuted(inputSourceHeadingLabel, 12.0f);
    addAndMakeVisible(inputSourceHeadingLabel);

    inputSourceCombo.addItem("First non-null", 1);
    inputSourceCombo.addItem("Channel 1", 2);
    inputSourceCombo.addItem("Channel 2", 3);
    inputSourceCombo.addItem("Mix all", 4);
    inputSourceCombo.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    if (appContext.audioEngine != nullptr)
    {
        const auto m = appContext.audioEngine->getInputSourceMode();
        inputSourceCombo.setSelectedId(static_cast<int>(m) + 1, juce::dontSendNotification);
    }
    else
        inputSourceCombo.setSelectedId(2, juce::dontSendNotification);

    inputSourceCombo.onChange = [this]()
    {
        if (appContext.audioEngine == nullptr)
            return;
        const int id = inputSourceCombo.getSelectedId();
        if (id <= 0)
            return;
        appContext.audioEngine->setInputSourceMode(static_cast<InputSourceMode>(id - 1));
        Logger::info("FORGE7 AudioIO: input source mode set to id=" + juce::String(id));
        refreshStatus();
    };
    addAndMakeVisible(inputSourceCombo);

    styleMuted(micPermissionStatusLabel, 12.0f);
    addAndMakeVisible(micPermissionStatusLabel);

    micDeniedHintLabel.setJustificationType(juce::Justification::topLeft);
    micDeniedHintLabel.setFont(juce::Font(11.0f));
    micDeniedHintLabel.setColour(juce::Label::textColourId, warn());
    micDeniedHintLabel.setVisible(false);
    addAndMakeVisible(micDeniedHintLabel);

    for (auto* l : { &ch1RawPeakLabel, &ch2RawPeakLabel, &ch3RawPeakLabel, &ch4RawPeakLabel })
    {
        styleMuted(*l, 12.0f);
        addAndMakeVisible(*l);
    }

    diagnosisLabel.setJustificationType(juce::Justification::topLeft);
    diagnosisLabel.setFont(juce::Font(12.0f));
    diagnosisLabel.setColour(juce::Label::textColourId, text());
    addAndMakeVisible(diagnosisLabel);

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
                     &callbackDeviceLabel,
                     &inputChannelsLabel,
                     &outputChannelsLabel,
                     &callbackChannelsLabel,
                     &selectedInputLabel,
                     &mixedInputLabel,
                     &inputGainLabel,
                     &rawInputPeakLabel,
                     &inputPresentLabel,
                     &sampleRateLabel,
                     &bufferSizeLabel,
                     &cpuLabel,
                     &callbackCountLabel })
    {
        styleMuted(*l, 12.0f);
        addAndMakeVisible(*l);
    }

    inputWarningLabel.setJustificationType(juce::Justification::topLeft);
    inputWarningLabel.setFont(juce::Font(11.0f));
    inputWarningLabel.setColour(juce::Label::textColourId, warn());
    addAndMakeVisible(inputWarningLabel);

    inputMonitorToggle.setColour(juce::ToggleButton::textColourId, text());
    inputMonitorToggle.setColour(juce::ToggleButton::tickColourId, accent());
    inputMonitorToggle.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    inputMonitorToggle.setClickingTogglesState(true);
    if (appContext.audioEngine != nullptr)
        inputMonitorToggle.setToggleState(appContext.audioEngine->isInputMonitorEnabled(), juce::dontSendNotification);
    inputMonitorToggle.onClick = [this]()
    {
        if (appContext.audioEngine == nullptr)
            return;
        const bool enabled = inputMonitorToggle.getToggleState();
        appContext.audioEngine->setInputMonitorEnabled(enabled);
        Logger::info("FORGE7 AudioIO: input monitor " + juce::String(enabled ? "enabled" : "disabled"));
        // #region agent log
        juce::DynamicObject::Ptr d { new juce::DynamicObject() };
        d->setProperty("enabled", enabled);
        debugNdjsonAppendSettings("SettingsComponent.cpp:inputMonitorToggle", "input monitor toggled",
                                  juce::var(d.get()), "H1,H3");
        // #endregion
    };
    addAndMakeVisible(inputMonitorToggle);

    monitorHintLabel.setText("Input Monitor verifies live input routing. Test Tone verifies output only.",
                             juce::dontSendNotification);
    monitorHintLabel.setJustificationType(juce::Justification::topLeft);
    monitorHintLabel.setFont(juce::Font(12.0f));
    monitorHintLabel.setColour(juce::Label::textColourId, muted());
    addAndMakeVisible(monitorHintLabel);

    testToneHintLabel.setText("Test Tone verifies output only.", juce::dontSendNotification);
    testToneHintLabel.setJustificationType(juce::Justification::topLeft);
    testToneHintLabel.setFont(juce::Font(12.0f));
    testToneHintLabel.setColour(juce::Label::textColourId, muted());
    addAndMakeVisible(testToneHintLabel);

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

    area.removeFromTop(8);

    auto diagArea = area.removeFromTop(254);
    auto micRow = diagArea.removeFromTop(30);
    requestMicPermissionButton.setBounds(micRow.removeFromLeft(168).reduced(0, 2));
    micRow.removeFromLeft(8);
    checkMicPermissionButton.setBounds(micRow.removeFromLeft(152).reduced(0, 2));
    micRow.removeFromLeft(8);
    runInputProbeButton.setBounds(micRow.removeFromLeft(130).reduced(0, 2));

    diagArea.removeFromTop(6);
    auto probeRow = diagArea.removeFromTop(28);
    inputProbeToggle.setBounds(probeRow.removeFromLeft(240));

    diagArea.removeFromTop(6);
    auto srcRow = diagArea.removeFromTop(28);
    inputSourceHeadingLabel.setBounds(srcRow.removeFromLeft(100));
    srcRow.removeFromLeft(8);
    inputSourceCombo.setBounds(srcRow.removeFromLeft(220).reduced(0, 2));

    diagArea.removeFromTop(6);
    micPermissionStatusLabel.setBounds(diagArea.removeFromTop(18));

    diagArea.removeFromTop(4);
    micDeniedHintLabel.setBounds(diagArea.removeFromTop(44));

    diagArea.removeFromTop(4);
    auto chRow = diagArea.removeFromTop(18);
    const int chW = juce::jmax(80, chRow.getWidth() / 4);
    ch1RawPeakLabel.setBounds(chRow.removeFromLeft(chW));
    ch2RawPeakLabel.setBounds(chRow.removeFromLeft(chW));
    ch3RawPeakLabel.setBounds(chRow.removeFromLeft(chW));
    ch4RawPeakLabel.setBounds(chRow);

    diagArea.removeFromTop(6);
    diagnosisLabel.setBounds(diagArea);

    area.removeFromTop(8);

    auto monitorRow = area.removeFromTop(28);
    inputMonitorToggle.setBounds(monitorRow.removeFromLeft(180));
    monitorRow.removeFromLeft(10);
    monitorHintLabel.setBounds(monitorRow);

    area.removeFromTop(2);
    testToneHintLabel.setBounds(area.removeFromTop(18));

    area.removeFromTop(10);

    // Right column: status + meters.
    const int rightW = juce::jlimit(280, 400, area.getWidth() / 3);
    auto right = area.removeFromRight(rightW);
    right.removeFromLeft(12);

    statusHeading.setBounds(right.removeFromTop(20));
    right.removeFromTop(4);

    const int rowH = 18;
    deviceTypeLabel.setBounds(right.removeFromTop(rowH));
    inputDeviceLabel.setBounds(right.removeFromTop(rowH));
    outputDeviceLabel.setBounds(right.removeFromTop(rowH));
    callbackDeviceLabel.setBounds(right.removeFromTop(rowH));
    inputChannelsLabel.setBounds(right.removeFromTop(rowH));
    outputChannelsLabel.setBounds(right.removeFromTop(rowH));
    callbackChannelsLabel.setBounds(right.removeFromTop(rowH));
    selectedInputLabel.setBounds(right.removeFromTop(rowH));
    mixedInputLabel.setBounds(right.removeFromTop(rowH));
    inputGainLabel.setBounds(right.removeFromTop(rowH));
    rawInputPeakLabel.setBounds(right.removeFromTop(rowH));
    inputPresentLabel.setBounds(right.removeFromTop(rowH));
    sampleRateLabel.setBounds(right.removeFromTop(rowH));
    bufferSizeLabel.setBounds(right.removeFromTop(rowH));
    cpuLabel.setBounds(right.removeFromTop(rowH));
    callbackCountLabel.setBounds(right.removeFromTop(rowH));

    right.removeFromTop(6);
    inputWarningLabel.setBounds(right.removeFromTop(72));

    right.removeFromTop(6);

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
        inputDeviceLabel.setText("Input device: -", juce::dontSendNotification);
        outputDeviceLabel.setText("Output device: -", juce::dontSendNotification);
        callbackDeviceLabel.setText("Callback device: -", juce::dontSendNotification);
        inputChannelsLabel.setText("Input channels: -", juce::dontSendNotification);
        outputChannelsLabel.setText("Output channels: -", juce::dontSendNotification);
        callbackChannelsLabel.setText("Callback channels: -", juce::dontSendNotification);
        selectedInputLabel.setText("Selected callback input: -", juce::dontSendNotification);
        mixedInputLabel.setText("Inputs mixed to mono: -", juce::dontSendNotification);
        inputGainLabel.setText("Input gain (linear): -", juce::dontSendNotification);
        rawInputPeakLabel.setText("Raw peak (pre-gain): -", juce::dontSendNotification);
        inputPresentLabel.setText("Input present: -", juce::dontSendNotification);
        inputWarningLabel.setText("", juce::dontSendNotification);
        sampleRateLabel.setText("Sample rate: -", juce::dontSendNotification);
        bufferSizeLabel.setText("Buffer: -", juce::dontSendNotification);
        cpuLabel.setText("CPU: -", juce::dontSendNotification);
        callbackCountLabel.setText("Callbacks: -", juce::dontSendNotification);
        micPermissionStatusLabel.setText("Mic permission status: -", juce::dontSendNotification);
        micDeniedHintLabel.setVisible(false);
        ch1RawPeakLabel.setText("Ch 1 raw peak: -", juce::dontSendNotification);
        ch2RawPeakLabel.setText("Ch 2 raw peak: -", juce::dontSendNotification);
        ch3RawPeakLabel.setText("Ch 3 raw peak: -", juce::dontSendNotification);
        ch4RawPeakLabel.setText("Ch 4 raw peak: -", juce::dontSendNotification);
        diagnosisLabel.setText("", juce::dontSendNotification);
        return;
    }

    auto* engine = appContext.audioEngine;
    auto& dm = engine->getDeviceManager();

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    dm.getAudioDeviceSetup(setup);

    const juce::String dtype = dm.getCurrentAudioDeviceType();

    deviceTypeLabel.setText("Device type: " + (dtype.isNotEmpty() ? dtype : juce::String("-")), juce::dontSendNotification);
    inputDeviceLabel.setText("Input device: " + (setup.inputDeviceName.isNotEmpty() ? setup.inputDeviceName : juce::String("-")),
                             juce::dontSendNotification);
    outputDeviceLabel.setText("Output device: " + (setup.outputDeviceName.isNotEmpty() ? setup.outputDeviceName : juce::String("-")),
                              juce::dontSendNotification);

    if (auto* dev = dm.getCurrentAudioDevice())
        callbackDeviceLabel.setText("Callback device: " + dev->getName(), juce::dontSendNotification);
    else
        callbackDeviceLabel.setText("Callback device: -", juce::dontSendNotification);

    const int inEnabled = setup.inputChannels.countNumberOfSetBits();
    const int outEnabled = setup.outputChannels.countNumberOfSetBits();

    inputChannelsLabel.setText("Input channels: " + juce::String(inEnabled) + " (mask " + setup.inputChannels.toString(2) + ")",
                               juce::dontSendNotification);
    outputChannelsLabel.setText("Output channels: " + juce::String(outEnabled) + " (mask " + setup.outputChannels.toString(2) + ")",
                                juce::dontSendNotification);

    const int cbIn = engine->getLastNumInputChannels();
    const int cbOut = engine->getLastNumOutputChannels();
    const int selIn = engine->getLastSelectedInputChannel();
    const int mixN = engine->getLastMixedInputChannelCount();
    const bool inputPresent = engine->isInputPresentInCallback();

    callbackChannelsLabel.setText("Callback channels: in=" + juce::String(cbIn) + " out=" + juce::String(cbOut),
                                  juce::dontSendNotification);
    selectedInputLabel.setText("Selected callback input: " + (selIn >= 0 ? juce::String(selIn) : juce::String("-")),
                               juce::dontSendNotification);
    mixedInputLabel.setText("Inputs mixed to mono: " + juce::String(mixN), juce::dontSendNotification);
    inputPresentLabel.setText(juce::String("Input present: ") + (inputPresent ? "yes" : "no"),
                              juce::dontSendNotification);

    const float inPeak = engine->getSmoothedInputPeak();
    const float rawPk = engine->getLastInputPeakRaw();
    const float inGainV = engine->getInputGainLinear();

    inputGainLabel.setText("Input gain (linear): " + juce::String(inGainV, 3), juce::dontSendNotification);
    rawInputPeakLabel.setText("Raw peak (pre-gain): " + juce::String(rawPk, 6), juce::dontSendNotification);

    const float ch1r = engine->getInputChannelRawPeak(0);
    const float ch2r = engine->getInputChannelRawPeak(1);
    const float ch3r = engine->getInputChannelRawPeak(2);
    const float ch4r = engine->getInputChannelRawPeak(3);
    ch1RawPeakLabel.setText("Ch 1 raw peak: " + juce::String(ch1r, 6), juce::dontSendNotification);
    ch2RawPeakLabel.setText("Ch 2 raw peak: " + juce::String(ch2r, 6), juce::dontSendNotification);
    ch3RawPeakLabel.setText("Ch 3 raw peak: " + juce::String(ch3r, 6), juce::dontSendNotification);
    ch4RawPeakLabel.setText("Ch 4 raw peak: " + juce::String(ch4r, 6), juce::dontSendNotification);

    const auto micStatus = forge7::getMacMicPermissionStatus();
    micPermissionStatusLabel.setText("Mic permission status: " + macMicStatusDisplayString(micStatus),
                                     juce::dontSendNotification);

    const bool isMac = (juce::SystemStats::getOperatingSystemType() == juce::SystemStats::MacOSX);
    const bool micDeniedOnMac = isMac && micStatus == MacMicPermissionStatus::Denied;
    micDeniedHintLabel.setVisible(micDeniedOnMac);
    if (micDeniedOnMac)
        micDeniedHintLabel.setText("Mic permission denied. Enable FORGE 7 in System Settings > Privacy & Security > Microphone.",
                                   juce::dontSendNotification);
    else
        micDeniedHintLabel.setText("", juce::dontSendNotification);

    juce::String warnText;
    if (auto* dev = dm.getCurrentAudioDevice())
    {
        const bool splitIo = setup.inputDeviceName.isNotEmpty() && setup.outputDeviceName.isNotEmpty()
                             && ! setup.inputDeviceName.equalsIgnoreCase(setup.outputDeviceName);
        if (splitIo && ! dev->getName().equalsIgnoreCase(setup.inputDeviceName))
        {
            warnText += "The callback stream uses \"" + dev->getName()
                        + "\", not your selected input \"" + setup.inputDeviceName
                        + "\". On macOS, two different devices require an Aggregate Device in Audio MIDI Setup, "
                        + "or use one interface for both input and output.\n\n";
        }
    }

    if (! inputPresent)
        warnText += "No live input channel is reaching the audio callback.";
    else if (inGainV <= 0.0001f && ! engine->isInputProbeEnabled())
        warnText += "Input gain is zero inside the app.";
    else if (inPeak <= 0.0001f && rawPk <= 0.0000001f && ! engine->isInputProbeEnabled())
        warnText += "The driver is delivering silence on all enabled input channels (raw peak 0). Check interface "
                    "preamp gain, instrument vs line, cable, and macOS Microphone access for this app.";
    else if (inPeak <= 0.0001f && ! engine->isInputProbeEnabled())
        warnText += "Input channel present, but signal is silent after gain. If your guitar is on input 2, try Input Source "
                    "Channel 2 or Mix all; check macOS Microphone permission and interface gain.";

    inputWarningLabel.setText(warnText.trimEnd(), juce::dontSendNotification);

    juce::String diagnosis;
    constexpr float kPeakEpsilon = 1.0e-8f;
    const bool allFourRawPeaksZero = (ch1r <= kPeakEpsilon && ch2r <= kPeakEpsilon && ch3r <= kPeakEpsilon
                                      && ch4r <= kPeakEpsilon);

    if (isMac && micStatus != MacMicPermissionStatus::Authorized)
        diagnosis = "Diagnosis: macOS microphone permission is not authorized.";
    else if (cbIn == 0)
        diagnosis = "Diagnosis: JUCE callback is receiving zero input channels.";
    else if (! inputPresent)
        diagnosis = "Diagnosis: Input channel pointers are null.";
    else if (allFourRawPeaksZero)
        diagnosis = "Diagnosis: Input stream is open, but CoreAudio is delivering silence.";
    else
        diagnosis = "Diagnosis: Input signal is reaching FORGE7.";

    diagnosisLabel.setText(diagnosis, juce::dontSendNotification);

    const int sourceComboId = static_cast<int>(engine->getInputSourceMode()) + 1;
    if (inputSourceCombo.getSelectedId() != sourceComboId)
        inputSourceCombo.setSelectedId(sourceComboId, juce::dontSendNotification);

    const bool probeOn = engine->isInputProbeEnabled();
    inputProbeToggle.setToggleState(probeOn, juce::dontSendNotification);
    inputProbeToggle.setButtonText(probeOn ? "Input Probe: ON" : "Input Probe: OFF");

    sampleRateLabel.setText("Sample rate: " + juce::String(engine->getCurrentSampleRate(), 1) + " Hz",
                            juce::dontSendNotification);
    bufferSizeLabel.setText("Buffer: " + juce::String(engine->getCurrentBufferSize()) + " frames",
                            juce::dontSendNotification);

    const double cpu = engine->getApproximateCpuUsage();
    cpuLabel.setText("CPU: " + juce::String(cpu * 100.0, 1) + "%", juce::dontSendNotification);

    callbackCountLabel.setText("Callbacks: " + juce::String((juce::int64) engine->getAudioCallbackInvocationCount()),
                               juce::dontSendNotification);

    inputMonitorToggle.setToggleState(engine->isInputMonitorEnabled(), juce::dontSendNotification);

    // #region agent log
    // Throttle: timer is 8 Hz; log a snapshot every 16 ticks (~2 s) to keep noise low.
    if ((diagnosticTickCount++ % 16) == 0)
    {
        juce::DynamicObject::Ptr d { new juce::DynamicObject() };
        d->setProperty("inputDeviceName", setup.inputDeviceName);
        d->setProperty("outputDeviceName", setup.outputDeviceName);
        d->setProperty("inputChannelsMask", setup.inputChannels.toString(2));
        d->setProperty("outputChannelsMask", setup.outputChannels.toString(2));
        d->setProperty("inputChannelsBitCount", inEnabled);
        d->setProperty("outputChannelsBitCount", outEnabled);
        d->setProperty("callbackInputChannels", cbIn);
        d->setProperty("callbackOutputChannels", cbOut);
        d->setProperty("selectedInputChannel", selIn);
        d->setProperty("mixedInputChannelCount", mixN);
        if (auto* dev = dm.getCurrentAudioDevice())
            d->setProperty("callbackDeviceName", dev->getName());
        d->setProperty("inputPresent", inputPresent);
        d->setProperty("inputGainLinear", engine->getInputGainLinear());
        d->setProperty("inputPeakRaw", rawPk);
        d->setProperty("inputPeak", inPeak);
        d->setProperty("outputPeak", engine->getSmoothedOutputPeak());
        d->setProperty("monitorEnabled", engine->isInputMonitorEnabled());
        d->setProperty("inputProbe", engine->isInputProbeEnabled());
        d->setProperty("inputSourceMode", (int) engine->getInputSourceMode());
        d->setProperty("ch1RawPeak", ch1r);
        d->setProperty("ch2RawPeak", ch2r);
        d->setProperty("macMicPermission", (int) micStatus);
        d->setProperty("globalBypass", engine->isGlobalBypass());
        d->setProperty("callbackInvocationCount",
                       (juce::int64) engine->getAudioCallbackInvocationCount());
        debugNdjsonAppendSettings("SettingsComponent.cpp:refreshStatus", "live diagnostics snapshot",
                                  juce::var(d.get()), "H8");
    }
    // #endregion
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

    Logger::info("FORGE7 AudioIO: saved audio device state"
                 + juce::String(" | type=") + dm.getCurrentAudioDeviceType()
                 + " | inputDevice=\"" + setup.inputDeviceName + "\""
                 + " | outputDevice=\"" + setup.outputDeviceName + "\""
                 + " | inMask=" + setup.inputChannels.toString(2)
                 + " | outMask=" + setup.outputChannels.toString(2)
                 + " | sr=" + juce::String(setup.sampleRate, 1)
                 + " | block=" + juce::String(setup.bufferSize)
                 + " | xmlLen=" + juce::String(xmlText.length()));

    // #region agent log
    juce::DynamicObject::Ptr d { new juce::DynamicObject() };
    d->setProperty("inputDeviceName", setup.inputDeviceName);
    d->setProperty("outputDeviceName", setup.outputDeviceName);
    d->setProperty("inputChannelsMask", setup.inputChannels.toString(2));
    d->setProperty("outputChannelsMask", setup.outputChannels.toString(2));
    d->setProperty("sampleRate", setup.sampleRate);
    d->setProperty("bufferSize", setup.bufferSize);
    d->setProperty("xmlBytes", xmlText.length());
    d->setProperty("ok", ok);
    debugNdjsonAppendSettings("SettingsComponent.cpp:saveAudioSettings", "user saved audio settings",
                              juce::var(d.get()), "H2,H5");
    // #endregion
}

} // namespace forge7

