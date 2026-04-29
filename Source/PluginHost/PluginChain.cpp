#include "PluginChain.h"

#include "../Audio/ChainMeterTaps.h"
#include "PluginSlot.h"
#include "../Utilities/Logger.h"

namespace forge7
{
namespace
{
float peakAbsBlock(const float* x, int numSamples) noexcept
{
    if (x == nullptr || numSamples <= 0)
        return 0.0f;

    float p = 0.0f;

    for (int i = 0; i < numSamples; ++i)
        p = juce::jmax(p, std::abs(x[i]));

    return juce::jlimit(0.0f, 1.0f, p);
}
} // namespace

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

void PluginChain::processMonoBlock(float* monoInOut, int numSamples, bool publishMeters)
{
    // RT: non-blocking shared lock - if exclusive editing holds the mutex, skip FX this block (dry pass-through).
    std::shared_lock<std::shared_mutex> sharedLock(chainMutex, std::try_to_lock);
    if (! sharedLock.owns_lock())
        return;

    if (monoInOut == nullptr || numSamples <= 0)
        return;

    midiScratch.clear();

    for (size_t i = 0; i < slots.size(); ++i)
    {
        auto& slot = slots[i];

        if (slot != nullptr)
            slot->processMonoBlock(monoInOut, numSamples, midiScratch);

        if (publishMeters && meterTaps != nullptr && i < meterTaps->postSlotPeak.size())
            meterTaps->postSlotPeak[i].store(peakAbsBlock(monoInOut, numSamples), std::memory_order_relaxed);
    }
}

void PluginChain::processBlock(float* monoInOut, int numSamples)
{
    processMonoBlock(monoInOut, numSamples, true);
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
    //   assigns via PluginChain::assignPluginToSlot -> PluginSlot::assignHostedPlugin.
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
    Logger::info("FORGE7 PlayablePreset: removePluginFromSlot slot=" + juce::String(slotIndex));
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
    {
        slot->setBypass(shouldBypass);
        Logger::info("FORGE7 PlayablePreset: bypassSlot slot=" + juce::String(slotIndex)
                     + " bypass=" + juce::String(shouldBypass ? "true" : "false"));
    }
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

    Logger::info("FORGE7 PlayablePreset: assignPluginToSlot slot=" + juce::String(slotIndex)
                 + " name=\"" + description.name + "\" format=\"" + description.pluginFormatName + "\"");
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
