#include "NavigationStatus.h"

#include "../App/AppContext.h"
#include "../Scene/ChainVariation.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneManager.h"

namespace forge7
{

juce::String NavigationStatus::formatChainIndex1Based(const int oneBasedIndex)
{
    if (oneBasedIndex <= 0)
        return "00";

    if (oneBasedIndex < 10)
        return "0" + juce::String(oneBasedIndex);

    return juce::String(oneBasedIndex);
}

juce::String NavigationStatus::getChainDisplayLabel() const
{
    if (! hasActiveChain())
        return "Chain -";

    const juce::String idx = formatChainIndex1Based(chainIndex + 1);
    juce::String out = "Chain " + idx;

    if (chainName.isNotEmpty())
        out += " - " + chainName;

    return out;
}

juce::String NavigationStatus::getChainCountSummary() const
{
    if (! hasActiveChain())
        return "-";

    return "Chain " + formatChainIndex1Based(chainIndex + 1) + " / "
           + formatChainIndex1Based(chainCount);
}

juce::String NavigationStatus::getSceneCountSummary() const
{
    if (sceneCount <= 0 || sceneIndex < 0 || sceneIndex >= sceneCount)
        return "-";

    return "Scene " + formatChainIndex1Based(sceneIndex + 1) + " / "
           + formatChainIndex1Based(sceneCount);
}

juce::String NavigationStatus::getProjectHeaderLine() const
{
    if (projectName.isEmpty())
        return {};

    return "Project: " + projectName;
}

NavigationStatus computeNavigationStatus(const AppContext& appContext)
{
    NavigationStatus s;

    if (appContext.getProjectDisplayName != nullptr)
        s.projectName = appContext.getProjectDisplayName();

    if (appContext.sceneManager == nullptr)
        return s;

    const auto& scenes = appContext.sceneManager->getScenes();
    s.sceneCount = static_cast<int>(scenes.size());

    const int activeIdx = appContext.sceneManager->getActiveSceneIndex();

    if (! juce::isPositiveAndBelow(activeIdx, s.sceneCount))
        return s;

    const auto* sceneRaw = scenes[static_cast<size_t>(activeIdx)].get();

    if (sceneRaw == nullptr)
        return s;

    const auto& scene = *sceneRaw;
    s.sceneIndex = activeIdx;
    s.sceneName = scene.getSceneName();
    s.sceneId = scene.getSceneId();
    s.tempoBpm = scene.getTempoBpm();

    const auto& vars = scene.getVariations();
    s.chainCount = static_cast<int>(vars.size());

    if (vars.empty())
        return s;

    const int rawCi = scene.getActiveChainVariationIndex();
    const int ci = juce::jlimit(0, s.chainCount - 1, rawCi);

    if (! juce::isPositiveAndBelow(ci, s.chainCount))
        return s;

    const auto* chainRaw = vars[static_cast<size_t>(ci)].get();

    if (chainRaw == nullptr)
        return s;

    s.chainIndex = ci;
    s.chainName = chainRaw->getVariationName();
    s.chainId = chainRaw->getVariationId();

    return s;
}

} // namespace forge7
