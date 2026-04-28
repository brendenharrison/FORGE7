#pragma once

#include <juce_core/juce_core.h>

namespace forge7
{

class SceneManager;
class ParameterMappingManager;
class PluginHostManager;

/** Persists FORGE 7 projects as UTF-8 JSON (`juce::JSON`, `juce::var`, `juce::DynamicObject`).

    Threading: **message thread only** — never call from `audioDeviceIOCallback`.

    Versioning:
    - `projectFileVersion` field gates parsing. **When bumping for breaking changes**, add a new
      branch in the loader (e.g. `case 2:`) and optionally implement one-shot migrators from older
      versions so users can open legacy files (e.g. map renamed keys, default new fields).
    - Prefer additive JSON keys when possible so older loaders can still ignore unknown fields.
    - Keep `kCurrentProjectFileVersion` in sync with the writer.

    Save flow (optional live capture): if `captureLiveFromHost` is non-null, the **active scene’s
    active chain variation** is copied from `PluginHostManager`’s live `PluginChain` into the domain
    model before serializing (processor state via `getStateInformation` → Base64).

    Load flow (optional hydrate): if `hydrateIntoHost` is non-null, the **active scene’s active
    variation** is instantiated into the live chain; missing plugins become placeholder + missing
    flag without failing the whole project. */
class ProjectSerializer
{
public:
    ProjectSerializer(SceneManager& scenes,
                       ParameterMappingManager& mappings,
                       juce::String& projectNameStorage);

    ~ProjectSerializer();

    /** When `captureLiveFromHost` is set, snapshots the live rack for the active variation first. */
    juce::Result saveProjectToFile(const juce::File& file, PluginHostManager* captureLiveFromHost = nullptr);

    /** When `hydrateIntoHost` is set, loads plugins for the active variation (best-effort). */
    juce::Result loadProjectFromFile(const juce::File& file, PluginHostManager* hydrateIntoHost = nullptr);

private:
    SceneManager& sceneManager;
    ParameterMappingManager& parameterMappingManager;
    juce::String& projectName;

    void syncActiveVariationFromLiveHost(PluginHostManager& host);
    void hydrateActiveVariationIntoHost(PluginHostManager& host);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectSerializer)
};

} // namespace forge7
