#pragma once

#include <memory>

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

struct AppContext;
class AudioEngine;
class AppConfig;
class LevelMeter;

/** In-app Settings surface (message thread only). V1: Audio I/O selection + basic status. */
class SettingsComponent final : public juce::Component,
                                private juce::Timer
{
public:
    explicit SettingsComponent(AppContext& context, std::function<void()> onBack);
    ~SettingsComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    void refreshStatus();
    void saveAudioSettings();

    AppContext& appContext;
    std::function<void()> onBackPressed;

    juce::TextButton backButton { "Back" };
    juce::Label titleLabel;

    juce::Label sectionAudioLabel;
    juce::Label monoRoutingHintLabel;

    juce::Label sectionStorageLabel;
    juce::Label libraryRootPathLabel;
    juce::TextButton openLibraryFolderButton { "Open Library Folder" };

    juce::TextButton saveAudioButton { "Save Audio Settings" };
    juce::Label saveStatusLabel;

    juce::TextButton requestMicPermissionButton { "Request Mic Permission" };
    juce::TextButton checkMicPermissionButton { "Check Mic Permission" };
    juce::TextButton runInputProbeButton { "Run Input Probe" };
    juce::ToggleButton inputProbeToggle { "Input Probe" };
    juce::Label inputSourceHeadingLabel;
    juce::ComboBox inputSourceCombo;
    juce::Label micPermissionStatusLabel;
    juce::Label micDeniedHintLabel;
    juce::Label ch1RawPeakLabel;
    juce::Label ch2RawPeakLabel;
    juce::Label ch3RawPeakLabel;
    juce::Label ch4RawPeakLabel;
    juce::Label diagnosisLabel;

    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    std::unique_ptr<juce::Viewport> deviceSelectorViewport;

    juce::Label statusHeading;
    juce::Label deviceTypeLabel;
    juce::Label inputDeviceLabel;
    juce::Label outputDeviceLabel;
    juce::Label callbackDeviceLabel;
    juce::Label inputChannelsLabel;
    juce::Label outputChannelsLabel;
    juce::Label callbackChannelsLabel;
    juce::Label selectedInputLabel;
    juce::Label mixedInputLabel;
    juce::Label inputGainLabel;
    juce::Label rawInputPeakLabel;
    juce::Label inputPresentLabel;
    juce::Label inputWarningLabel;
    juce::Label sampleRateLabel;
    juce::Label bufferSizeLabel;
    juce::Label cpuLabel;
    juce::Label callbackCountLabel;

    juce::ToggleButton inputMonitorToggle { "Input Monitor" };
    juce::Label monitorHintLabel;
    juce::Label testToneHintLabel;

    std::unique_ptr<LevelMeter> inputMeter;
    std::unique_ptr<LevelMeter> outputMeter;
    juce::Label meterHintLabel;

    int diagnosticTickCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};

} // namespace forge7

