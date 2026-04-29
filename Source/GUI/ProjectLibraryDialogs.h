#pragma once

#include <functional>

#include <juce_core/juce_core.h>

namespace juce
{
class Component;
}

namespace forge7
{

struct AppContext;

void runSaveProjectToLibraryDialog(juce::Component* modalParent,
                                   AppContext& appContext,
                                   std::function<void(const juce::String&)> statusMessage = {},
                                   std::function<void()> onSavedSuccessfully = {});

void runLoadProjectFromLibraryBrowser(juce::Component* modalParent,
                                      AppContext& appContext,
                                      std::function<void(const juce::String&)> statusMessage = {});

void runExportProjectWithFileChooser(juce::Component* modalParent, AppContext& appContext);

void runImportProjectWithFileChooser(juce::Component* modalParent, AppContext& appContext);

/** Replace session with a library project file; optional scene/chain indices after load (Jump Browser).

    Handles unsaved-current-project modal when needed. Message thread only. */
void openLibraryProjectFileReplacingCurrent(juce::Component* modalParent,
                                            AppContext& appContext,
                                            const juce::File& projectFile,
                                            int selectSceneIndexAfterLoad = -1,
                                            int selectChainIndexAfterLoad = -1);

} // namespace forge7
