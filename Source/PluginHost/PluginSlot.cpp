#include "PluginSlot.h"

#include "../Scene/ChainVariation.h"
#include "../Utilities/Logger.h"

namespace forge7
{

PluginSlot::PluginSlot() = default;

PluginSlot::~PluginSlot()
{
    releaseResources();
}

void PluginSlot::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
{
    rtProcessorPtr.store(nullptr, std::memory_order_release);

    if (hostedInstance != nullptr)
    {
        Logger::info("FORGE7 PlayablePreset: slot prepareToPlay name=\"" + hostedInstance->getName()
                     + "\" sr=" + juce::String(sampleRate, 1) + " block=" + juce::String(maximumExpectedSamplesPerBlock));
        hostedInstance->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
    }

    syncRtProcessorPointer();
}

void PluginSlot::releaseResources()
{
    rtProcessorPtr.store(nullptr, std::memory_order_release);

    if (hostedInstance != nullptr)
        hostedInstance->releaseResources();
}

void PluginSlot::syncRtProcessorPointer() noexcept
{
    rtProcessorPtr.store(hostedInstance != nullptr ? static_cast<juce::AudioProcessor*>(hostedInstance.get())
                                                   : nullptr,
                         std::memory_order_release);
}

void PluginSlot::processMonoBlock(float* monoInOut,
                                  int numSamples,
                                  juce::MidiBuffer& midiScratch) noexcept
{
    if (monoInOut == nullptr || numSamples <= 0)
        return;

    if (bypassAtomic.load(std::memory_order_relaxed))
        return;

    auto* proc = rtProcessorPtr.load(std::memory_order_acquire);
    if (proc == nullptr)
        return;

    float* channelPointers[1] = { monoInOut };
    juce::AudioBuffer<float> audioBuffer(channelPointers, 1, numSamples);
    midiScratch.clear();
    processBlockCallCount.fetch_add(1, std::memory_order_relaxed);
    proc->processBlock(audioBuffer, midiScratch);
}

void PluginSlot::syncMetadataFromDescription(const juce::PluginDescription& desc)
{
    cachedPluginDescription = desc;
    slotName = desc.name.isNotEmpty() ? desc.name : desc.descriptiveName;
    pluginIdentifier = desc.createIdentifierString();
}

void PluginSlot::loadPlaceholderPlugin(const juce::String& pluginUid, const juce::String& displayName)
{
    cachedPluginDescription = {};
    cachedPluginDescription.fileOrIdentifier = pluginUid;
    cachedPluginDescription.uniqueId = static_cast<int32_t>(pluginUid.hashCode());
    cachedPluginDescription.name = displayName.isEmpty() ? pluginUid : displayName;
    cachedPluginDescription.descriptiveName = cachedPluginDescription.name;
    cachedPluginDescription.pluginFormatName = "Placeholder";
    cachedPluginDescription.manufacturerName = {};

    pluginIdentifier = pluginUid;
    slotName = displayName.isEmpty() ? pluginUid : displayName;
}

void PluginSlot::clearSlotContent()
{
    missingPluginRuntime = false;

    releaseResources();
    hostedInstance.reset();
    cachedPluginDescription = {};
    slotName.clear();
    pluginIdentifier.clear();
    pluginStatePlaceholder.reset();
}

void PluginSlot::assignHostedPlugin(std::unique_ptr<juce::AudioPluginInstance> instance,
                                    const juce::PluginDescription& description,
                                    double sampleRate,
                                    int maximumExpectedSamplesPerBlock)
{
    missingPluginRuntime = false;

    releaseResources();
    hostedInstance = std::move(instance);
    syncMetadataFromDescription(description);
    processBlockCallCount.store(0, std::memory_order_relaxed);

    if (hostedInstance != nullptr)
    {
        Logger::info("FORGE7 PlayablePreset: slot assignHostedPlugin name=\"" + hostedInstance->getName()
                     + "\" format=\"" + description.pluginFormatName
                     + "\" fileOrIdentifier=\"" + description.fileOrIdentifier + "\"");
        // Future: optional editor attachment (not enabled in FORGE V1).
        hostedInstance->enableAllBuses();

        // Mono guitar path: prefer mono bus layout when the processor reports flexible I/O.
        // If layout negotiation fails, `prepareToPlay` may still succeed with default buses.
        const juce::AudioProcessor::BusesLayout monoLayout = []()
        {
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add(juce::AudioChannelSet::mono());
            layout.outputBuses.add(juce::AudioChannelSet::mono());
            return layout;
        }();

        if (! hostedInstance->setBusesLayout(monoLayout))
        {
            // Some VST3 effects only expose stereo - leave enabled default layout from enableAllBuses().
            juce::ignoreUnused(monoLayout);
        }

        hostedInstance->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
        Logger::info("FORGE7 PlayablePreset: slot assigned and prepared name=\"" + hostedInstance->getName()
                     + "\" sr=" + juce::String(sampleRate, 1) + " block=" + juce::String(maximumExpectedSamplesPerBlock));
    }

    syncRtProcessorPointer();
}

bool PluginSlot::isEmptySlot() const noexcept
{
    return hostedInstance == nullptr && pluginIdentifier.isEmpty();
}

bool PluginSlot::isPlaceholderOnly() const noexcept
{
    return pluginIdentifier.isNotEmpty() && hostedInstance == nullptr;
}

void PluginSlot::populateSnapshotForProjectSave(ChainSlotSnapshot& snap) const
{
    snap.bypass = isBypassedForAudio();
    snap.isEmpty = isEmptySlot();
    snap.slotDisplayName = slotName;
    snap.pluginIdentifier = pluginIdentifier;

    const auto& d = cachedPluginDescription;

    snap.descriptiveName = d.descriptiveName.isNotEmpty() ? d.descriptiveName : d.name;
    snap.pluginFormatName = d.pluginFormatName;
    snap.manufacturerName = d.manufacturerName;
    snap.fileOrIdentifier = d.fileOrIdentifier;
    snap.uniqueId = d.uniqueId;

    if (snap.pluginIdentifier.isEmpty() && d.createIdentifierString().isNotEmpty())
        snap.pluginIdentifier = d.createIdentifierString();

    snap.pluginStateBase64.clear();
    snap.missingPlugin = missingPluginRuntime;

    if (hostedInstance != nullptr)
    {
        juce::MemoryBlock mb;

        hostedInstance->getStateInformation(mb);

        if (! mb.isEmpty())
            snap.pluginStateBase64 = mb.toBase64Encoding();
    }
    else if (pluginStatePlaceholder.getSize() > 0)
        snap.pluginStateBase64 = pluginStatePlaceholder.toBase64Encoding();
}

} // namespace forge7
