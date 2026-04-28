#include "PluginHostManager.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginChain.h"
#include "PluginSlot.h"

#include "../Scene/ChainVariation.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneManager.h"
#include "../Utilities/Logger.h"

namespace forge7
{
namespace
{
juce::PluginDescription pluginDescriptionFromSnapshot(const ChainSlotSnapshot& snap)
{
    juce::PluginDescription pd;

    pd.name = snap.descriptiveName.isNotEmpty() ? snap.descriptiveName : snap.slotDisplayName;
    pd.descriptiveName = snap.descriptiveName;
    pd.pluginFormatName = snap.pluginFormatName;
    pd.manufacturerName = snap.manufacturerName;
    pd.fileOrIdentifier = snap.fileOrIdentifier;
    pd.uniqueId = snap.uniqueId;

    return pd;
}
} // namespace

PluginHostManager::PluginHostManager()
{
    formatManager.addDefaultFormats();
    chains[0] = std::make_unique<PluginChain>();
    chains[1] = std::make_unique<PluginChain>();
}

PluginHostManager::~PluginHostManager()
{
    shutdownFlag.store(true, std::memory_order_relaxed);
}

PluginChain* PluginHostManager::getPluginChain() noexcept
{
    const int idx = juce::jlimit(0, 1, audibleChainIndex.load(std::memory_order_relaxed));
    return chains[static_cast<size_t>(idx)].get();
}

juce::Array<juce::PluginDescription> PluginHostManager::getAvailablePluginDescriptions() const
{
    const juce::ScopedLock lock(pluginListLock);
    return knownPluginList.getTypes();
}

void PluginHostManager::addPluginScanDirectory(const juce::File& directory)
{
    const juce::ScopedLock lock(scanFoldersLock);

    if (! directory.isDirectory())
        return;

    auto norm = directory.getFullPathName();

    for (auto& existing : userScanDirectories)
        if (existing.getFullPathName().equalsIgnoreCase(norm))
            return;

    userScanDirectories.add(directory);
}

void PluginHostManager::removePluginScanDirectory(const juce::File& directory)
{
    const juce::ScopedLock lock(scanFoldersLock);

    for (int i = userScanDirectories.size(); --i >= 0;)
        if (userScanDirectories.getReference(i) == directory)
            userScanDirectories.remove(i);
}

void PluginHostManager::clearPluginScanDirectories()
{
    const juce::ScopedLock lock(scanFoldersLock);
    userScanDirectories.clear();
}

juce::Array<juce::File> PluginHostManager::getPluginScanDirectories() const
{
    const juce::ScopedLock lock(scanFoldersLock);
    return userScanDirectories;
}

void PluginHostManager::addStandardPlatformScanDirectories()
{
#if JUCE_LINUX || JUCE_MAC || JUCE_WINDOWS
    addPluginScanDirectory(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".vst3"));
#endif

#if JUCE_LINUX
    addPluginScanDirectory(juce::File("/usr/lib/vst3"));
#elif JUCE_MAC
    addPluginScanDirectory(juce::File("/Library/Audio/Plug-Ins/VST3"));
#elif JUCE_WINDOWS
    auto pf = juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory);
    addPluginScanDirectory(pf.getChildFile("Common Files").getChildFile("VST3"));
#endif
}

int PluginHostManager::scanAllPluginsBlocking()
{
    const juce::ScopedLock plist(pluginListLock);
    const juce::ScopedLock folders(scanFoldersLock);

    juce::FileSearchPath searchPaths;

    for (auto& dir : userScanDirectories)
        if (dir.isDirectory())
            searchPaths.add(dir);

    const size_t countBefore = knownPluginList.getTypes().size();

    for (auto* format : formatManager.getFormats())
    {
        if (format == nullptr)
            continue;

        juce::PluginDirectoryScanner scanner(knownPluginList,
                                             *format,
                                             searchPaths,
                                             true,
                                             juce::File(),
                                             false);

        juce::String nameBeingScanned;
        while (scanner.scanNextFile(true, nameBeingScanned))
        {
            juce::ignoreUnused(nameBeingScanned);

            if (shutdownFlag.load(std::memory_order_relaxed))
                break;
        }
    }

    const size_t countAfter = knownPluginList.getTypes().size();

    Logger::info("FORGE7: plugin scan finished — descriptions: " + juce::String(static_cast<int>(countAfter))
                 + ", added this pass: " + juce::String(static_cast<int>(countAfter - countBefore)));

    return static_cast<int>(countAfter - countBefore);
}

void PluginHostManager::scanAllPluginsAsync(std::function<void(int numDescriptionsAdded)> onFinished)
{
    auto callback = std::move(onFinished);

    juce::Thread::launch([this, cb = std::move(callback)]() mutable
                         {
                             const int added = scanAllPluginsBlocking();

                             if (cb != nullptr && juce::MessageManager::getInstanceWithoutCreating() != nullptr)
                                 juce::MessageManager::callAsync([cb = std::move(cb), added]() mutable
                                                                 {
                                                                     cb(added);
                                                                 });
                         });
}

bool PluginHostManager::saveKnownPluginsToFile(const juce::File& xmlFile) const
{
    const juce::ScopedLock lock(pluginListLock);

    const std::unique_ptr<juce::XmlElement> xml(knownPluginList.createXml());

    if (xml == nullptr || xmlFile.getFullPathName().isEmpty())
        return false;

    return xml->writeTo(xmlFile);
}

bool PluginHostManager::loadKnownPluginsFromFile(const juce::File& xmlFile)
{
    const juce::ScopedLock lock(pluginListLock);

    if (! xmlFile.existsAsFile())
        return false;

    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(xmlFile.loadFileAsString()));

    if (xml == nullptr)
        return false;

    knownPluginList.recreateFromXml(*xml);

    Logger::info("FORGE7: restored known plugin list (" + juce::String(knownPluginList.getTypes().size())
                 + " entries) from " + xmlFile.getFullPathName());

    return true;
}

bool PluginHostManager::assignInstanceToSlotMessageThread(std::unique_ptr<juce::AudioPluginInstance> instance,
                                                         const juce::PluginDescription& description,
                                                         int slotIndex,
                                                         juce::String& errorOut)
{
    double sr {};
    int bs {};

    {
        const juce::ScopedLock layoutLock(audioLayoutLock);
        sr = lastSampleRate;
        bs = lastBlockSize;
    }

    if (slotIndex < 0 || slotIndex >= kPluginChainMaxSlots)
    {
        errorOut = "Slot index out of range";
        return false;
    }

    auto* chain = getPluginChain();

    jassert(chain != nullptr);

    if (! chain->assignPluginToSlot(slotIndex, std::move(instance), description, sr, bs))
    {
        errorOut = "Could not assign plugin instance to chain slot";
        return false;
    }

    return true;
}

void PluginHostManager::loadPluginIntoSlotAsync(const juce::PluginDescription& description,
                                               int slotIndex,
                                                PluginLoadCompletion onComplete)
{
    const juce::PluginDescription descCopy(description);
    auto completion = std::move(onComplete);

    juce::Thread::launch([this, descCopy, slotIndex, completion = std::move(completion)]() mutable
                         {
                             juce::String instantiationError;

                             double sr {};
                             int bs {};

                             {
                                 const juce::ScopedLock layoutLock(audioLayoutLock);
                                 sr = lastSampleRate;
                                 bs = lastBlockSize;
                             }

                             std::unique_ptr<juce::AudioPluginInstance> instance(formatManager.createPluginInstance(
                                 descCopy,
                                 sr,
                                 bs,
                                 instantiationError));

                             if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
                             {
                                 auto instanceHolder =
                                     std::make_shared<std::unique_ptr<juce::AudioPluginInstance>>(std::move(instance));

                                 juce::MessageManager::callAsync([this,
                                                                  descCopy,
                                                                  slotIndex,
                                                                  instanceHolder,
                                                                  instantiationError,
                                                                  completion = std::move(completion)]() mutable
                                                                 {
                                                                     auto instanceLocal = std::move(*instanceHolder);

                                                                     if (instanceLocal == nullptr)
                                                                     {
                                                                         Logger::error("FORGE7: plugin instantiation failed — "
                                                                                       + instantiationError);

                                                                         if (completion != nullptr)
                                                                             completion(false,
                                                                                        instantiationError.isNotEmpty()
                                                                                            ? instantiationError
                                                                                            : juce::String(
                                                                                                  "Instantiation failed"));

                                                                         return;
                                                                     }

                                                                     juce::String assignErr;

                                                                     if (! assignInstanceToSlotMessageThread(std::move(instanceLocal),
                                                                                                            descCopy,
                                                                                                            slotIndex,
                                                                                                            assignErr))
                                                                     {
                                                                         Logger::error("FORGE7: assign to slot failed — "
                                                                                       + assignErr);

                                                                         if (completion != nullptr)
                                                                             completion(false, assignErr);

                                                                         return;
                                                                     }

                                                                     if (completion != nullptr)
                                                                         completion(true, {});
                                                                 });
                             }
                             else
                             {
                                 if (completion != nullptr)
                                     completion(false, "No message manager");
                             }
                         });
}

bool PluginHostManager::loadPluginIntoSlotSynchronously(const juce::PluginDescription& description,
                                                       int slotIndex,
                                                       juce::String& errorMessageOut)
{
    double sr {};
    int bs {};

    {
        const juce::ScopedLock layoutLock(audioLayoutLock);
        sr = lastSampleRate;
        bs = lastBlockSize;
    }

    juce::String instantiationError;

    auto instance = formatManager.createPluginInstance(description,
                                                       sr,
                                                       bs,
                                                       instantiationError);

    if (instance == nullptr)
    {
        errorMessageOut = instantiationError.isNotEmpty() ? instantiationError : juce::String("Instantiation failed");

        Logger::warn("FORGE7: synchronous plugin load failed — " + errorMessageOut);

        return false;
    }

    return assignInstanceToSlotMessageThread(std::move(instance), description, slotIndex, errorMessageOut);
}

void PluginHostManager::hydratePluginChainFromChainVariation(PluginChain& chain, const ChainVariation& variation)
{
    const double sr = getLastKnownSampleRate();
    const int bs = getLastKnownBlockSize();

    const_cast<ChainVariation&>(variation).getChainSnapshot().ensureFixedSlotLayout();

    for (int i = 0; i < kChainSnapshotMaxSlots; ++i)
    {
        auto* slot = chain.getSlot(static_cast<size_t>(i));

        if (slot == nullptr)
            continue;

        slot->clearSlotContent();

        const ChainSlotSnapshot& snap = variation.getChainSnapshot().slots[static_cast<size_t>(i)];

        if (snap.isEmpty || snap.fileOrIdentifier.isEmpty())
            continue;

        juce::PluginDescription pd = pluginDescriptionFromSnapshot(snap);

        juce::MemoryBlock preset;

        if (snap.pluginStateBase64.isNotEmpty())
            preset.fromBase64Encoding(snap.pluginStateBase64);

        juce::String err;

        auto instance = formatManager.createPluginInstance(pd, sr, bs, err);

        if (instance != nullptr)
        {
            if (! preset.isEmpty())
                instance->setStateInformation(preset.getData(), static_cast<int>(preset.getSize()));

            if (! chain.assignPluginToSlot(i, std::move(instance), pd, sr, bs))
                Logger::warn("FORGE7: hydrate — could not assign plugin to slot " + juce::String(i));
        }
        else
        {
            Logger::warn("FORGE7: hydrate — missing plugin (slot " + juce::String(i) + "): " + err);

            slot->loadPlaceholderPlugin(pd.createIdentifierString(),
                                        pd.name.isNotEmpty() ? pd.name : pd.descriptiveName);

            slot->setMissingPluginState(true);

            if (! preset.isEmpty())
                slot->getPluginStatePlaceholder() = preset;
        }
    }
}

bool PluginHostManager::commitChainVariationCrossfade(SceneManager& scenes)
{
    if (variationCrossfade.isCrossfading())
        return false;

    auto* scene = scenes.getActiveScene();

    if (scene == nullptr)
        return false;

    scene->clampActiveVariationIndex();

    const int vidx = scene->getActiveChainVariationIndex();
    auto& vars = scene->getVariations();

    if (! juce::isPositiveAndBelow(vidx, static_cast<int>(vars.size())))
        return false;

    auto* variation = vars[static_cast<size_t>(vidx)].get();

    if (variation == nullptr)
        return false;

    const int aud = juce::jlimit(0, 1, audibleChainIndex.load(std::memory_order_relaxed));
    const int idle = 1 - aud;

    auto& idleChain = chains[static_cast<size_t>(idle)];

    if (idleChain == nullptr)
        return false;

    hydratePluginChainFromChainVariation(*idleChain, *variation);

    fadeOutChainIndex.store(aud, std::memory_order_release);
    fadeInChainIndex.store(idle, std::memory_order_release);
    variationCrossfade.beginCrossfade();

    return true;
}

void PluginHostManager::resetVariationRoutingAfterProjectLoad() noexcept
{
    variationCrossfade.abort();
    audibleChainIndex.store(0, std::memory_order_relaxed);
    fadeOutChainIndex.store(0, std::memory_order_relaxed);
    fadeInChainIndex.store(1, std::memory_order_relaxed);

    if (chains[1] != nullptr)
        chains[1]->clearChain();
}

void PluginHostManager::setVariationCrossfadeTimeMs(double milliseconds) noexcept
{
    variationCrossfade.setCrossfadeTimeMs(milliseconds);
}

double PluginHostManager::getVariationCrossfadeTimeMs() const noexcept
{
    return variationCrossfade.getCrossfadeTimeMs();
}

bool PluginHostManager::isVariationCrossfadeActive() const noexcept
{
    return variationCrossfade.isCrossfading();
}

void PluginHostManager::processMonoBlock(float* monoInOut, int numSamples)
{
    // RT: **no heap allocation** — scratch vectors are sized only in `prepareToPlay`.
    if (monoInOut == nullptr || numSamples <= 0)
        return;

    if (variationCrossfade.isCrossfading())
    {
        if (numSamples > crossfadeScratchCapacity || crossfadeScratchA.size() < static_cast<size_t>(numSamples))
        {
#if JUCE_DEBUG
            jassertfalse;
#endif
            return;
        }

        const int outIdx = juce::jlimit(0, 1, fadeOutChainIndex.load(std::memory_order_acquire));
        const int inIdx = juce::jlimit(0, 1, fadeInChainIndex.load(std::memory_order_acquire));

        float* bufA = crossfadeScratchA.data();
        float* bufB = crossfadeScratchB.data();

        juce::FloatVectorOperations::copy(bufA, monoInOut, numSamples);
        juce::FloatVectorOperations::copy(bufB, monoInOut, numSamples);

        if (chains[static_cast<size_t>(outIdx)] != nullptr)
            chains[static_cast<size_t>(outIdx)]->processMonoBlock(bufA, numSamples);

        if (chains[static_cast<size_t>(inIdx)] != nullptr)
            chains[static_cast<size_t>(inIdx)]->processMonoBlock(bufB, numSamples);

        const bool completed = variationCrossfade.processCrossfadeBlock(bufA, bufB, monoInOut, numSamples);

        if (completed)
            audibleChainIndex.store(juce::jlimit(0, 1, fadeInChainIndex.load(std::memory_order_relaxed)),
                                    std::memory_order_relaxed);

        return;
    }

    const int aud = juce::jlimit(0, 1, audibleChainIndex.load(std::memory_order_relaxed));

    if (chains[static_cast<size_t>(aud)] != nullptr)
        chains[static_cast<size_t>(aud)]->processMonoBlock(monoInOut, numSamples);
}

void PluginHostManager::prepareToPlay(double sampleRate, int blockSize)
{
    {
        const juce::ScopedLock layoutLock(audioLayoutLock);
        lastSampleRate = sampleRate;
        lastBlockSize = blockSize;
    }

    for (auto& c : chains)
        if (c != nullptr)
            c->prepareToPlay(sampleRate, blockSize);

    variationCrossfade.prepare(sampleRate, blockSize);

    const int cap = juce::jmax(1, blockSize);
    crossfadeScratchA.assign(static_cast<size_t>(cap), 0.0f);
    crossfadeScratchB.assign(static_cast<size_t>(cap), 0.0f);
    crossfadeScratchCapacity = blockSize;
}

double PluginHostManager::getLastKnownSampleRate() const noexcept
{
    const juce::ScopedLock layoutLock(audioLayoutLock);
    return lastSampleRate;
}

int PluginHostManager::getLastKnownBlockSize() const noexcept
{
    const juce::ScopedLock layoutLock(audioLayoutLock);
    return lastBlockSize;
}

void PluginHostManager::releaseResources()
{
    for (auto& c : chains)
        if (c != nullptr)
            c->releaseResources();
}

} // namespace forge7
