#pragma once

#include <vector>

#include <juce_core/juce_core.h>

namespace forge7
{

struct ProjectBrowserChainInfo
{
    juce::String chainName;
    int chainIndex { 0 };
};

struct ProjectBrowserSceneInfo
{
    juce::String sceneName;
    int sceneIndex { 0 };
    std::vector<ProjectBrowserChainInfo> chains;
};

/** Lightweight metadata for Jump Browser (JSON only - no plugins). */
struct ProjectBrowserProjectInfo
{
    juce::String projectName;
    juce::File projectFile;

    /** Best-effort match to highlight disk-backed row vs live session. */
    bool isCurrentProject { false };

    /** True when this row represents the in-memory session without a matching library file. */
    bool isLiveSessionPlaceholder { false };

    int activeSceneIndexFromFile { 0 };

    std::vector<ProjectBrowserSceneInfo> scenes;
};

/** Parse project JSON for browser hierarchy only. Does not touch PluginHostManager. */
bool loadProjectBrowserMetadata(const juce::File& file, ProjectBrowserProjectInfo& out);

} // namespace forge7
