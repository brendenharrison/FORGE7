#pragma once

#include <atomic>
#include <functional>

namespace forge7
{

class SceneManager;
class PluginHostManager;
class ProjectSerializer;

/** Coordinates project model vs live plugin host: capture before navigation, hydrate after.

    All methods are message-thread only. */
class ProjectSession
{
public:
    ProjectSession(SceneManager& scenes, PluginHostManager& host, ProjectSerializer& serializer);

    void setOnProjectStateChanged(std::function<void()> cb) { onChanged = std::move(cb); }

    void captureLiveChainIntoModel();
    void hydrateActiveChainIntoHost();

    /** After model-only edits (new chain/scene, etc.): crossfade idle rack from active variation + mark dirty. */
    void pushActiveChainToLiveHost();

    bool switchToChain(int chainIndex);
    void nextChain();
    void previousChain();

    bool switchToScene(int sceneIndex);
    void nextScene();
    void previousScene();

    void markProjectDirty();
    void clearProjectDirtyAfterSave();
    bool isProjectDirty() const noexcept { return projectDirty.load(std::memory_order_acquire); }

private:
    void notifyChanged();
    bool commitCrossfadeAfterModelNavigation();

    SceneManager& sceneManager;
    PluginHostManager& pluginHostManager;
    ProjectSerializer& projectSerializer;

    std::function<void()> onChanged;
    std::atomic<bool> projectDirty { false };
};

} // namespace forge7
