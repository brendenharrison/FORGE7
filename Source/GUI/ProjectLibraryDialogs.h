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
                                   std::function<void(const juce::String&)> statusMessage = {});

void runLoadProjectFromLibraryBrowser(juce::Component* modalParent,
                                      AppContext& appContext,
                                      std::function<void(const juce::String&)> statusMessage = {});

void runExportProjectWithFileChooser(juce::Component* modalParent, AppContext& appContext);

void runImportProjectWithFileChooser(juce::Component* modalParent, AppContext& appContext);

} // namespace forge7
