#include "DevToolsWindow.h"

#include "AppContext.h"
#include "../GUI/SimulatedControlsComponent.h"
#include "../Utilities/DebugSessionLog.h"

namespace forge7
{
namespace
{
constexpr int kDefaultW = 420;
constexpr int kDefaultH = 820;
} // namespace

DevToolsWindow::DevToolsWindow(AppContext& context)
    : DocumentWindow("FORGE 7 - Simulated Hardware",
                     juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                     DocumentWindow::closeButton,
                     false)
{
    setUsingNativeTitleBar(true);
    setResizable(true, true);
    setContentOwned(new SimulatedControlsComponent(context), true);
    setSize(kDefaultW, kDefaultH);
    /** Stay above the main FORGE window so startup `toFront` calls do not hide this panel. */
    setAlwaysOnTop(true);
    setVisible(true);

    // #region agent log
    {
        auto* st = new juce::DynamicObject();
        st->setProperty("w", getWidth());
        st->setProperty("h", getHeight());
        st->setProperty("visible", isVisible());
        st->setProperty("bounds", getBounds().toString());
        debugAgentLog("H4", "DevToolsWindow::ctor", "ctor_end", juce::var(st));
    }
    // #endregion
}

DevToolsWindow::~DevToolsWindow() = default;

void DevToolsWindow::closeButtonPressed()
{
    setVisible(false);
}

} // namespace forge7
