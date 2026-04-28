#pragma once

#include <array>
#include <memory>
#include <shared_mutex>

#include <juce_audio_processors/juce_audio_processors.h>

namespace forge7
{

class PluginSlot;

constexpr int kPluginChainMaxSlots = 8;

/** Snapshot for UI / serialization — safe to produce from the message thread via `getSlotInfo`. */
struct SlotInfo
{
    int slotIndex = -1;
    juce::String slotDisplayName;
    juce::String pluginIdentifier;
    bool bypass = false;
    /** No placeholder UID and no hosted processor. */
    bool isEmpty = true;
    /** UID assigned but real `AudioPluginInstance` not yet constructed (audio pass-through). */
    bool isPlaceholder = false;

    /** Plugin referenced by project/snapshot could not be instantiated (offline/missing build). */
    bool missingPlugin = false;
};

/** Fixed serial chain (V1 = 8 slots). Processes left-to-right: each slot receives the mono buffer in place.

    Threading:
    - **`processMonoBlock` / `processBlock`**: real-time safe — walks fixed slot array, no heap
      allocations; slot audio path uses atomics + pre-cleared shared `midiScratch`.
    - **`addPluginToSlot`, `removePluginFromSlot`, `moveSlot`, `clearChain`, `getSlotInfo`**:
      message-thread / control thread — take an exclusive lock on `chainMutex`. The audio callback
      uses **`try_shared_lock`**; if editors hold the exclusive lock, **FX processing is skipped for
      that block** (audio still passes through upstream/downstream of the chain in `AudioEngine`).

    Future: slot `unique_ptr` swaps reorder the rack instantly; heavier graph edits may suspend
    `AudioIODevice` briefly when inserting real VST3 instances from `PluginHostManager`. */
class PluginChain
{
public:
    PluginChain();
    ~PluginChain();

    static constexpr int getMaxSlots() noexcept { return kPluginChainMaxSlots; }

    /** Audio device prepares hosted processors once formats + instances exist. */
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock);

    /** Release hosted processors — not from inside `processMonoBlock`. */
    void releaseResources();

    /** Serial mono RT processing in fixed slot order — allocation-free hot path. */
    void processMonoBlock(float* monoInOut, int numSamples);

    /** Alias requested for symmetry with future stereo `AudioBuffer` overloads — same mono path as `processMonoBlock`. */
    void processBlock(float* monoInOut, int numSamples);

    /** Register placeholder metadata (pass-through audio until real instance is wired). Message thread only. */
    bool addPluginToSlot(int slotIndex,
                         const juce::String& pluginIdentifier,
                         const juce::String& slotDisplayName);

    /** Clears metadata and drops hosted instance reference for that slot. Message thread only. */
    bool removePluginFromSlot(int slotIndex);

    /** Swap two rack positions (`unique_ptr`s). Message thread only; may require `prepareToPlay` recall if instances exist. */
    bool moveSlot(int fromSlotIndex, int toSlotIndex);

    void bypassSlot(int slotIndex, bool shouldBypass);

    void clearChain();

    /** Inspect slot metadata for menus / inspectors — shared lock on `chainMutex`. */
    SlotInfo getSlotInfo(int slotIndex) const;

    PluginSlot* getSlot(size_t slotIndex) noexcept;

    const PluginSlot* getSlot(size_t slotIndex) const noexcept;

    /** Host-managed load: installs instance under exclusive lock — never call from audio callback. */
    bool assignPluginToSlot(int slotIndex,
                           std::unique_ptr<juce::AudioPluginInstance> instance,
                           const juce::PluginDescription& description,
                           double sampleRate,
                           int maximumSamplesPerBlock);

private:
    using SlotArray = std::array<std::unique_ptr<PluginSlot>, static_cast<size_t>(kPluginChainMaxSlots)>;

    SlotArray slots;

    /** Cleared once per audio block — avoids per-slot `MidiBuffer` construction during guitar FX chain. */
    juce::MidiBuffer midiScratch;

    mutable std::shared_mutex chainMutex;

    bool isValidSlotIndex(int slotIndex) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChain)
};

} // namespace forge7
