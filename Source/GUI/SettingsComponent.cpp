#include "SettingsComponent.h"

#include <fstream>

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
    else if (inGainV <= 0.0001f)
        warnText += "Input gain is zero inside the app.";
    else if (inPeak <= 0.0001f && rawPk <= 0.0000001f)
        warnText += "The driver is delivering silence on all enabled input channels (raw peak 0). Check interface "
                    "preamp gain, instrument vs line, cable, and macOS Microphone access for this app.";
    else if (inPeak <= 0.0001f)
        warnText += "Input channel present, but signal is silent after gain. If your guitar is on input 2, FORGE7 "
                    "sums all enabled inputs; check macOS Microphone permission and interface gain.";

    inputWarningLabel.setText(warnText.trimEnd(), juce::dontSendNotification);

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

