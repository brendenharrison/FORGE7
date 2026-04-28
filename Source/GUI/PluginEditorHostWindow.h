#pragma once

#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

namespace juce
{
class AudioPluginInstance;
class AudioProcessorEditor;
}

namespace forge7
{

/** Floating window that owns one `AudioProcessorEditor` for a loaded `AudioPluginInstance`.

    Threading: **create / show / close only on the JUCE message thread.** Destroying the window deletes
    the editor UI only - the plugin instance keeps running in the processing graph, so audio is
    unaffected (same contract as any JUCE host).

    Future (not implemented): intercept editor mouse/attachments here or via a transparent overlay to
    drive `ParameterMappingManager::armLearnTargetForHardware` ("click a knob to map" learn mode).
    That must remain optional and must never block `processBlock`. */
class PluginEditorHostWindow final : public juce::DocumentWindow
{
public:
    /** Returns nullptr if `hasEditor()` is false or `createEditorIfNeeded()` fails - caller shows a warning. */
    static std::unique_ptr<PluginEditorHostWindow> tryCreate(juce::AudioPluginInstance& plugin,
                                                             const juce::String& windowTitle,
                                                             std::function<void()> onWindowFullyClosed);

    ~PluginEditorHostWindow() override;

    void closeButtonPressed() override;

private:
    PluginEditorHostWindow(const juce::String& title,
                           juce::AudioProcessorEditor* editor,
                           std::function<void()> onWindowFullyClosed);

    std::function<void()> onClosed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorHostWindow)
};

} // namespace forge7
