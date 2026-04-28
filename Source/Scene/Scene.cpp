#include "Scene.h"

namespace forge7
{

Scene::Scene()
    : sceneId(juce::Uuid().toString())
    , name("Untitled Scene")
{
    variations.push_back(std::make_unique<ChainVariation>());
    activeChainVariationIndex = 0;
}

Scene::Scene(juce::String sceneIdToUse, juce::String sceneName)
    : sceneId(std::move(sceneIdToUse))
    , name(std::move(sceneName))
{
    variations.push_back(std::make_unique<ChainVariation>());
    activeChainVariationIndex = 0;
}

ChainVariation* Scene::findVariationById(const juce::String& variationId) noexcept
{
    for (auto& v : variations)
        if (v != nullptr && v->getVariationId() == variationId)
            return v.get();

    return nullptr;
}

const ChainVariation* Scene::findVariationById(const juce::String& variationId) const noexcept
{
    return const_cast<Scene*>(this)->findVariationById(variationId);
}

void Scene::clampActiveVariationIndex() noexcept
{
    if (variations.empty())
    {
        activeChainVariationIndex = 0;
        return;
    }

    activeChainVariationIndex = juce::jlimit(0, static_cast<int>(variations.size()) - 1, activeChainVariationIndex);
}

std::unique_ptr<Scene> Scene::duplicateWithNewSceneIdentity(const juce::String& duplicatedSceneName) const
{
    auto copy = std::make_unique<Scene>();
    copy->sceneId = juce::Uuid().toString();
    copy->name = duplicatedSceneName;
    copy->tempoBpm = tempoBpm;
    copy->activeChainVariationIndex = activeChainVariationIndex;

    copy->variations.clear();

    for (const auto& v : variations)
        if (v != nullptr)
            copy->variations.push_back(v->duplicateWithNewIdentity());

    copy->clampActiveVariationIndex();

    return copy;
}

} // namespace forge7
