#pragma once

#include <juce_core/juce_core.h>

namespace forge7
{

/** Root FORGE7 library folder (Projects, Scenes, Presets, etc.). */
juce::File getForgeUserDataDirectory();

juce::File getProjectsDirectory();
juce::File getScenesDirectory();
juce::File getPresetsDirectory();
juce::File getPluginCacheDirectory();
juce::File getBackupsDirectory();

/** Creates Projects, Scenes, Presets, PluginCache, Backups under the library root. */
bool ensureForgeStorageFoldersExist();

/** Safe single-segment name for library files (no path separators). */
juce::String sanitizeLibraryItemName(const juce::String& userName);

// --- File extensions (V1: project only; scene/preset filenames reserved for later.) --------------
inline constexpr const char* kForgeProjectExtension = ".forgeproject";
// TODO: constexpr const char* kForgeSceneExtension = ".forgescene";
// TODO: constexpr const char* kForgePresetExtension = ".forgepreset";

/** Sorted list of .forgeproject files in Projects (may be empty). */
juce::Array<juce::File> listLibraryProjectFiles();

} // namespace forge7
