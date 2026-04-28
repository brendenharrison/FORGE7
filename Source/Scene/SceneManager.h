#pragma once

#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

namespace forge7
{

class Scene;
class PluginHostManager;

/** Owns the project's **Scenes** -> **ChainVariations** tree (V1: no Setlists/Songs).

    Pure model + indices - **no GUI**. After changing the active variation index, call
    `PluginHostManager::commitChainVariationCrossfade` (or the `*WithCrossfade` helpers below) so
    the dual-rack host can crossfade without blocking the audio thread.

    Hardware mapping: **chain prev/next** -> `nextChainVariationWithCrossfade()` etc. */
class SceneManager
{
public:
    SceneManager();
    ~SceneManager();

    const std::vector<std::unique_ptr<Scene>>& getScenes() const noexcept { return scenes; }
    std::vector<std::unique_ptr<Scene>>& getScenes() noexcept { return scenes; }

    int getActiveSceneIndex() const noexcept { return activeSceneIndex; }

    Scene* getActiveScene() noexcept;
    const Scene* getActiveScene() const noexcept;

    /** Current scene's selected variation index; `0` if no active scene. */
    int getActiveChainVariationIndex() const noexcept;

    // --- Scenes ---------------------------------------------------------------------------

    /** Creates a scene with optional name; becomes the active scene. Returns new `sceneId`. */
    juce::String createScene(const juce::String& sceneName = {});

    /** Keeps at least one scene. */
    bool deleteScene(int sceneIndex);

    bool renameScene(int sceneIndex, const juce::String& newName);

    /** Returns new scene's `sceneId`. */
    juce::String duplicateScene(int sceneIndex);

    bool selectScene(int sceneIndex);

    void previousScene();
    void nextScene();

    // --- Chain variations (active scene) -------------------------------------------------

    /** Adds a variation; optional name; selects the new variation. Returns `variationId`. */
    juce::String createChainVariation(const juce::String& variationName = {});

    /** Keeps at least one variation per scene. */
    bool deleteChainVariation(int variationIndex);

    bool selectChainVariation(int variationIndex);

    /** Dedicated hardware: step variation within the active scene (wraps). */
    void previousChainVariation();

    /** Dedicated hardware: step variation within the active scene (wraps). */
    void nextChainVariation();

    /** Message thread: updates index then hydrates the idle rack + arms crossfade (see `PluginHostManager`). */
    bool selectChainVariationWithCrossfade(int variationIndex, PluginHostManager& host);

    bool nextChainVariationWithCrossfade(PluginHostManager& host);
    bool previousChainVariationWithCrossfade(PluginHostManager& host);

    /** Replace all scenes from deserialized project data (message thread only). */
    void deserializeReset(std::vector<std::unique_ptr<Scene>> replacementScenes, int newActiveSceneIndex);

private:
    void clampActiveSceneIndex() noexcept;

    std::vector<std::unique_ptr<Scene>> scenes;
    int activeSceneIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SceneManager)
};

} // namespace forge7
