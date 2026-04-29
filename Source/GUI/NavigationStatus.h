#pragma once

#include <juce_core/juce_core.h>

namespace forge7
{

struct AppContext;

/** Centralized read-model for the user-facing **Project > Scene > Chain** hierarchy.

    Computed on the message thread from `SceneManager` + `AppContext::getProjectDisplayName`.
    Internal naming still uses `ChainVariation`; user-facing strings expose **Chain** terminology. */
struct NavigationStatus
{
    juce::String projectName;

    juce::String sceneName;
    int sceneIndex { -1 };
    int sceneCount { 0 };
    juce::String sceneId;

    juce::String chainName;
    int chainIndex { -1 };
    int chainCount { 0 };
    juce::String chainId;

    double tempoBpm { 0.0 };

    /** True when `sceneCount > 0` and a scene is selected. */
    bool hasActiveScene() const noexcept { return sceneIndex >= 0 && sceneCount > 0; }

    /** True when active scene has at least one chain selected. */
    bool hasActiveChain() const noexcept { return chainIndex >= 0 && chainCount > 0; }

    /** Display label for the active chain: "Chain 02" or "Chain 02 - Chorus". */
    juce::String getChainDisplayLabel() const;

    /** Two-digit 1-based index, e.g. "01". */
    static juce::String formatChainIndex1Based(int oneBasedIndex);

    /** "Chain 02 / 04" or "-" when none. */
    juce::String getChainCountSummary() const;

    /** "Scene 02 / 10" or "-" when none. */
    juce::String getSceneCountSummary() const;

    /** "Project: ..." or empty when no project name. */
    juce::String getProjectHeaderLine() const;
};

/** Compute current navigation status from app state (safe with null fields). */
NavigationStatus computeNavigationStatus(const AppContext& appContext);

} // namespace forge7
