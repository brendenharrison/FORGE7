#pragma once

#include <atomic>
#include <vector>

#include <juce_audio_devices/juce_audio_devices.h>

namespace forge7
{

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

    /** Message-thread-only: removes callback and closes the audio device (called from destructor). */
    void shutdownAudio();

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

    /** Last-callback peak levels in [0, 1], written from RT; safe to read from GUI via relaxed loads. */
    float getSmoothedInputPeak() const noexcept { return meterInputPeak.load(std::memory_order_relaxed); }
    float getSmoothedOutputPeak() const noexcept { return meterOutputPeak.load(std::memory_order_relaxed); }

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

    std::atomic<float> meterInputPeak { 0.0f };
    std::atomic<float> meterOutputPeak { 0.0f };

    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<int> currentBufferSize { 64 };

    std::atomic<uint64_t> audioCallbackInvocationCount { 0 };

    static float clampGain(float g) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

} // namespace forge7
