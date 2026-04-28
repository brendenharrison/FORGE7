#include "SceneManager.h"

#include "../PluginHost/PluginHostManager.h"

#include "ChainVariation.h"
#include "Scene.h"

namespace forge7
{

SceneManager::SceneManager()
{
    scenes.push_back(std::make_unique<Scene>());
    activeSceneIndex = 0;
}

SceneManager::~SceneManager() = default;

void SceneManager::clampActiveSceneIndex() noexcept
{
    if (scenes.empty())
    {
        activeSceneIndex = -1;
        return;
    }

    activeSceneIndex = juce::jlimit(0, static_cast<int>(scenes.size()) - 1, activeSceneIndex);
}

const Scene* SceneManager::getActiveScene() const noexcept
{
    if (scenes.empty())
        return nullptr;

    const int idx =
        juce::jlimit(0, static_cast<int>(scenes.size()) - 1, activeSceneIndex);

    return scenes[static_cast<size_t>(idx)].get();
}

Scene* SceneManager::getActiveScene() noexcept
{
    return const_cast<Scene*>(std::as_const(*this).getActiveScene());
}

int SceneManager::getActiveChainVariationIndex() const noexcept
{
    if (auto* s = getActiveScene())
        return s->getActiveChainVariationIndex();

    return 0;
}

juce::String SceneManager::createScene(const juce::String& sceneName)
{
    auto scene = std::make_unique<Scene>();

    if (sceneName.isNotEmpty())
        scene->setSceneName(sceneName);

    const juce::String id = scene->getSceneId();

    scenes.push_back(std::move(scene));
    activeSceneIndex = static_cast<int>(scenes.size()) - 1;

    return id;
}

bool SceneManager::deleteScene(int sceneIndex)
{
    if (! juce::isPositiveAndBelow(sceneIndex, static_cast<int>(scenes.size())))
        return false;

    if (scenes.size() <= 1)
        return false;

    scenes.erase(scenes.begin() + sceneIndex);

    clampActiveSceneIndex();

    if (auto* s = getActiveScene())
        s->clampActiveVariationIndex();

    return true;
}

bool SceneManager::renameScene(int sceneIndex, const juce::String& newName)
{
    if (! juce::isPositiveAndBelow(sceneIndex, static_cast<int>(scenes.size())))
        return false;

    scenes[static_cast<size_t>(sceneIndex)]->setSceneName(newName);
    return true;
}

juce::String SceneManager::duplicateScene(int sceneIndex)
{
    if (! juce::isPositiveAndBelow(sceneIndex, static_cast<int>(scenes.size())))
        return {};

    const auto& src = *scenes[static_cast<size_t>(sceneIndex)];

    auto dup = src.duplicateWithNewSceneIdentity(src.getSceneName() + " Copy");
    const juce::String newId = dup->getSceneId();

    scenes.push_back(std::move(dup));
    activeSceneIndex = static_cast<int>(scenes.size()) - 1;

    return newId;
}

bool SceneManager::selectScene(int sceneIndex)
{
    if (! juce::isPositiveAndBelow(sceneIndex, static_cast<int>(scenes.size())))
        return false;

    activeSceneIndex = sceneIndex;

    if (auto* s = getActiveScene())
        s->clampActiveVariationIndex();

    return true;
}

void SceneManager::previousScene()
{
    if (scenes.empty())
        return;

    clampActiveSceneIndex();

    if (activeSceneIndex > 0)
        --activeSceneIndex;
    else
        activeSceneIndex = static_cast<int>(scenes.size()) - 1;

    if (auto* s = getActiveScene())
        s->clampActiveVariationIndex();
}

void SceneManager::nextScene()
{
    if (scenes.empty())
        return;

    clampActiveSceneIndex();

    if (activeSceneIndex + 1 < static_cast<int>(scenes.size()))
        ++activeSceneIndex;
    else
        activeSceneIndex = 0;

    if (auto* s = getActiveScene())
        s->clampActiveVariationIndex();
}

juce::String SceneManager::createChainVariation(const juce::String& variationName)
{
    auto* scene = getActiveScene();

    if (scene == nullptr)
        return {};

    auto variation = std::make_unique<ChainVariation>();

    if (variationName.isNotEmpty())
        variation->setVariationName(variationName);

    const juce::String vid = variation->getVariationId();

    scene->getVariations().push_back(std::move(variation));
    scene->setActiveChainVariationIndex(static_cast<int>(scene->getVariations().size()) - 1);

    return vid;
}

bool SceneManager::deleteChainVariation(int variationIndex)
{
    auto* scene = getActiveScene();

    if (scene == nullptr)
        return false;

    auto& vars = scene->getVariations();

    if (! juce::isPositiveAndBelow(variationIndex, static_cast<int>(vars.size())))
        return false;

    if (vars.size() <= 1)
        return false;

    vars.erase(vars.begin() + variationIndex);
    scene->clampActiveVariationIndex();

    return true;
}

bool SceneManager::selectChainVariation(int variationIndex)
{
    auto* scene = getActiveScene();

    if (scene == nullptr)
        return false;

    if (! juce::isPositiveAndBelow(variationIndex, static_cast<int>(scene->getVariations().size())))
        return false;

    scene->setActiveChainVariationIndex(variationIndex);
    return true;
}

void SceneManager::previousChainVariation()
{
    auto* scene = getActiveScene();

    if (scene == nullptr)
        return;

    auto& vars = scene->getVariations();

    if (vars.empty())
        return;

    scene->clampActiveVariationIndex();

    int idx = scene->getActiveChainVariationIndex();

    if (idx > 0)
        --idx;
    else
        idx = static_cast<int>(vars.size()) - 1;

    scene->setActiveChainVariationIndex(idx);
}

void SceneManager::deserializeReset(std::vector<std::unique_ptr<Scene>> replacementScenes, int newActiveSceneIndex)
{
    scenes = std::move(replacementScenes);
    activeSceneIndex = newActiveSceneIndex;

    clampActiveSceneIndex();

    if (auto* sc = getActiveScene())
        sc->clampActiveVariationIndex();
}

void SceneManager::nextChainVariation()
{
    auto* scene = getActiveScene();

    if (scene == nullptr)
        return;

    auto& vars = scene->getVariations();

    if (vars.empty())
        return;

    scene->clampActiveVariationIndex();

    int idx = scene->getActiveChainVariationIndex();

    if (idx + 1 < static_cast<int>(vars.size()))
        ++idx;
    else
        idx = 0;

    scene->setActiveChainVariationIndex(idx);
}

bool SceneManager::selectChainVariationWithCrossfade(int variationIndex, PluginHostManager& host)
{
    if (! selectChainVariation(variationIndex))
        return false;

    return host.commitChainVariationCrossfade(*this);
}

bool SceneManager::nextChainVariationWithCrossfade(PluginHostManager& host)
{
    nextChainVariation();
    return host.commitChainVariationCrossfade(*this);
}

bool SceneManager::previousChainVariationWithCrossfade(PluginHostManager& host)
{
    previousChainVariation();
    return host.commitChainVariationCrossfade(*this);
}

} // namespace forge7
