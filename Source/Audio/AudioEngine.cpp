#include "AudioEngine.h"

#include <JuceHeader.h>

#include <juce_gui_basics/juce_gui_basics.h>

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
} // namespace

AudioEngine::AudioEngine(PluginHostManager& host)
    : pluginHostManager(host)
{
}

AudioEngine::~AudioEngine()
{
    shutdownAudio();
}

void AudioEngine::initialiseAudioDevice()
{
    // Message thread: device negotiation, logging, and callback registration.
    Logger::info("AudioEngine: initialising AudioDeviceManager (mono in, stereo out @ 48 kHz, buffer " + juce::String(kPreferredBufferSize)
                 + " / fallback " + juce::String(kFallbackBufferSize) + ")");

    deviceManager.removeAudioCallback(this);

    const juce::String initError = deviceManager.initialise(1, 2, nullptr, true, juce::String(), nullptr);
    if (initError.isNotEmpty())
        Logger::error("AudioEngine: AudioDeviceManager::initialise failed — " + initError);

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
        Logger::warn("AudioEngine: primary device setup failed (" + setupError + "); retrying buffer size " + juce::String(kFallbackBufferSize));

        setup.bufferSize = kFallbackBufferSize;
        setupError = deviceManager.setAudioDeviceSetup(setup, true);
    }

    if (setupError.isNotEmpty())
        Logger::error("AudioEngine: audio device setup failed — " + setupError);
    else
        Logger::info("AudioEngine: audio device setup succeeded — sample rate "
                     + juce::String(setup.sampleRate, 1) + " Hz, buffer "
                     + juce::String(setup.bufferSize) + " frames");

    deviceManager.addAudioCallback(this);

    if (auto* device = deviceManager.getCurrentAudioDevice())
        Logger::info("AudioEngine: active device — " + device->getName());
    else
        Logger::error("AudioEngine: no audio device is active after setup");
}

void AudioEngine::shutdownAudio()
{
    // Message thread.
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
    Logger::info("AudioEngine: audio shutdown complete");
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
    // Real-time / audio callback — allocation-free and lock-free. Gain/bypass/meters use relaxed atomics;
    // monoWorkBuffer is resized only in audioDeviceAboutToStart (never here).
    juce::ignoreUnused(context);

    audioCallbackInvocationCount.fetch_add(1, std::memory_order_relaxed);

    if (outputChannelData == nullptr || numOutputChannels <= 0 || numSamples <= 0)
        return;

    if (monoWorkBufferCapacity < numSamples)
    {
#if JUCE_DEBUG
        static std::atomic<int> once { 0 };
        if (once.fetch_add(1, std::memory_order_relaxed) == 0)
            DBG("AudioEngine: callback block larger than prepared mono buffer — dropping audio");
#endif
        return;
    }

    float* mono = monoWorkBuffer.data();

    const float* inMono = (inputChannelData != nullptr && numInputChannels > 0 && inputChannelData[0] != nullptr) ? inputChannelData[0] : nullptr;

    const float inGain = inputGainLinear.load(std::memory_order_relaxed);
    const float outGain = outputGainLinear.load(std::memory_order_relaxed);
    const bool bypassChain = globalBypass.load(std::memory_order_relaxed) != 0;

    if (inMono != nullptr)
        juce::FloatVectorOperations::copyWithMultiply(mono, inMono, inGain, numSamples);
    else
        juce::FloatVectorOperations::clear(mono, numSamples);

    float inPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        inPeak = juce::jmax(inPeak, std::abs(mono[i]));

    if (! bypassChain)
        pluginHostManager.processMonoBlock(mono, numSamples);

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

    meterInputPeak.store(juce::jlimit(0.0f, 1.0f, inPeak), std::memory_order_relaxed);
    meterOutputPeak.store(juce::jlimit(0.0f, 1.0f, outPeak), std::memory_order_relaxed);

    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);

    float* outL = outputChannelData[0];
    float* outR = numOutputChannels > 1 ? outputChannelData[1] : outL;

    if (outL != nullptr)
        juce::FloatVectorOperations::copy(outL, mono, numSamples);
    if (outR != nullptr && outR != outL)
        juce::FloatVectorOperations::copy(outR, mono, numSamples);
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    // Invoked before streaming (often audio/device thread): resize scratch here only — never in processBlock.
    if (device == nullptr)
    {
        juce::MessageManager::callAsync([] { Logger::error("AudioEngine: audioDeviceAboutToStart — device is null"); });
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

    pluginHostManager.prepareToPlay(sr, block);

    const int cap = monoWorkBufferCapacity;
    juce::MessageManager::callAsync([sr, block, cap]()
                                    {
                                        Logger::info("AudioEngine: stream starting — SR " + juce::String(sr, 1) + " Hz, block "
                                                     + juce::String(block) + " samples, mono work capacity " + juce::String(cap));
                                    });
}

void AudioEngine::audioDeviceStopped()
{
    // Typically audio thread / device thread; avoid logging here (can be problematic on some stacks).
    pluginHostManager.releaseResources();
    meterInputPeak.store(0.0f, std::memory_order_relaxed);
    meterOutputPeak.store(0.0f, std::memory_order_relaxed);
}

} // namespace forge7
