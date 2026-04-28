#include "PluginChain.h"

#include "PluginSlot.h"

namespace forge7
{

PluginChain::PluginChain()
{
    for (auto& slot : slots)
        slot = std::make_unique<PluginSlot>();
}

PluginChain::~PluginChain() = default;

bool PluginChain::isValidSlotIndex(int slotIndex) const noexcept
{
    return slotIndex >= 0 && slotIndex < kPluginChainMaxSlots;
}

PluginSlot* PluginChain::getSlot(size_t slotIndex) noexcept
{
    return slotIndex < slots.size() ? slots[slotIndex].get() : nullptr;
}

const PluginSlot* PluginChain::getSlot(size_t slotIndex) const noexcept
{
    return slotIndex < slots.size() ? slots[slotIndex].get() : nullptr;
}

void PluginChain::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    midiScratch.clear();

    for (auto& slot : slots)
        if (slot != nullptr)
            slot->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
}

void PluginChain::releaseResources()
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    midiScratch.clear();

    for (auto& slot : slots)
        if (slot != nullptr)
            slot->releaseResources();
}

void PluginChain::processMonoBlock(float* monoInOut, int numSamples)
{
    // RT: non-blocking shared lock — if exclusive editing holds the mutex, skip FX this block (dry pass-through).
    std::shared_lock<std::shared_mutex> sharedLock(chainMutex, std::try_to_lock);
    if (! sharedLock.owns_lock())
        return;

    if (monoInOut == nullptr || numSamples <= 0)
        return;

    midiScratch.clear();

    for (auto& slot : slots)
    {
        if (slot == nullptr)
            continue;

        slot->processMonoBlock(monoInOut, numSamples, midiScratch);

        // Future VST3: latency reporting / PDC compensation between slots.
    }
}

void PluginChain::processBlock(float* monoInOut, int numSamples)
{
    processMonoBlock(monoInOut, numSamples);
}

bool PluginChain::addPluginToSlot(int slotIndex,
                                  const juce::String& pluginIdentifier,
                                  const juce::String& slotDisplayName)
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    if (! isValidSlotIndex(slotIndex))
        return false;

    auto* slot = slots[static_cast<size_t>(slotIndex)].get();
    jassert(slot != nullptr);

    // Future VST3 / async loader: PluginHostManager.loadPluginIntoSlotAsync(...)
    //   assigns via PluginChain::assignPluginToSlot → PluginSlot::assignHostedPlugin.
    slot->loadPlaceholderPlugin(pluginIdentifier, slotDisplayName);
    return true;
}

bool PluginChain::removePluginFromSlot(int slotIndex)
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    if (! isValidSlotIndex(slotIndex))
        return false;

    auto* slot = slots[static_cast<size_t>(slotIndex)].get();
    jassert(slot != nullptr);

    // Future VST3: unload editor if open; delete Editor; async free library if ref-count hits zero.
    slot->clearSlotContent();
    return true;
}

bool PluginChain::moveSlot(int fromSlotIndex, int toSlotIndex)
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    if (! isValidSlotIndex(fromSlotIndex) || ! isValidSlotIndex(toSlotIndex))
        return false;

    if (fromSlotIndex == toSlotIndex)
        return true;

    std::swap(slots[static_cast<size_t>(fromSlotIndex)],
              slots[static_cast<size_t>(toSlotIndex)]);

    // Future VST3: If instances carry editor handles or bus maps, refresh those or call prepareToPlay on chain.
    return true;
}

void PluginChain::bypassSlot(int slotIndex, bool shouldBypass)
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    if (! isValidSlotIndex(slotIndex))
        return;

    if (auto* slot = slots[static_cast<size_t>(slotIndex)].get())
        slot->setBypass(shouldBypass);
}

void PluginChain::clearChain()
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    for (auto& slot : slots)
        if (slot != nullptr)
            slot->clearSlotContent();
}

bool PluginChain::assignPluginToSlot(int slotIndex,
                                     std::unique_ptr<juce::AudioPluginInstance> instance,
                                     const juce::PluginDescription& description,
                                     double sampleRate,
                                     int maximumSamplesPerBlock)
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    if (! isValidSlotIndex(slotIndex))
        return false;

    auto* slot = slots[static_cast<size_t>(slotIndex)].get();
    if (slot == nullptr || instance == nullptr)
        return false;

    slot->assignHostedPlugin(std::move(instance), description, sampleRate, maximumSamplesPerBlock);
    return true;
}

SlotInfo PluginChain::getSlotInfo(int slotIndex) const
{
    std::shared_lock<std::shared_mutex> lock(chainMutex);

    SlotInfo info;
    info.slotIndex = slotIndex;

    if (! isValidSlotIndex(slotIndex))
        return info;

    auto* slot = slots[static_cast<size_t>(slotIndex)].get();
    if (slot == nullptr)
        return info;

    info.slotDisplayName = slot->getSlotName();
    info.pluginIdentifier = slot->getPluginIdentifier();
    info.bypass = slot->isBypassedForAudio();
    info.isEmpty = slot->isEmptySlot();
    info.isPlaceholder = slot->isPlaceholderOnly();
    info.missingPlugin = slot->hasMissingPlugin();

    return info;
}

} // namespace forge7
