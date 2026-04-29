/*
 * Project JSON migrations (future):
 * ---------------------------------------------------------------------------
 * - Increment `projectFileVersion` whenever the on-disk schema changes incompatibly.
 * - In `loadProjectFromFile`, branch on `projectFileVersion` and either:
 *   (a) parse legacy shapes into the current in-memory model, or
 *   (b) run small upgrade steps (v1->v2->v3) so older files still open.
 * - Prefer additive keys for minor changes so older app versions can skip unknown fields.
 * - Never remove old parser branches until you no longer support those shipped releases.
 */

#include "ProjectSerializer.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Controls/ParameterMappingManager.h"
#include "../PluginHost/PluginChain.h"
#include "../PluginHost/PluginHostManager.h"
#include "../PluginHost/PluginSlot.h"
#include "../Scene/ChainVariation.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneManager.h"
#include "../Audio/AudioEngine.h"
#include "../Utilities/Logger.h"

namespace forge7
{
namespace
{

constexpr int kCurrentProjectFileVersion = 2;
constexpr int kMinSupportedProjectFileVersion = 1;

juce::var slotSnapshotToVar(const ChainSlotSnapshot& s)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("pluginIdentifier", s.pluginIdentifier);
    o->setProperty("slotDisplayName", s.slotDisplayName);
    o->setProperty("bypass", s.bypass);
    o->setProperty("isEmpty", s.isEmpty);
    o->setProperty("descriptiveName", s.descriptiveName);
    o->setProperty("pluginFormatName", s.pluginFormatName);
    o->setProperty("manufacturerName", s.manufacturerName);
    o->setProperty("fileOrIdentifier", s.fileOrIdentifier);
    o->setProperty("uniqueId", static_cast<int>(s.uniqueId));
    o->setProperty("pluginStateBase64", s.pluginStateBase64);
    o->setProperty("missingPlugin", s.missingPlugin);
    return juce::var(o);
}

bool slotSnapshotFromVar(const juce::var& v, ChainSlotSnapshot& out)
{
    auto* o = v.getDynamicObject();
    if (o == nullptr)
        return false;

    out.pluginIdentifier = o->getProperty("pluginIdentifier").toString();
    out.slotDisplayName = o->getProperty("slotDisplayName").toString();
    out.bypass = static_cast<bool>(o->getProperty("bypass"));
    out.isEmpty = static_cast<bool>(o->getProperty("isEmpty"));
    out.descriptiveName = o->getProperty("descriptiveName").toString();
    out.pluginFormatName = o->getProperty("pluginFormatName").toString();
    out.manufacturerName = o->getProperty("manufacturerName").toString();
    out.fileOrIdentifier = o->getProperty("fileOrIdentifier").toString();
    out.uniqueId = static_cast<int32_t>(static_cast<int>(o->getProperty("uniqueId")));
    out.pluginStateBase64 = o->getProperty("pluginStateBase64").toString();
    out.missingPlugin = static_cast<bool>(o->getProperty("missingPlugin"));
    return true;
}

juce::var chainSnapshotToVar(const ChainSnapshot& cs)
{
    juce::Array<juce::var> slotVars;

    for (const auto& slot : cs.slots)
        slotVars.add(slotSnapshotToVar(slot));

    auto* o = new juce::DynamicObject();
    o->setProperty("slots", juce::var(slotVars));
    return juce::var(o);
}

void chainSnapshotFromVar(const juce::var& v, ChainSnapshot& out)
{
    out.slots.clear();

    auto* o = v.getDynamicObject();
    if (o == nullptr)
    {
        out.ensureFixedSlotLayout();
        return;
    }

    const juce::var slotsVar(o->getProperty("slots"));

    if (slotsVar.isArray())
    {
        const auto* arr = slotsVar.getArray();
        if (arr != nullptr)
        {
            for (const auto& item : *arr)
            {
                ChainSlotSnapshot slot {};
                if (slotSnapshotFromVar(item, slot))
                    out.slots.push_back(slot);
            }
        }
    }

    out.ensureFixedSlotLayout();
}

juce::var chainVariationToVar(const ChainVariation& cv)
{
    /** v2: write `chainId` / `chainName` (user-facing terminology). Legacy v1 used `variationId` / `variationName`. */
    auto* o = new juce::DynamicObject();
    o->setProperty("chainId", cv.getVariationId());
    o->setProperty("chainName", cv.getVariationName());
    o->setProperty("chainSnapshot", chainSnapshotToVar(cv.getChainSnapshot()));
    o->setProperty("controlMappings", cv.getControlMappingsSerialized());
    o->setProperty("notes", cv.getNotes());
    o->setProperty("estimatedCpuLoad01", static_cast<double>(cv.getEstimatedCpuLoad01()));
    o->setProperty("totalLatencySamples", cv.getTotalLatencySamples());
    return juce::var(o);
}

bool chainVariationFromVar(const juce::var& v, std::unique_ptr<ChainVariation>& outCv)
{
    auto* o = v.getDynamicObject();
    if (o == nullptr)
        return false;

    /** v2 keys preferred; v1 keys (`variationId` / `variationName`) accepted for backward compatibility. */
    juce::String vid = o->getProperty("chainId").toString();
    if (vid.isEmpty())
        vid = o->getProperty("variationId").toString();

    juce::String vname = o->getProperty("chainName").toString();
    if (vname.isEmpty())
        vname = o->getProperty("variationName").toString();

    if (vid.isEmpty())
        return false;

    auto cv = std::make_unique<ChainVariation>(vid, vname);

    chainSnapshotFromVar(o->getProperty("chainSnapshot"), cv->getChainSnapshot());

    {
        juce::String cm = o->getProperty("controlMappings").toString();

        if (cm.isEmpty())
            cm = o->getProperty("parameterMappings").toString(); // v1 rename; keep when adding v2 migrations

        cv->getControlMappingsSerialized() = cm;
    }
    cv->getNotes() = o->getProperty("notes").toString();
    cv->setEstimatedCpuLoad01(static_cast<float>(static_cast<double>(o->getProperty("estimatedCpuLoad01"))));
    cv->setTotalLatencySamples(static_cast<int>(static_cast<int>(o->getProperty("totalLatencySamples"))));

    outCv = std::move(cv);
    return true;
}

juce::var sceneToVar(const Scene& scene)
{
    juce::Array<juce::var> vars;

    for (const auto& cv : scene.getVariations())
        if (cv != nullptr)
            vars.add(chainVariationToVar(*cv));

    /** v2: write `chains` and `activeChainIndex` (user-facing). Legacy keys not written. */
    auto* o = new juce::DynamicObject();
    o->setProperty("sceneId", scene.getSceneId());
    o->setProperty("sceneName", scene.getSceneName());
    o->setProperty("tempoBpm", scene.getTempoBpm());
    o->setProperty("activeChainIndex", scene.getActiveChainVariationIndex());
    o->setProperty("chains", juce::var(vars));

    return juce::var(o);
}

bool sceneFromVar(const juce::var& v, std::unique_ptr<Scene>& outScene)
{
    auto* o = v.getDynamicObject();
    if (o == nullptr)
        return false;

    const juce::String sid = o->getProperty("sceneId").toString();
    const juce::String sname = o->getProperty("sceneName").toString();

    if (sid.isEmpty())
        return false;

    auto scene = sname.isEmpty() ? std::make_unique<Scene>(sid, juce::String("Scene"))
                                 : std::make_unique<Scene>(sid, sname);

    scene->setTempoBpm(static_cast<double>(o->getProperty("tempoBpm")));

    /** v2 active index key preferred; fall back to v1 `activeChainVariationIndex`. */
    int activeIdx = 0;
    if (o->hasProperty("activeChainIndex"))
        activeIdx = static_cast<int>(o->getProperty("activeChainIndex"));
    else if (o->hasProperty("activeChainVariationIndex"))
        activeIdx = static_cast<int>(o->getProperty("activeChainVariationIndex"));
    scene->setActiveChainVariationIndex(activeIdx);

    scene->getVariations().clear();

    /** v2: `chains` array; v1 fallback: `chainVariations`. */
    juce::var chainsArr(o->getProperty("chains"));
    if (! chainsArr.isArray())
        chainsArr = o->getProperty("chainVariations");

    if (chainsArr.isArray())
    {
        const auto* arr = chainsArr.getArray();
        if (arr != nullptr)
        {
            for (const auto& item : *arr)
            {
                std::unique_ptr<ChainVariation> cv;

                if (chainVariationFromVar(item, cv) && cv != nullptr)
                    scene->getVariations().push_back(std::move(cv));
            }
        }
    }

    if (scene->getVariations().empty())
        scene->getVariations().push_back(std::make_unique<ChainVariation>());

    scene->clampActiveVariationIndex();

    outScene = std::move(scene);
    return true;
}

} // namespace

ProjectSerializer::ProjectSerializer(SceneManager& scenes,
                                     ParameterMappingManager& mappings,
                                     juce::String& projectNameStorage)
    : sceneManager(scenes)
    , parameterMappingManager(mappings)
    , projectName(projectNameStorage)
{
}

ProjectSerializer::~ProjectSerializer() = default;

void ProjectSerializer::captureActiveChainFromLiveHost(PluginHostManager& host)
{
    syncActiveVariationFromLiveHost(host);
}

void ProjectSerializer::hydrateActiveChainIntoHost(PluginHostManager& host)
{
    hydrateActiveVariationIntoHost(host);
}

void ProjectSerializer::syncActiveVariationFromLiveHost(PluginHostManager& host)
{
    auto* scene = sceneManager.getActiveScene();

    if (scene == nullptr)
        return;

    scene->clampActiveVariationIndex();

    const int vidx = scene->getActiveChainVariationIndex();

    if (! juce::isPositiveAndBelow(vidx, static_cast<int>(scene->getVariations().size())))
        return;

    auto* variation = scene->getVariations()[static_cast<size_t>(vidx)].get();

    if (variation == nullptr)
        return;

    auto* chain = host.getPluginChain();

    if (chain == nullptr)
        return;

    variation->getChainSnapshot().ensureFixedSlotLayout();

    for (int i = 0; i < kChainSnapshotMaxSlots; ++i)
    {
        auto* slot = chain->getSlot(static_cast<size_t>(i));

        if (slot == nullptr)
            continue;

        ChainSlotSnapshot snap {};
        slot->populateSnapshotForProjectSave(snap);

        variation->getChainSnapshot().slots[static_cast<size_t>(i)] = snap;
    }
}

void ProjectSerializer::hydrateActiveVariationIntoHost(PluginHostManager& host)
{
    auto* scene = sceneManager.getActiveScene();

    if (scene == nullptr)
        return;

    scene->clampActiveVariationIndex();

    const int vidx = scene->getActiveChainVariationIndex();

    if (! juce::isPositiveAndBelow(vidx, static_cast<int>(scene->getVariations().size())))
        return;

    auto* variation = scene->getVariations()[static_cast<size_t>(vidx)].get();

    if (variation == nullptr)
        return;

    auto* chain = host.getPluginChain();

    if (chain == nullptr)
        return;

    host.hydratePluginChainFromChainVariation(*chain, *variation);
}

juce::Result ProjectSerializer::saveProjectToFile(const juce::File& file,
                                                  PluginHostManager* captureLiveFromHost,
                                                  AudioEngine* audioEngine)
{
    if (captureLiveFromHost != nullptr)
        syncActiveVariationFromLiveHost(*captureLiveFromHost);

    auto* root = new juce::DynamicObject();

    root->setProperty("projectFileVersion", kCurrentProjectFileVersion);
    root->setProperty("projectName", projectName);

    const int activeSceneIndex = sceneManager.getActiveSceneIndex();

    root->setProperty("activeSceneIndex", activeSceneIndex);

    if (audioEngine != nullptr)
        root->setProperty("globalBypassEffects", audioEngine->isGlobalBypass());

    root->setProperty("globalParameterMappings", parameterMappingManager.exportMappingsToVar());

    juce::Array<juce::var> sceneVars;

    for (const auto& scene : sceneManager.getScenes())
        if (scene != nullptr)
            sceneVars.add(sceneToVar(*scene));

    root->setProperty("scenes", juce::var(sceneVars));

    const juce::String jsonText(juce::JSON::toString(juce::var(root), true));

    if (jsonText.isEmpty())
        return juce::Result::fail("FORGE7: failed to encode project JSON");

    if (! file.replaceWithText(jsonText))
        return juce::Result::fail("FORGE7: could not write project file");

    Logger::info("FORGE7: saved project - " + file.getFullPathName());

    return juce::Result::ok();
}

juce::Result ProjectSerializer::loadProjectFromFile(const juce::File& file,
                                                    PluginHostManager* hydrateIntoHost,
                                                    AudioEngine* audioEngine)
{
    if (! file.existsAsFile())
        return juce::Result::fail("FORGE7: project file does not exist");

    juce::String jsonText;

    try
    {
        jsonText = file.loadFileAsString();
    }
    catch (...)
    {
        return juce::Result::fail("FORGE7: could not read project file");
    }

    juce::var parsed;

    const auto parseResult = juce::JSON::parse(jsonText, parsed);

    if (parseResult.failed())
        return juce::Result::fail("FORGE7: invalid JSON - " + parseResult.getErrorMessage());

    auto* root = parsed.getDynamicObject();

    if (root == nullptr)
        return juce::Result::fail("FORGE7: root must be a JSON object");

    const int version = static_cast<int>(root->getProperty("projectFileVersion"));

    if (version < kMinSupportedProjectFileVersion || version > kCurrentProjectFileVersion)
        return juce::Result::fail("FORGE7: unsupported projectFileVersion (got "
                                  + juce::String(version) + ", supported "
                                  + juce::String(kMinSupportedProjectFileVersion) + "-"
                                  + juce::String(kCurrentProjectFileVersion)
                                  + ").");

    if (version < kCurrentProjectFileVersion)
        Logger::info("FORGE7: loading legacy projectFileVersion=" + juce::String(version)
                     + ", will save as version " + juce::String(kCurrentProjectFileVersion));

    projectName = root->getProperty("projectName").toString();

    const int activeSceneIndexValue = static_cast<int>(root->getProperty("activeSceneIndex"));

    parameterMappingManager.importMappingsFromVar(root->getProperty("globalParameterMappings"));

    std::vector<std::unique_ptr<Scene>> loadedScenes;

    const juce::var scenesVar(root->getProperty("scenes"));

    if (! scenesVar.isArray())
        return juce::Result::fail("FORGE7: \"scenes\" must be an array");

    const auto* sceneArr = scenesVar.getArray();

    if (sceneArr == nullptr || sceneArr->isEmpty())
        return juce::Result::fail("FORGE7: project contains no scenes");

    for (const auto& sv : *sceneArr)
    {
        std::unique_ptr<Scene> scene;

        if (sceneFromVar(sv, scene) && scene != nullptr)
            loadedScenes.push_back(std::move(scene));
    }

    if (loadedScenes.empty())
        return juce::Result::fail("FORGE7: could not parse any scenes");

    sceneManager.deserializeReset(std::move(loadedScenes), activeSceneIndexValue);

    if (hydrateIntoHost != nullptr)
    {
        hydrateIntoHost->resetVariationRoutingAfterProjectLoad();
        hydrateActiveVariationIntoHost(*hydrateIntoHost);
    }

    if (audioEngine != nullptr && root->hasProperty("globalBypassEffects"))
        audioEngine->setGlobalBypass(static_cast<bool>(root->getProperty("globalBypassEffects")));

    Logger::info("FORGE7: loaded project - " + file.getFullPathName());

    return juce::Result::ok();
}

} // namespace forge7
