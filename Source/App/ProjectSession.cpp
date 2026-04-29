#include "ProjectSession.h"

#include "../PluginHost/PluginHostManager.h"
#include "../Scene/SceneManager.h"
#include "../Storage/ProjectSerializer.h"
#include "../Utilities/Logger.h"

namespace forge7
{

ProjectSession::ProjectSession(SceneManager& scenes, PluginHostManager& host, ProjectSerializer& serializer)
    : sceneManager(scenes)
    , pluginHostManager(host)
    , projectSerializer(serializer)
{
}

void ProjectSession::captureLiveChainIntoModel()
{
    projectSerializer.captureActiveChainFromLiveHost(pluginHostManager);
}

void ProjectSession::hydrateActiveChainIntoHost()
{
    projectSerializer.hydrateActiveChainIntoHost(pluginHostManager);
}

void ProjectSession::pushActiveChainToLiveHost()
{
    commitCrossfadeAfterModelNavigation();
    markProjectDirty();
}

void ProjectSession::notifyChanged()
{
    if (onChanged != nullptr)
        onChanged();
}

bool ProjectSession::commitCrossfadeAfterModelNavigation()
{
    const bool ok = pluginHostManager.commitChainVariationCrossfade(sceneManager);

    if (! ok)
        Logger::warn("FORGE7: commitChainVariationCrossfade declined (crossfade active or invalid state)");

    return ok;
}

void ProjectSession::nextChain()
{
    captureLiveChainIntoModel();
    sceneManager.nextChainVariation();
    commitCrossfadeAfterModelNavigation();
    markProjectDirty();
}

void ProjectSession::previousChain()
{
    captureLiveChainIntoModel();
    sceneManager.previousChainVariation();
    commitCrossfadeAfterModelNavigation();
    markProjectDirty();
}

bool ProjectSession::switchToChain(int chainIndex)
{
    captureLiveChainIntoModel();

    if (! sceneManager.selectChainVariation(chainIndex))
        return false;

    commitCrossfadeAfterModelNavigation();
    markProjectDirty();
    return true;
}

void ProjectSession::nextScene()
{
    captureLiveChainIntoModel();
    sceneManager.nextScene();

    Logger::info(
        "FORGE7 V2 TODO: preload active scene chains for seamless performance switching.");

    commitCrossfadeAfterModelNavigation();
    markProjectDirty();
}

void ProjectSession::previousScene()
{
    captureLiveChainIntoModel();
    sceneManager.previousScene();

    Logger::info(
        "FORGE7 V2 TODO: preload active scene chains for seamless performance switching.");

    commitCrossfadeAfterModelNavigation();
    markProjectDirty();
}

bool ProjectSession::switchToScene(int sceneIndex)
{
    captureLiveChainIntoModel();

    if (! sceneManager.selectScene(sceneIndex))
        return false;

    Logger::info(
        "FORGE7 V2 TODO: preload active scene chains for seamless performance switching.");

    commitCrossfadeAfterModelNavigation();
    markProjectDirty();
    return true;
}

void ProjectSession::markProjectDirty()
{
    projectDirty.store(true, std::memory_order_release);
    notifyChanged();
}

void ProjectSession::clearProjectDirtyAfterSave()
{
    projectDirty.store(false, std::memory_order_release);
    notifyChanged();
}

} // namespace forge7
