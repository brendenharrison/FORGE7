#include "AudioEngine.h"

#include <cstdint>

#include <JuceHeader.h>

#include <fstream>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ChainMeterTaps.h"
#include "../PluginHost/PluginHostManager.h"
#include "../Utilities/Logger.h"

namespace forge7
{
namespace
{
constexpr int kPreferredBufferSize = 64;
constexpr int kFallbackBufferSize = 128;
constexpr double kTargetSampleRate = 48000.0;
constexpr float kMaxLinearGain = 4.0f;
constexpr int kInputPeakChannels = 8;

void clearInputChannelPeaks(std::array<std::atomic<float>, static_cast<size_t>(kInputPeakChannels)>& peaks) noexcept
{
    for (auto& p : peaks)
        p.store(0.0f, std::memory_order_relaxed);
}

void updateInputChannelPeaks(const float* const* inputChannelData,
                             int numInputChannels,
                             int numSamples,
                             std::array<std::atomic<float>, static_cast<size_t>(kInputPeakChannels)>& peaks) noexcept
{
    for (int ch = 0; ch < kInputPeakChannels; ++ch)
    {
        float pk = 0.0f;
        if (inputChannelData != nullptr && ch < numInputChannels && inputChannelData[ch] != nullptr)
        {
            const float* inCh = inputChannelData[ch];
            for (int i = 0; i < numSamples; ++i)
                pk = juce::jmax(pk, std::abs(inCh[i]));
        }
        peaks[static_cast<size_t>(ch)].store(juce::jlimit(0.0f, 1.0f, pk), std::memory_order_relaxed);
    }
}

float maxStoredInputPeaks(const std::array<std::atomic<float>, static_cast<size_t>(kInputPeakChannels)>& peaks) noexcept
{
    float m = 0.0f;
    for (const auto& p : peaks)
        m = juce::jmax(m, p.load(std::memory_order_relaxed));
    return m;
}

float peakAbsBlock(const float* x, int numSamples) noexcept
{
    if (x == nullptr || numSamples <= 0)
        return 0.0f;

    float p = 0.0f;

    for (int i = 0; i < numSamples; ++i)
        p = juce::jmax(p, std::abs(x[i]));

    return juce::jlimit(0.0f, 1.0f, p);
}

void storeBypassPassthroughSlotPeaks(ChainMeterTaps& taps, const float* mono, int numSamples) noexcept
{
    const float pk = peakAbsBlock(mono, numSamples);

    for (auto& slotPeak : taps.postSlotPeak)
        slotPeak.store(pk, std::memory_order_relaxed);
}

/** Returns number of channels mixed into mono; `outSelectedCh` is first contributing channel index or -1. */
int fillMonoFromInputSelection(const float* const* inputChannelData,
                               int numInputChannels,
                               int numSamples,
                               float* mono,
                               float gain,
                               InputSourceMode mode,
                               int& outSelectedCh) noexcept
{
    outSelectedCh = -1;
    if (mono == nullptr || numSamples <= 0)
        return 0;

    if (inputChannelData == nullptr || numInputChannels <= 0)
    {
        juce::FloatVectorOperations::clear(mono, numSamples);
        return 0;
    }

    switch (mode)
    {
        case InputSourceMode::FirstNonNull:
            for (int ch = 0; ch < numInputChannels; ++ch)
            {
                const float* inCh = inputChannelData[ch];
                if (inCh == nullptr)
                    continue;
                juce::FloatVectorOperations::copyWithMultiply(mono, inCh, gain, numSamples);
                outSelectedCh = ch;
                return 1;
            }
            break;

        case InputSourceMode::Channel1:
            if (numInputChannels > 0 && inputChannelData[0] != nullptr)
            {
                juce::FloatVectorOperations::copyWithMultiply(mono, inputChannelData[0], gain, numSamples);
                outSelectedCh = 0;
                return 1;
            }
            break;

        case InputSourceMode::Channel2:
            if (numInputChannels > 1 && inputChannelData[1] != nullptr)
            {
                juce::FloatVectorOperations::copyWithMultiply(mono, inputChannelData[1], gain, numSamples);
                outSelectedCh = 1;
                return 1;
            }
            break;

        case InputSourceMode::MixAll: {
            int count = 0;
            for (int ch = 0; ch < numInputChannels; ++ch)
            {
                const float* inCh = inputChannelData[ch];
                if (inCh == nullptr)
                    continue;
                if (count == 0)
                {
                    outSelectedCh = ch;
                    juce::FloatVectorOperations::copyWithMultiply(mono, inCh, gain, numSamples);
                }
                else
                    juce::FloatVectorOperations::addWithMultiply(mono, inCh, gain, numSamples);
                ++count;
            }
            if (count > 1)
            {
                const float scale = 1.0f / static_cast<float>(count);
                juce::FloatVectorOperations::multiply(mono, scale, numSamples);
            }
            return count;
        }

        default:
            break;
    }

    juce::FloatVectorOperations::clear(mono, numSamples);
    return 0;
}

// #region agent log
// Tiny NDJSON appender for debug session af0f62 - message thread only.
// Path is provisioned by the debug harness; misses are silent.
void debugNdjsonAppend(const juce::String& location,
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
} // namespace

AudioEngine::AudioEngine(PluginHostManager& host)
    : pluginHostManager(host)
{
    clearInputChannelPeaks(inputChannelRawPeaks);
}

AudioEngine::~AudioEngine()
{
    shutdownAudio();
}

void AudioEngine::initialiseAudioDevice()
{
    initialiseAudioDeviceFromConfig({});
}

void AudioEngine::initialiseAudioDeviceFromConfig(const juce::String& savedDeviceStateXml)
{
    // Message thread: device negotiation, logging, and callback registration.
    Logger::info("AudioEngine: initialising AudioDeviceManager (mono in, stereo out @ 48 kHz, buffer "
                 + juce::String(kPreferredBufferSize) + " / fallback " + juce::String(kFallbackBufferSize) + ")");

    deviceManager.removeAudioCallback(this);

    if (savedDeviceStateXml.isNotEmpty())
    {
        std::unique_ptr<juce::XmlElement> xml(juce::parseXML(savedDeviceStateXml));
        if (xml != nullptr)
        {
            Logger::info("AudioEngine: attempting restore of saved audio device state XML");
            const juce::String err = deviceManager.initialise(8, 8, xml.get(), true, juce::String(), nullptr);
            if (err.isNotEmpty())
                Logger::warn("AudioEngine: restore from saved audio state failed - " + err);
        }
        else
        {
            Logger::warn("AudioEngine: saved audio device state XML was invalid; falling back to defaults");
        }
    }

    // If no device is active after restore attempt, fall back to defaults.
    if (deviceManager.getCurrentAudioDevice() == nullptr)
    {
        const juce::String initError = deviceManager.initialise(1, 2, nullptr, true, juce::String(), nullptr);
        if (initError.isNotEmpty())
            Logger::error("AudioEngine: AudioDeviceManager::initialise failed - " + initError);

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup(setup);

        setup.sampleRate = kTargetSampleRate;
        setup.bufferSize = kPreferredBufferSize;

        juce::BigInteger inputChannels;
        inputChannels.setBit(0);

        juce::BigInteger outputChannels;
        outputChannels.setBit(0);
        outputChannels.setBit(1);

        setup.inputChannels = inputChannels;
        setup.outputChannels = outputChannels;

        juce::String setupError = deviceManager.setAudioDeviceSetup(setup, true);

        if (setupError.isNotEmpty())
        {
            Logger::warn("AudioEngine: primary device setup failed (" + setupError + "); retrying buffer size "
                         + juce::String(kFallbackBufferSize));

            setup.bufferSize = kFallbackBufferSize;
            setupError = deviceManager.setAudioDeviceSetup(setup, true);
        }

        if (setupError.isNotEmpty())
            Logger::error("AudioEngine: audio device setup failed - " + setupError);
        else
            Logger::info("AudioEngine: audio device setup succeeded - sample rate "
                         + juce::String(setup.sampleRate, 1) + " Hz, buffer "
                         + juce::String(setup.bufferSize) + " frames");
    }

    deviceManager.addAudioCallback(this);

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        juce::AudioDeviceManager::AudioDeviceSetup activeSetup;
        deviceManager.getAudioDeviceSetup(activeSetup);

        Logger::info("AudioEngine: active device - " + device->getName()
                     + " | type=" + deviceManager.getCurrentAudioDeviceType()
                     + " | inputDevice=\"" + activeSetup.inputDeviceName + "\""
                     + " | outputDevice=\"" + activeSetup.outputDeviceName + "\""
                     + " | inMask=" + activeSetup.inputChannels.toString(2)
                     + " | outMask=" + activeSetup.outputChannels.toString(2)
                     + " | sr=" + juce::String(activeSetup.sampleRate, 1)
                     + " | block=" + juce::String(activeSetup.bufferSize));

        // #region agent log
        juce::DynamicObject::Ptr d { new juce::DynamicObject() };
        d->setProperty("inputDeviceName", activeSetup.inputDeviceName);
        d->setProperty("outputDeviceName", activeSetup.outputDeviceName);
        d->setProperty("inputChannelsMask", activeSetup.inputChannels.toString(2));
        d->setProperty("outputChannelsMask", activeSetup.outputChannels.toString(2));
        d->setProperty("inputChannelsBitCount", activeSetup.inputChannels.countNumberOfSetBits());
        d->setProperty("outputChannelsBitCount", activeSetup.outputChannels.countNumberOfSetBits());
        d->setProperty("sampleRate", activeSetup.sampleRate);
        d->setProperty("bufferSize", activeSetup.bufferSize);
        d->setProperty("deviceTypeName", deviceManager.getCurrentAudioDeviceType());
        debugNdjsonAppend("AudioEngine.cpp:setupComplete", "device setup after init/restore", juce::var(d.get()), "H2,H5");
        // #endregion
    }
    else
    {
        Logger::error("AudioEngine: no audio device is active after setup");
        // #region agent log
        debugNdjsonAppend("AudioEngine.cpp:setupComplete", "no active device after setup", juce::var(), "H2");
        // #endregion
    }
}

void AudioEngine::shutdownAudio()
{
    // Message thread.
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
    Logger::info("AudioEngine: audio shutdown complete");
}

void AudioEngine::logAudioInputDiagnostics(const juce::String& reason) noexcept
{
    // Message thread only (never call from audio callback).
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    Logger::info("FORGE7 AudioIO diagnostic [" + reason + "] setup input=\"" + setup.inputDeviceName + "\" output=\""
                   + setup.outputDeviceName + "\" inMask=" + setup.inputChannels.toString(2) + " outMask="
                   + setup.outputChannels.toString(2) + " sr=" + juce::String(setup.sampleRate, 1) + " buffer="
                   + juce::String(setup.bufferSize));

    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        Logger::info("FORGE7 AudioIO diagnostic [" + reason + "] openDevice=\"" + dev->getName() + "\" activeInMask="
                     + dev->getActiveInputChannels().toString(2) + " activeOutMask="
                     + dev->getActiveOutputChannels().toString(2));
    }
    else
        Logger::info("FORGE7 AudioIO diagnostic [" + reason + "] openDevice=(none)");

    Logger::info("FORGE7 AudioIO diagnostic [" + reason + "] callback inputPresent="
                 + juce::String(isInputPresentInCallback() ? "yes" : "no") + " selectedInputCh="
                 + juce::String(getLastSelectedInputChannel()) + " mixedInputs="
                 + juce::String(getLastMixedInputChannelCount()));
}

void AudioEngine::setInputGainLinear(float linearGain)
{
    inputGainLinear.store(clampGain(linearGain), std::memory_order_relaxed);
}

void AudioEngine::setOutputGainLinear(float linearGain)
{
    outputGainLinear.store(clampGain(linearGain), std::memory_order_relaxed);
}

void AudioEngine::setGlobalBypass(bool shouldBypass)
{
    globalBypass.store(shouldBypass ? 1u : 0u, std::memory_order_relaxed);
}

void AudioEngine::setInputMonitorEnabled(bool shouldMonitor)
{
    inputMonitorEnabled.store(shouldMonitor ? 1u : 0u, std::memory_order_relaxed);
}

void AudioEngine::setInputSourceMode(InputSourceMode mode) noexcept
{
    inputSourceModeStorage.store(static_cast<uint32_t>(mode), std::memory_order_relaxed);
}

InputSourceMode AudioEngine::getInputSourceMode() const noexcept
{
    return static_cast<InputSourceMode>(inputSourceModeStorage.load(std::memory_order_relaxed));
}

void AudioEngine::setInputProbeEnabled(bool shouldProbe) noexcept
{
    inputProbeMode.store(shouldProbe ? 1u : 0u, std::memory_order_relaxed);
}

float AudioEngine::getInputChannelRawPeak(int channelIndex) const noexcept
{
    if (channelIndex < 0 || channelIndex >= kInputPeakChannels)
        return 0.0f;
    return inputChannelRawPeaks[static_cast<size_t>(channelIndex)].load(std::memory_order_relaxed);
}

float AudioEngine::clampGain(float g) noexcept
{
    return juce::jlimit(0.0f, kMaxLinearGain, g);
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int numInputChannels,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext& context)
{
    // Real-time / audio callback - allocation-free and lock-free. Gain/bypass/meters use relaxed atomics;
    // monoWorkBuffer is resized only in audioDeviceAboutToStart (never here).
    juce::ignoreUnused(context);

    audioCallbackInvocationCount.fetch_add(1, std::memory_order_relaxed);

    lastNumInputChannels.store(juce::jmax(0, numInputChannels), std::memory_order_relaxed);
    lastNumOutputChannels.store(juce::jmax(0, numOutputChannels), std::memory_order_relaxed);

    if (outputChannelData == nullptr || numOutputChannels <= 0 || numSamples <= 0)
    {
        lastSelectedInputChannel.store(-1, std::memory_order_relaxed);
        lastMixedInputChannelCount.store(0, std::memory_order_relaxed);
        inputPresentFlag.store(0u, std::memory_order_relaxed);
        meterInputPeakRaw.store(0.0f, std::memory_order_relaxed);
        clearInputChannelPeaks(inputChannelRawPeaks);
        return;
    }

    if (monoWorkBufferCapacity < numSamples)
    {
#if JUCE_DEBUG
        static std::atomic<int> once { 0 };
        if (once.fetch_add(1, std::memory_order_relaxed) == 0)
            DBG("AudioEngine: callback block larger than prepared mono buffer - dropping audio");
#endif
        lastSelectedInputChannel.store(-1, std::memory_order_relaxed);
        lastMixedInputChannelCount.store(0, std::memory_order_relaxed);
        inputPresentFlag.store(0u, std::memory_order_relaxed);
        meterInputPeakRaw.store(0.0f, std::memory_order_relaxed);
        clearInputChannelPeaks(inputChannelRawPeaks);
        return;
    }

    updateInputChannelPeaks(inputChannelData, numInputChannels, numSamples, inputChannelRawPeaks);

    float* mono = monoWorkBuffer.data();

    const auto sourceMode = static_cast<InputSourceMode>(inputSourceModeStorage.load(std::memory_order_relaxed));
    const bool probe = inputProbeMode.load(std::memory_order_relaxed) != 0;

    int selectedInputChannel = -1;
    int mixedCount = 0;

    const float rawPeakPreGain = maxStoredInputPeaks(inputChannelRawPeaks);

    if (probe)
    {
        constexpr float kProbeGain = 1.0f;
        mixedCount = fillMonoFromInputSelection(inputChannelData,
                                                numInputChannels,
                                                numSamples,
                                                mono,
                                                kProbeGain,
                                                sourceMode,
                                                selectedInputChannel);

        meterInputPeakRaw.store(juce::jlimit(0.0f, 1.0f, rawPeakPreGain), std::memory_order_relaxed);

        lastSelectedInputChannel.store(selectedInputChannel, std::memory_order_relaxed);
        lastMixedInputChannelCount.store(mixedCount, std::memory_order_relaxed);
        inputPresentFlag.store(mixedCount > 0 ? 1u : 0u, std::memory_order_relaxed);

        if (mixedCount == 0)
            juce::FloatVectorOperations::clear(mono, numSamples);

        float inPeak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            inPeak = juce::jmax(inPeak, std::abs(mono[i]));

        meterInputPeak.store(juce::jlimit(0.0f, 1.0f, inPeak), std::memory_order_relaxed);

        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);

        float* outPrimary = nullptr;
        float* outSecondary = nullptr;
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] == nullptr)
                continue;
            if (outPrimary == nullptr)
                outPrimary = outputChannelData[ch];
            else if (outSecondary == nullptr)
            {
                outSecondary = outputChannelData[ch];
                break;
            }
        }

        if (outPrimary != nullptr)
            juce::FloatVectorOperations::copy(outPrimary, mono, numSamples);
        if (outSecondary != nullptr && outSecondary != outPrimary)
            juce::FloatVectorOperations::copy(outSecondary, mono, numSamples);

        meterOutputPeak.store(juce::jlimit(0.0f, 1.0f, inPeak), std::memory_order_relaxed);

        appendTunerPreFxMonoToRing(mono, numSamples);

        {
            auto& taps = pluginHostManager.getChainMeterTaps();
            taps.preChainPeak.store(juce::jlimit(0.0f, 1.0f, inPeak), std::memory_order_relaxed);
            storeBypassPassthroughSlotPeaks(taps, mono, numSamples);
            taps.postOutputGainPeak.store(juce::jlimit(0.0f, 1.0f, inPeak), std::memory_order_relaxed);
        }
        return;
    }

    const float inGain = inputGainLinear.load(std::memory_order_relaxed);
    mixedCount = fillMonoFromInputSelection(
        inputChannelData, numInputChannels, numSamples, mono, inGain, sourceMode, selectedInputChannel);

    meterInputPeakRaw.store(juce::jlimit(0.0f, 1.0f, rawPeakPreGain), std::memory_order_relaxed);

    lastSelectedInputChannel.store(selectedInputChannel, std::memory_order_relaxed);
    lastMixedInputChannelCount.store(mixedCount, std::memory_order_relaxed);
    inputPresentFlag.store(mixedCount > 0 ? 1u : 0u, std::memory_order_relaxed);

    if (mixedCount == 0)
        juce::FloatVectorOperations::clear(mono, numSamples);

    const float outGain = outputGainLinear.load(std::memory_order_relaxed);
    const bool bypassChain = globalBypass.load(std::memory_order_relaxed) != 0;
    const bool monitorOn = inputMonitorEnabled.load(std::memory_order_relaxed) != 0;

    float inPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        inPeak = juce::jmax(inPeak, std::abs(mono[i]));

    auto& meterTaps = pluginHostManager.getChainMeterTaps();
    meterTaps.preChainPeak.store(juce::jlimit(0.0f, 1.0f, inPeak), std::memory_order_relaxed);

    appendTunerPreFxMonoToRing(mono, numSamples);

    if (! bypassChain)
        pluginHostManager.processMonoBlock(mono, numSamples);
    else
        storeBypassPassthroughSlotPeaks(meterTaps, mono, numSamples);

    float outPeak = 0.0f;
    if (outGain != 1.0f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = mono[i] * outGain;
            mono[i] = s;
            outPeak = juce::jmax(outPeak, std::abs(s));
        }
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
            outPeak = juce::jmax(outPeak, std::abs(mono[i]));
    }

    meterTaps.postOutputGainPeak.store(juce::jlimit(0.0f, 1.0f, outPeak), std::memory_order_relaxed);

    const bool tunerCapturing = tunerCaptureActive.load(std::memory_order_relaxed) != 0;
    const bool tunerMuteOut = tunerCapturing && (tunerMutesOutput.load(std::memory_order_relaxed) != 0);
    const bool silenceOutput = (monitorOn == false) || tunerMuteOut;

    meterInputPeak.store(juce::jlimit(0.0f, 1.0f, inPeak), std::memory_order_relaxed);
    meterOutputPeak.store(silenceOutput ? 0.0f : juce::jlimit(0.0f, 1.0f, outPeak), std::memory_order_relaxed);

    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);

    if (silenceOutput)
        return;

    float* outPrimary = nullptr;
    float* outSecondary = nullptr;

    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] == nullptr)
            continue;

        if (outPrimary == nullptr)
            outPrimary = outputChannelData[ch];
        else if (outSecondary == nullptr)
        {
            outSecondary = outputChannelData[ch];
            break;
        }
    }

    if (outPrimary != nullptr)
        juce::FloatVectorOperations::copy(outPrimary, mono, numSamples);
    if (outSecondary != nullptr && outSecondary != outPrimary)
        juce::FloatVectorOperations::copy(outSecondary, mono, numSamples);
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    // Invoked before streaming (often audio/device thread): resize scratch here only - never in processBlock.
    if (device == nullptr)
    {
        juce::MessageManager::callAsync([] { Logger::error("AudioEngine: audioDeviceAboutToStart - device is null"); });
        monoWorkBuffer.clear();
        monoWorkBufferCapacity = 0;
        return;
    }

    const double sr = device->getCurrentSampleRate();
    const int block = device->getCurrentBufferSizeSamples();

    currentSampleRate.store(sr, std::memory_order_relaxed);
    currentBufferSize.store(block, std::memory_order_relaxed);

    monoWorkBuffer.assign(static_cast<size_t>(juce::jmax(1, block)), 0.0f);
    monoWorkBufferCapacity = block;

    tunerPreFxRing.assign(16384u, 0.0f);
    tunerRingWrite.store(0, std::memory_order_relaxed);

    pluginHostManager.prepareToPlay(sr, block);

    const int cap = monoWorkBufferCapacity;
    const juce::String deviceName = device->getName();
    const juce::StringArray inputChannelNames = device->getInputChannelNames();
    const juce::StringArray outputChannelNames = device->getOutputChannelNames();
    const juce::BigInteger activeInputs = device->getActiveInputChannels();
    const juce::BigInteger activeOutputs = device->getActiveOutputChannels();

    juce::MessageManager::callAsync([sr, block, cap, deviceName, inputChannelNames, outputChannelNames, activeInputs, activeOutputs]()
                                    {
                                        Logger::info("AudioEngine: stream starting - device \"" + deviceName + "\""
                                                     + " | SR " + juce::String(sr, 1) + " Hz | block " + juce::String(block)
                                                     + " | mono work capacity " + juce::String(cap)
                                                     + " | activeIn=" + activeInputs.toString(2)
                                                     + " | activeOut=" + activeOutputs.toString(2)
                                                     + " | inNames=[" + inputChannelNames.joinIntoString(", ") + "]"
                                                     + " | outNames=[" + outputChannelNames.joinIntoString(", ") + "]");

                                        // #region agent log
                                        juce::DynamicObject::Ptr d { new juce::DynamicObject() };
                                        d->setProperty("deviceName", deviceName);
                                        d->setProperty("sampleRate", sr);
                                        d->setProperty("bufferSize", block);
                                        d->setProperty("monoWorkCapacity", cap);
                                        d->setProperty("activeInputs", activeInputs.toString(2));
                                        d->setProperty("activeOutputs", activeOutputs.toString(2));
                                        d->setProperty("activeInputCount", activeInputs.countNumberOfSetBits());
                                        d->setProperty("activeOutputCount", activeOutputs.countNumberOfSetBits());
                                        d->setProperty("inputChannelNames", inputChannelNames.joinIntoString("|"));
                                        d->setProperty("outputChannelNames", outputChannelNames.joinIntoString("|"));
                                        debugNdjsonAppend("AudioEngine.cpp:audioDeviceAboutToStart", "stream starting",
                                                          juce::var(d.get()), "H2,H3,H4");
                                        // #endregion
                                    });
}

void AudioEngine::audioDeviceStopped()
{
    // Typically audio thread / device thread; avoid logging here (can be problematic on some stacks).
    pluginHostManager.releaseResources();
    meterInputPeak.store(0.0f, std::memory_order_relaxed);
    meterInputPeakRaw.store(0.0f, std::memory_order_relaxed);
    meterOutputPeak.store(0.0f, std::memory_order_relaxed);
    lastSelectedInputChannel.store(-1, std::memory_order_relaxed);
    lastMixedInputChannelCount.store(0, std::memory_order_relaxed);
    lastNumInputChannels.store(0, std::memory_order_relaxed);
    lastNumOutputChannels.store(0, std::memory_order_relaxed);
    inputPresentFlag.store(0u, std::memory_order_relaxed);
    clearInputChannelPeaks(inputChannelRawPeaks);
    pluginHostManager.getChainMeterTaps().resetPeaksToZero();
    tunerRingWrite.store(0, std::memory_order_relaxed);
}

void AudioEngine::appendTunerPreFxMonoToRing(const float* mono, int numSamples) noexcept
{
    if (tunerCaptureActive.load(std::memory_order_relaxed) == 0)
        return;

    if (mono == nullptr || numSamples <= 0)
        return;

    const size_t cap = tunerPreFxRing.size();

    if (cap == 0)
        return;

    uint64_t w = tunerRingWrite.load(std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        tunerPreFxRing[static_cast<size_t>(w % cap)] = mono[i];
        ++w;
    }

    tunerRingWrite.store(w, std::memory_order_relaxed);
}

void AudioEngine::setTunerCaptureActive(const bool active) noexcept
{
    tunerCaptureActive.store(active ? 1u : 0u, std::memory_order_relaxed);
}

void AudioEngine::setTunerMutesOutput(const bool shouldMute) noexcept
{
    tunerMutesOutput.store(shouldMute ? 1u : 0u, std::memory_order_relaxed);
}

int AudioEngine::copyTunerMonoSnapshot(float* dst, const int dstCapacity) const noexcept
{
    if (dst == nullptr || dstCapacity <= 0)
        return 0;

    const size_t cap = tunerPreFxRing.size();

    if (cap == 0)
        return 0;

    const auto w = static_cast<int64_t>(tunerRingWrite.load(std::memory_order_relaxed));
    const int n = juce::jmin(dstCapacity, 4096);

    for (int i = 0; i < n; ++i)
    {
        const int64_t back = w - static_cast<int64_t>(n - i);

        if (back < 0)
            dst[i] = 0.0f;
        else
            dst[i] = tunerPreFxRing[static_cast<size_t>(back) % cap];
    }

    return n;
}

} // namespace forge7
