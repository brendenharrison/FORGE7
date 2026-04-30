#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include <juce_audio_devices/juce_audio_devices.h>


namespace forge7
{

enum class InputSourceMode : uint32_t
{
    FirstNonNull = 0,
    Channel1 = 1,
    Channel2 = 2,
    MixAll = 3
};

class PluginHostManager;

/** Owns JUCE AudioDeviceManager and drives the real-time guitar path:
    mono in -> input gain -> PluginChain (mono) -> output gain -> stereo duplicate.

    Threading contract:
    - **Audio / real-time callback** (`audioDeviceIOCallbackWithContext`, helpers it calls):
      only lock-free atomics and pre-sized buffers; no allocations, locks, logging, or
      PluginHostManager graph edits.
    - **Message thread** (`initialiseAudioDevice`, `shutdownAudio`, gain/bypass setters,
      DeviceManager queries): safe to allocate, log, and reconfigure devices; call these
      from startup code or GUI, never from `audioDeviceIOCallbackWithContext`.

    Interaction: publishes per-block peak levels for GUI via relaxed atomics; forwards the
    mono guitar buffer to PluginHostManager for future PluginChain hosting. */
class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    explicit AudioEngine(PluginHostManager& pluginHostManager);
    ~AudioEngine() override;

    /** Message-thread-only: opens the default device with mono-in/stereo-out @ 48 kHz and
        preferred buffer 64 (fallback 128). Logs success/failure via Logger. Safe to call once at startup. */
    void initialiseAudioDevice();

    /** Message-thread-only: restore device state from config (if present), else fall back to defaults. */
    void initialiseAudioDeviceFromConfig(const juce::String& savedDeviceStateXml);

    /** Message-thread-only: removes callback and closes the audio device (called from destructor). */
    void shutdownAudio();

    /** Message-thread-only: log saved setup + open device active masks + callback snapshot atomics. */
    void logAudioInputDiagnostics(const juce::String& reason) noexcept;

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

    /** Message-thread-only (GUI / control surface): linear gain applied after A/D conversion, before PluginChain. Range [0, 4]. */
    void setInputGainLinear(float linearGain);
    /** Audio-thread-safe read (relaxed); suitable for metering labels on timer. */
    float getInputGainLinear() const noexcept { return inputGainLinear.load(std::memory_order_relaxed); }

    /** Message-thread-only: linear gain at output, before metering and D/A. Range [0, 4]. */
    void setOutputGainLinear(float linearGain);
    float getOutputGainLinear() const noexcept { return outputGainLinear.load(std::memory_order_relaxed); }

    /** Message-thread-only: when true, PluginChain processing is skipped (dry pass-through of gained input). */
    void setGlobalBypass(bool shouldBypass);
    bool isGlobalBypass() const noexcept { return globalBypass.load(std::memory_order_relaxed) != 0; }

    /** Message-thread-only: when true, the live input is monitored to the output (dry or processed).
        When false, the output stays silent regardless of input (test/diagnostic). Default true. */
    void setInputMonitorEnabled(bool shouldMonitor);
    bool isInputMonitorEnabled() const noexcept { return inputMonitorEnabled.load(std::memory_order_relaxed) != 0; }

    /** Message-thread-only: how mono input is derived from device channels (applies to normal path and input probe). Default Channel1. */
    void setInputSourceMode(InputSourceMode mode) noexcept;
    InputSourceMode getInputSourceMode() const noexcept;

    /** Message-thread-only: raw hardware input probe (no plugins, no global bypass, no input-monitor mute). */
    void setInputProbeEnabled(bool shouldProbe) noexcept;
    bool isInputProbeEnabled() const noexcept { return inputProbeMode.load(std::memory_order_relaxed) != 0; }

    /** Raw peak [0, 1] for hardware input channel index (0-7), from last callback; GUI / message thread. */
    float getInputChannelRawPeak(int channelIndex) const noexcept;

    /** Last-callback peak levels in [0, 1], written from RT; safe to read from GUI via relaxed loads. */
    float getSmoothedInputPeak() const noexcept { return meterInputPeak.load(std::memory_order_relaxed); }
    float getSmoothedOutputPeak() const noexcept { return meterOutputPeak.load(std::memory_order_relaxed); }

    /** Max absolute sample across enabled input buffers before input gain (diagnostics). */
    float getLastInputPeakRaw() const noexcept { return meterInputPeakRaw.load(std::memory_order_relaxed); }

    /** Approximate SR / block size after device open; updated on audio thread in aboutToStart (relaxed). */
    double getCurrentSampleRate() const noexcept { return currentSampleRate.load(std::memory_order_relaxed); }
    int getCurrentBufferSize() const noexcept { return currentBufferSize.load(std::memory_order_relaxed); }

    /** Total audio callback invocations (audio thread); for UI health / dropout heuristics only. */
    uint64_t getAudioCallbackInvocationCount() const noexcept
    {
        return audioCallbackInvocationCount.load(std::memory_order_relaxed);
    }

    /** JUCE-reported approximate CPU load of the audio callback [0, 1]. Safe to read from GUI/timer only. */
    double getApproximateCpuUsage() const noexcept { return deviceManager.getCpuUsage(); }

    /** Message thread: tuner captures mono input after input gain, before plugin chain (same path as pre-chain meter). */
    void setTunerCaptureActive(bool active) noexcept;
    bool isTunerCaptureActive() const noexcept { return tunerCaptureActive.load(std::memory_order_relaxed) != 0; }

    /** Message thread: when tuner is open and this is true, main outputs are silenced at the final copy stage. */
    void setTunerMutesOutput(bool shouldMute) noexcept;
    bool getTunerMutesOutput() const noexcept { return tunerMutesOutput.load(std::memory_order_relaxed) != 0; }

    /** Message thread: linearize up to `dstCapacity` recent samples from the tuner ring; returns count written. */
    int copyTunerMonoSnapshot(float* dst, int dstCapacity) const noexcept;

    /** Diagnostic atomics published from the audio callback - read safely from message thread (relaxed). */
    int getLastSelectedInputChannel() const noexcept { return lastSelectedInputChannel.load(std::memory_order_relaxed); }
    /** Number of non-null input buffers averaged into mono last callback (0 if none). */
    int getLastMixedInputChannelCount() const noexcept { return lastMixedInputChannelCount.load(std::memory_order_relaxed); }
    int getLastNumInputChannels() const noexcept { return lastNumInputChannels.load(std::memory_order_relaxed); }
    int getLastNumOutputChannels() const noexcept { return lastNumOutputChannels.load(std::memory_order_relaxed); }
    bool isInputPresentInCallback() const noexcept { return inputPresentFlag.load(std::memory_order_relaxed) != 0; }

    // AudioIODeviceCallback  (audio thread - see implementation comments)
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    PluginHostManager& pluginHostManager;
    juce::AudioDeviceManager deviceManager;

    /** Pre-sized in `audioDeviceAboutToStart` only - holds mono samples between stages. */
    std::vector<float> monoWorkBuffer;
    int monoWorkBufferCapacity = 0;

    std::atomic<float> inputGainLinear { 1.0f };
    std::atomic<float> outputGainLinear { 1.0f };
    std::atomic<uint32_t> globalBypass { 0 };
    std::atomic<uint32_t> inputMonitorEnabled { 1 };
    std::atomic<uint32_t> inputSourceModeStorage { static_cast<uint32_t>(InputSourceMode::Channel1) };
    std::atomic<uint32_t> inputProbeMode { 0 };

    std::array<std::atomic<float>, 8> inputChannelRawPeaks {};

    std::atomic<float> meterInputPeak { 0.0f };
    std::atomic<float> meterInputPeakRaw { 0.0f };
    std::atomic<float> meterOutputPeak { 0.0f };

    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<int> currentBufferSize { 64 };

    std::atomic<uint64_t> audioCallbackInvocationCount { 0 };

    std::atomic<int> lastSelectedInputChannel { -1 };
    std::atomic<int> lastMixedInputChannelCount { 0 };
    std::atomic<int> lastNumInputChannels { 0 };
    std::atomic<int> lastNumOutputChannels { 0 };
    std::atomic<uint32_t> inputPresentFlag { 0 };

    static float clampGain(float g) noexcept;

    void appendTunerPreFxMonoToRing(const float* mono, int numSamples) noexcept;

    std::vector<float> tunerPreFxRing;
    std::atomic<uint64_t> tunerRingWrite { 0 };
    std::atomic<uint32_t> tunerCaptureActive { 0 };
    std::atomic<uint32_t> tunerMutesOutput { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

} // namespace forge7
