#include "ChainVariation.h"

namespace forge7
{

void ChainSnapshot::ensureFixedSlotLayout()
{
    slots.resize(static_cast<size_t>(kChainSnapshotMaxSlots));
}

ChainSnapshot ChainSnapshot::createEmptyFixedLayout()
{
    ChainSnapshot snap;
    snap.ensureFixedSlotLayout();
    return snap;
}

ChainVariation::ChainVariation()
    : variationId(juce::Uuid().toString())
    , variationName()
{
    chainSnapshot = ChainSnapshot::createEmptyFixedLayout();
}

ChainVariation::ChainVariation(juce::String variationIdToUse, juce::String variationNameToUse)
    : variationId(std::move(variationIdToUse))
    , variationName(std::move(variationNameToUse))
{
    chainSnapshot = ChainSnapshot::createEmptyFixedLayout();
}

std::unique_ptr<ChainVariation> ChainVariation::duplicateWithNewIdentity() const
{
    auto copy = std::make_unique<ChainVariation>();
    copy->variationId = juce::Uuid().toString();
    copy->variationName = variationName;
    copy->chainSnapshot = chainSnapshot;
    copy->controlMappingsSerialized = controlMappingsSerialized;
    copy->notes = notes;
    copy->estimatedCpuLoad01 = estimatedCpuLoad01;
    copy->totalLatencySamples = totalLatencySamples;
    return copy;
}

} // namespace forge7
