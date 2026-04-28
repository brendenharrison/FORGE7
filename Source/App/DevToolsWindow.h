#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

namespace forge7
{

struct AppContext;

/** Floating development window: simulated hardware controls. Not used on embedded Linux builds.

    TODO: optional collapsible dock inside main window when multi-window is undesirable. */
class DevToolsWindow final : public juce::DocumentWindow
{
public:
    explicit DevToolsWindow(AppContext& context);
    ~DevToolsWindow() override;

    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DevToolsWindow)
};

} // namespace forge7
