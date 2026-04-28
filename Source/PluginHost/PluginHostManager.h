#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Audio/CrossfadeMixer.h"

namespace forge7
{

class PluginChain;
class SceneManager;
class ChainVariation;

/** Central registry: VST3-capable formats, `KnownPluginList`, filesystem scanning off the audio
 * thread, and `AudioPluginInstance` lifecycle for **two** duplicate `PluginChain` racks used for
 * seamless chain-variation crossfading.

 * Threading (**never scan or instantiate on the audio callback**):
 * - **Audio thread**: only `processMonoBlock`, `prepareToPlay`, `releaseResources`; **no allocations**
 *   on the inner path (scratch buffers sized in `prepareToPlay`).
 * - **Background worker**: disk scanning (`PluginDirectoryScanner`), `createPluginInstance` — both may block.
 * - **Message thread**: completion callbacks (`MessageManager::callAsync`), applying instances to slots,
 *   `hydratePluginChainFromChainVariation`, `commitChainVariationCrossfade`.

 * Variation switching: the **non-audible** chain is hydrated from the target `ChainVariation` snapshot
 * on the message thread **before** the fade is armed — **no plugin loads inside** `processMonoBlock`. */
class PluginHostManager
{
public:
    PluginHostManager();
    ~PluginHostManager();

    juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager; }

    /** Thread-safe snapshot of scanned plugins — safe for UI lists (copy). */
    juce::Array<juce::PluginDescription> getAvailablePluginDescriptions() const;

    /** Prefer `getAvailablePluginDescriptions()` from UI — concurrent scanning mutates internally. */
    juce::KnownPluginList& getKnownPluginList() noexcept { return knownPluginList; }

    void addPluginScanDirectory(const juce::File& directory);
    void removePluginScanDirectory(const juce::File& directory);
    void clearPluginScanDirectories();
    juce::Array<juce::File> getPluginScanDirectories() const;

    /** Typical OS folders (`~/.vst3`, system/user VST3 dirs on macOS, `/usr/lib/vst3` on Linux, etc.). */
    void addStandardPlatformScanDirectories();

    /** Blocking scan — **never** invoke from audio thread. Delegates to `scanVST3PluginsBlocking()`. */
    int scanAllPluginsBlocking();

    /** Blocking VST3-only scan using registered VST3 `AudioPluginFormat`. Returns descriptions added this pass. */
    int scanVST3PluginsBlocking();

    /** Worker thread runs `scanVST3PluginsBlocking`; completion on message thread with added count. */
    void scanVST3PluginsAsync(std::function<void(int numDescriptionsAdded)> onFinished);

    /** Worker thread runs scan; completion invoked on message thread (`numDescriptionsAdded`). */
    void scanAllPluginsAsync(std::function<void(int numDescriptionsAdded)> onFinished);

    /** Same as `getKnownPluginDescriptionCount()` — total entries in `KnownPluginList` under lock. */
    int getKnownPluginCount() const;

    /** Thread-safe count of entries in `KnownPluginList` (may include cached non-VST3 from older sessions). */
    int getKnownPluginDescriptionCount() const;

    /** Persist / restore KnownPluginList cache — message thread only. */
    bool saveKnownPluginsToFile(const juce::File& xmlFile) const;
    bool loadKnownPluginsFromFile(const juce::File& xmlFile);

    using PluginLoadCompletion = std::function<void(bool success, const juce::String& errorMessage)>;

    /** Background instantiation + message-thread slot assignment — never blocks the audio callback. */
    void loadPluginIntoSlotAsync(const juce::PluginDescription& description,
                                int slotIndex,
                                PluginLoadCompletion onComplete);

    /** Same as async path but blocking on caller thread — **message thread only**, never audio callback. */
    bool loadPluginIntoSlotSynchronously(const juce::PluginDescription& description,
                                        int slotIndex,
                                        juce::String& errorMessageOut);

    /** Real-time mono path — allocation-free when not crossfading; uses pre-sized scratch when crossfading. */
    void processMonoBlock(float* monoInOut, int numSamples);

    void prepareToPlay(double sampleRate, int blockSize);
    void releaseResources();

    /** Audible rack for UI load/editor — points at the chain currently designated “live” after the last
        completed fade (during a fade this is still the **outgoing** chain until the fade completes). */
    PluginChain* getPluginChain() noexcept;

    /** Last audio layout passed to `prepareToPlay` — safe for project hydrate on message thread. */
    double getLastKnownSampleRate() const noexcept;
    int getLastKnownBlockSize() const noexcept;

    /** Message thread only: clears `chain` and rebuilds processors from `variation` snapshot (matches project load). */
    void hydratePluginChainFromChainVariation(PluginChain& chain, const ChainVariation& variation);

    /** Message thread: set `SceneManager`’s active variation first, then call this to hydrate the **idle**
        rack and arm a crossfade. Returns false if a fade is already running or the scene/variation is invalid.

        Never instantiates plugins on the audio thread — all loads complete here before `beginCrossfade`. */
    bool commitChainVariationCrossfade(SceneManager& scenes);

    /** Message thread: abort any fade, force audible rack index `0`, clear the secondary chain — call
        before project hydrate so the loaded file owns a deterministic routing baseline. */
    void resetVariationRoutingAfterProjectLoad() noexcept;

    /** Default 20 ms; applied on next `prepare()` / SR change via `refreshFadeLengthSamples()`. */
    void setVariationCrossfadeTimeMs(double milliseconds) noexcept;
    double getVariationCrossfadeTimeMs() const noexcept;

    /** True while the audio thread is blending two racks (relaxed load — message/UI use only). */
    bool isVariationCrossfadeActive() const noexcept;

private:
    bool assignInstanceToSlotMessageThread(std::unique_ptr<juce::AudioPluginInstance> instance,
                                          const juce::PluginDescription& description,
                                          int slotIndex,
                                          juce::String& errorOut);

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;

    mutable juce::CriticalSection pluginListLock;

    mutable juce::CriticalSection scanFoldersLock;
    juce::Array<juce::File> userScanDirectories;

    mutable juce::CriticalSection audioLayoutLock;
    double lastSampleRate { 48000.0 };
    int lastBlockSize { 512 };

    /** Two identical racks — only one is “audible” while idle; during a fade both process in parallel. */
    std::array<std::unique_ptr<PluginChain>, 2> chains;

    /** 0 or 1 — updated when a crossfade completes (audio thread) or after `resetVariationRoutingAfterProjectLoad`. */
    std::atomic<int> audibleChainIndex { 0 };

    /** Valid while `variationCrossfade.isCrossfading()` — read from audio thread only after acquire. */
    std::atomic<int> fadeOutChainIndex { 0 };
    std::atomic<int> fadeInChainIndex { 1 };

    CrossfadeMixer variationCrossfade;

    /** Sized in `prepareToPlay` only — holds duplicate dry input for parallel chain processing. */
    std::vector<float> crossfadeScratchA;
    std::vector<float> crossfadeScratchB;
    int crossfadeScratchCapacity { 0 };

    std::atomic<bool> shutdownFlag { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHostManager)
};

} // namespace forge7
