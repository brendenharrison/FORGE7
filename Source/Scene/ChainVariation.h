#pragma once

#include <juce_core/juce_core.h>

#include <memory>
#include <vector>

namespace forge7
{

/** Matches live `PluginChain` slot count for snapshot storage (no runtime `PluginChain` pointer). */
constexpr int kChainSnapshotMaxSlots = 8;

/** One slot in a saved chain topology — mirrors what `PluginChain` / `PluginSlot` persist. */
struct ChainSlotSnapshot
{
    juce::String pluginIdentifier;
    juce::String slotDisplayName;
    bool bypass = false;
    bool isEmpty = true;

    /** Subset of `juce::PluginDescription` for recall / scan matching. */
    juce::String descriptiveName;
    juce::String pluginFormatName;
    juce::String manufacturerName;
    juce::String fileOrIdentifier;
    int32_t uniqueId { 0 };

    /** `AudioProcessor::getStateInformation` bytes, stored in project JSON as Base64 text. */
    juce::String pluginStateBase64;

    /** True after load if the plugin could not be instantiated (missing binary, etc.). */
    bool missingPlugin = false;
};

/** Serializable mono chain layout for one variation (not the RT `PluginChain` instance). */
struct ChainSnapshot
{
    std::vector<ChainSlotSnapshot> slots;

    /** Ensures `kChainSnapshotMaxSlots` entries exist (empty slots). */
    void ensureFixedSlotLayout();

    static ChainSnapshot createEmptyFixedLayout();
};

/** One chain variation within a Scene (A/B/C… routing / preset chains).

    Pure domain data — no GUI or `AudioProcessor` pointers. `PluginHostManager` applies this
    snapshot when the variation becomes active (future wiring). */
class ChainVariation
{
public:
    ChainVariation();

    explicit ChainVariation(juce::String variationIdToUse,
                            juce::String variationNameToUse);

    ~ChainVariation() = default;

    const juce::String& getVariationId() const noexcept { return variationId; }
    void setVariationId(juce::String id) { variationId = std::move(id); }

    const juce::String& getVariationName() const noexcept { return variationName; }
    void setVariationName(juce::String newName) { variationName = std::move(newName); }

    ChainSnapshot& getChainSnapshot() noexcept { return chainSnapshot; }
    const ChainSnapshot& getChainSnapshot() const noexcept { return chainSnapshot; }

    /** Future: JSON / ValueTree blob from `ParameterMappingManager`. */
    juce::String& getControlMappingsSerialized() noexcept { return controlMappingsSerialized; }
    const juce::String& getControlMappingsSerialized() const noexcept { return controlMappingsSerialized; }

    juce::String& getNotes() noexcept { return notes; }
    const juce::String& getNotes() const noexcept { return notes; }

    /** Rough relative CPU estimate for UI metering (0..1); not authoritative. */
    float getEstimatedCpuLoad01() const noexcept { return estimatedCpuLoad01; }
    void setEstimatedCpuLoad01(float load) noexcept { estimatedCpuLoad01 = load; }

    /** Sum of hosted plugin latencies in samples — placeholder until PDC graph exists. */
    int getTotalLatencySamples() const noexcept { return totalLatencySamples; }
    void setTotalLatencySamples(int samples) noexcept { totalLatencySamples = samples; }

    /** Deep copy with new UUID for `variationId` (used by duplicate scene). */
    std::unique_ptr<ChainVariation> duplicateWithNewIdentity() const;

private:
    juce::String variationId;
    juce::String variationName;

    ChainSnapshot chainSnapshot;

    juce::String controlMappingsSerialized;
    juce::String notes;

    float estimatedCpuLoad01 { 0.0f };
    int totalLatencySamples { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainVariation)
};

} // namespace forge7
