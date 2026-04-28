#pragma once

#include <atomic>
#include <cstdint>

#include <juce_audio_processors/juce_audio_processors.h>

namespace forge7
{

struct ChainSlotSnapshot;

/** One serial slot in a PluginChain (fixed array of 8). Holds optional
 * `juce::AudioPluginInstance`, bypass flag, and `PluginDescription` metadata for recall.

    Threading:
    - **Audio callback** (`processMonoBlock`): reads only atomics + processor pointer; never `String` / `PluginDescription`.
    - **Message / host thread**: `assignHostedPlugin`, `clearSlotContent`, metadata reads.

    VST3 integration: instances are created and prepared only from `PluginHostManager` off the audio thread. */
class PluginSlot
{
public:
    PluginSlot();

    PluginSlot(const PluginSlot&) = delete;
    PluginSlot& operator=(const PluginSlot&) = delete;

    ~PluginSlot();

    /** Called when the audio device starts or block size changes (not from process callback). */
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock);

    /** Called when the audio device stops (not from process callback). */
    void releaseResources();

    /** Real-time mono processing: bypass or null processor -> pass-through. */
    void processMonoBlock(float* monoInOut, int numSamples, juce::MidiBuffer& midiScratch) noexcept;

    /** Register placeholder metadata only (no instance). Message thread + chain lock. */
    void loadPlaceholderPlugin(const juce::String& pluginUid, const juce::String& displayName);

    /** Clear processor + metadata. Message thread + chain lock. */
    void clearSlotContent();

    /** Replace slot contents with a loaded instance; applies `prepareToPlay` immediately with the
        supplied layout. Message thread + chain lock - never from audio callback. */
    void assignHostedPlugin(std::unique_ptr<juce::AudioPluginInstance> instance,
                            const juce::PluginDescription& description,
                            double sampleRate,
                            int maximumExpectedSamplesPerBlock);

    void setBypass(bool shouldBypass) noexcept { bypassAtomic.store(shouldBypass, std::memory_order_relaxed); }

    bool isBypassedForAudio() const noexcept { return bypassAtomic.load(std::memory_order_relaxed); }

    const juce::String& getSlotName() const noexcept { return slotName; }
    const juce::String& getPluginIdentifier() const noexcept { return pluginIdentifier; }

    /** Full JUCE metadata for persistence (name, format, manufacturer, file path, UID, ...). */
    const juce::PluginDescription& getPluginDescription() const noexcept { return cachedPluginDescription; }

    const juce::MemoryBlock& getPluginStatePlaceholder() const noexcept { return pluginStatePlaceholder; }
    juce::MemoryBlock& getPluginStatePlaceholder() noexcept { return pluginStatePlaceholder; }

    bool isEmptySlot() const noexcept;

    bool isPlaceholderOnly() const noexcept;

    /** True when the project referenced a plugin that is not currently installed / loadable. */
    bool hasMissingPlugin() const noexcept { return missingPluginRuntime; }
    void setMissingPluginState(bool missing) noexcept { missingPluginRuntime = missing; }

    /** Fills snapshot fields + optional processor state Base64 for project save (message thread). */
    void populateSnapshotForProjectSave(ChainSlotSnapshot& snap) const;

    juce::AudioPluginInstance* getHostedInstance() noexcept { return hostedInstance.get(); }

    /** Debug/validation: increments when this slot calls AudioProcessor::processBlock (audio thread). */
    uint64_t getProcessBlockCallCount() const noexcept { return processBlockCallCount.load(std::memory_order_relaxed); }

private:
    void syncMetadataFromDescription(const juce::PluginDescription& desc);
    void syncRtProcessorPointer() noexcept;

    juce::String slotName;
    juce::String pluginIdentifier;

    /** Serialized recall: mirrors last loaded `PluginDescription` (includes fileOrIdentifier, uid, names, format). */
    juce::PluginDescription cachedPluginDescription;

    juce::MemoryBlock pluginStatePlaceholder;

    std::atomic<bool> bypassAtomic { false };

    std::atomic<juce::AudioProcessor*> rtProcessorPtr { nullptr };

    std::atomic<uint64_t> processBlockCallCount { 0 };

    std::unique_ptr<juce::AudioPluginInstance> hostedInstance;

    bool missingPluginRuntime { false };
};

} // namespace forge7
