#pragma once

#include <juce_core/juce_core.h>

#include <memory>
#include <vector>

#include "ChainVariation.h"

namespace forge7
{

/** One scene in the project: tempo, ordered chain variations, and which variation is armed.

    Holds **no** GUI or audio references — safe for headless tests and serialization. */
class Scene
{
public:
    Scene();

    explicit Scene(juce::String sceneIdToUse, juce::String sceneName);

    ~Scene() = default;

    const juce::String& getSceneId() const noexcept { return sceneId; }
    void setSceneId(juce::String id) { sceneId = std::move(id); }

    /** Display name shown in UI / setlists. */
    const juce::String& getSceneName() const noexcept { return name; }
    void setSceneName(juce::String newName) { name = std::move(newName); }

    /** Kept as `getName` for older call sites (`PerformanceViewComponent`). */
    const juce::String& getName() const noexcept { return name; }
    void setName(juce::String newName) { name = std::move(newName); }

    double getTempoBpm() const noexcept { return tempoBpm; }
    void setTempoBpm(double bpm) noexcept { tempoBpm = juce::jlimit(20.0, 400.0, bpm); }

    int getActiveChainVariationIndex() const noexcept { return activeChainVariationIndex; }
    void setActiveChainVariationIndex(int index) noexcept { activeChainVariationIndex = index; }

    std::vector<std::unique_ptr<ChainVariation>>& getVariations() noexcept { return variations; }
    const std::vector<std::unique_ptr<ChainVariation>>& getVariations() const noexcept { return variations; }

    ChainVariation* findVariationById(const juce::String& variationId) noexcept;
    const ChainVariation* findVariationById(const juce::String& variationId) const noexcept;

    /** Clamp `activeChainVariationIndex` into `[0, variations.size()-1]` or reset if empty. */
    void clampActiveVariationIndex() noexcept;

    /** Duplicate all variations with fresh IDs — used when duplicating a scene. */
    std::unique_ptr<Scene> duplicateWithNewSceneIdentity(const juce::String& duplicatedSceneName) const;

private:
    juce::String sceneId;
    juce::String name;

    double tempoBpm { 120.0 };

    int activeChainVariationIndex { 0 };

    std::vector<std::unique_ptr<ChainVariation>> variations;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Scene)
};

} // namespace forge7
